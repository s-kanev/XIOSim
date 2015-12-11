// Harness utility that launches XIOSIM for a multiprogrammed workload. Muliple
// pintool processes for a workload are forked according to a configuration
// file. The harness creates a shared memory segment for XIOSIM to use as
// interprocess communication between the separate Pin processes.
//
// To use:
//   harness -benchmark_cfg <CONFIG_FILE> setarch i686 ... pin -pin_args ...
//
// The benchmarK_cfg flag is required.
//
// Author: Sam Xi
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ezOptionParser_clean.hpp"

#include "multiprocess_shared.h"
#include "ipc_queues.h"

boost::interprocess::managed_shared_memory* global_shm;
SHARED_VAR_DEFINE(XIOSIM_LOCK, printing_lock)
SHARED_VAR_DEFINE(int, num_processes)
SHARED_VAR_DEFINE(int, next_asid)
SHARED_VAR_DEFINE(time_t, feeder_watchdogs)

#include "confuse.h"  // For parsing config files.

// Until libconfuse switches to const char*
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

namespace xiosim {
namespace shared {

const std::string CFG_FILE_FLAG = "-benchmark_cfg";
const std::string HARNESS_PID_FLAG = "-harness_pid";
const std::string TIMING_SIM_FLAG = "-timing_sim";
const char* LAST_PINTOOL_ARG = "-s";
const char* FIRST_PIN_ARG = "-pin";

std::vector<pid_t> harness_pids;
int harness_num_processes = 0;

}  // namespace shared
}  // namespace xiosim

void remove_shared_memory() {
    using namespace boost::interprocess;
    using namespace xiosim::shared;

    std::stringstream harness_pid_stream;
    harness_pid_stream << getpid();
    std::string shared_memory_key =
        harness_pid_stream.str() + std::string(XIOSIM_SHARED_MEMORY_KEY);
    // Shared locks are actually named with the prefix "sem.".
    std::string shared_lock_key =
        std::string("sem.") + harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);

    if (!shared_memory_object::remove(shared_memory_key.c_str())) {
        std::cerr << "Warning: Could not remove shared memory object " << shared_memory_key
                  << std::endl;
    }
    if (!shared_memory_object::remove(shared_lock_key.c_str())) {
        std::cerr << "Warning: Could not remove shared memory object " << shared_lock_key
                  << std::endl;
    }
}

// Kills all child processes that were forked by the harness, including the
// timing simulator process. This sends the SIGKILL signal rather than SIGTERM
// in order to ensure process termination.
// This is called when the harness intercepts a SIGINT interrupt or when the
// harness detects that one of the child processes exited unnaturally.
void kill_children(int sig) {
    using namespace xiosim::shared;
    // Using a stringstream to avoid races on std::cout.
    std::stringstream output;
    if (WEXITSTATUS(sig) == SIGINT)
        output << "Caught SIGINT, ";
    else
        output << "Detected signal " << sig << ", ";
    output << "killing child processes." << std::endl;
    std::cout << output.str();
    for (int i = 0; i < harness_num_processes - 1; i++) {
        pid_t pgid = getpgid(harness_pids[i]);
        killpg(pgid, SIGKILL);
    }

    // Kill timing_sim too, but let it execute the cleanup handlers
    pid_t timing_pgid = getpgid(harness_pids[harness_num_processes - 1]);
    killpg(timing_pgid, SIGTERM);

    remove_shared_memory();
    exit(1);
}

// Modify the generic pintool command for the timing simulator.
std::string get_timing_sim_args(std::string harness_args, std::string timing_filename) {
    size_t feeder_so_pos = harness_args.find("feeder_zesto.so");
    size_t feeder_so_end_pos = feeder_so_pos + strlen("feeder_zesto.so");

    // Remove everything before and including the "-t" feeder option
    harness_args.erase(0, feeder_so_end_pos);

    // Prefix with invoking timing_sim binary
    harness_args = timing_filename + " " + harness_args;

    std::cout << "timing_sim args: " << harness_args << std::endl;
    return harness_args;
}

// Fork the timing simulator process and return its pid.
pid_t fork_timing_simulator(std::string run_str, std::string timing_filename, bool debug_timing) {
    std::string timing_cmd = get_timing_sim_args(run_str, timing_filename);
    pid_t timing_sim_pid;

    if (!debug_timing) {
        timing_sim_pid = fork();
        switch (timing_sim_pid) {
        case 0: {  // child
            setpgid(0, 0);
            int ret = system(timing_cmd.c_str());
            if (WIFSIGNALED(ret))
                abort();
            exit(WEXITSTATUS(ret));
        }
        case 1: {
            perror("Fork failed.");
            abort();
        }
        default: {  // parent
            std::cout << "Timing simulator: " << timing_sim_pid << std::endl;
        }
        }
    } else {
        std::cout << "Enter timing_sim PID:";
        std::cout.flush();
        std::cin >> timing_sim_pid;
    }
    return timing_sim_pid;
}

static void wait_for_feeders(int num_feeders, bool no_waitpid) {
    std::cout << "[HARNESS] Waiting for feeder children to finish." << std::endl;

    if (no_waitpid) {
        // Ghetto waitpid replacement.
        // Each feeder routinely updates a location in shm and we periodicially
        // check on it. If we don't hear from the feeder for a while, we assume it
        // has terminated (cleanly or not).
        // We do this because in some execution modes (pin -pid <process>)
        // the feeder process is actually not a direct child of the
        // harness (and can't enter its process group), so we don't get SIGCHLD signals.
        const int WATCHDOG_PERIOD = 30; // sec
        while (1) {
            time_t curr_time = time(nullptr);
            int feeders_done = 0;
            for (int asid = 0; asid < num_feeders; asid++) {
                time_t last_seen = feeder_watchdogs[asid];
                if (std::difftime(curr_time, last_seen) > WATCHDOG_PERIOD)
                    feeders_done++;
            }

            if (feeders_done == num_feeders)
                return;

            sleep(WATCHDOG_PERIOD);
        }
    } else {
        // Waits for the children process to finish. If any of the children, including
        // the timing simulator process, terminate unexpectedly, then ALL children are
        // killed immediately. Otherwise, it waits for only the producer children to
        // finish normally.
        int status;
        for (int i = 0; i < num_feeders; i++) {
            // Wait for any child process to finish.
            pid_t terminated_process = wait(&status);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                std::cerr << "Process " << terminated_process << " failed." << std::endl;
                kill_children(status);
            }
        }
    }
}

int main(int argc, const char* argv[]) {
    using namespace boost::interprocess;
    using namespace xiosim::shared;

    ez::ezOptionParser cmd_opts;
    cmd_opts.overview = "XIOSim harness options";
    cmd_opts.syntax = "-benchmark_cfg CFG_FILE [-debug_timing]";
    cmd_opts.add("timing_sim", 1, 1, 0, "Path to timing_sim binary", TIMING_SIM_FLAG.c_str());
    cmd_opts.add("benchmarks.cfg", 1, 1, 0, "Programs to simulate", CFG_FILE_FLAG.c_str());
    cmd_opts.add("", 0, 0, 0, "Debug timing_sim (start manually)", "-debug_timing");
    cmd_opts.parse(argc, argv);

    std::string cfg_filename;
    cmd_opts.get(CFG_FILE_FLAG.c_str())->getString(cfg_filename);
    bool debug_timing = cmd_opts.get("-debug_timing")->isSet;
    std::string timing_filename;
    cmd_opts.get(TIMING_SIM_FLAG.c_str())->getString(timing_filename);

    // Parse the benchmark configuration file.
    cfg_opt_t program_opts[]{ CFG_STR("run_path", ".", CFGF_NONE),
                              CFG_STR("exe", "", CFGF_NONE),
                              CFG_STR("args", "", CFGF_NONE),
                              CFG_INT("instances", 1, CFGF_NONE),
                              CFG_INT("pid", -1, CFGF_NONE),
                              CFG_END() };
    cfg_opt_t opts[]{ CFG_SEC("program", program_opts, CFGF_MULTI), CFG_END() };
    cfg_t* cfg = cfg_init(opts, 0);
    cfg_parse(cfg, cfg_filename.c_str());

    bool has_pid_attach = false;
    // Compute the total number of benchmark processes that will be forked.
    int num_programs = cfg_size(cfg, "program");
    assert(num_programs > 0);
    for (int i = 0; i < num_programs; i++) {
        cfg_t* program_cfg = cfg_getnsec(cfg, "program", i);
        int instances = cfg_getint(program_cfg, "instances");
        harness_num_processes += instances;

        int pid = cfg_getint(program_cfg, "pid");
        if (pid != -1) {
            has_pid_attach = true;
            if (instances != 1)  {
                std::cerr << "Program instances != 1 when attaching to pid!" << std::endl;
                abort();
            }
        }
    }
    harness_num_processes++;  // For the timing simulator.

    // Setup SIGINT handler to kill child processes as well.
    struct sigaction sig_int_handler;
    sig_int_handler.sa_handler = kill_children;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;
    sigaction(SIGINT, &sig_int_handler, NULL);

    // Concatenate the flags into a single string to be executed when setup is
    // complete.
    std::stringstream command_stream;
    pid_t harness_pid = getpid();
    bool inserted_harness = false;
    bool found_pin = false;
    for (int i = 0; i < argc; i++) {
        // Look for "-pin" to signal end of harness args
        if ((strncmp(argv[i], FIRST_PIN_ARG, strlen(FIRST_PIN_ARG)) == 0) &&
            (strnlen(argv[i], strlen(FIRST_PIN_ARG) + 1) == strlen(FIRST_PIN_ARG))) {
            found_pin = true;
            continue;
        }

        // Bypass harness-specific arguments and start with pin
        if (!found_pin)
            continue;

        // If the next arg is "-s", add the harness_pid flag.
        if ((strncmp(argv[i], LAST_PINTOOL_ARG, strlen(LAST_PINTOOL_ARG)) == 0) &&
            (strnlen(argv[i], strlen(LAST_PINTOOL_ARG) + 1) == strlen(LAST_PINTOOL_ARG))) {
            command_stream << " " << HARNESS_PID_FLAG << " " << harness_pid << " ";
            inserted_harness = true;
        }

        // Pass down arg to children
        command_stream << argv[i] << " ";
    }
    if (!found_pin || !inserted_harness) {
        std::cerr << "Failed to find -pin or -s args!" << std::endl;
        abort();
    }

    std::cerr << "HARNESS CMD: " << command_stream.str() << std::endl;

    // Shared object keys are prefixed by the harness pid that created them.
    std::stringstream harness_pid_stream;
    harness_pid_stream << harness_pid;
    std::string shared_memory_key =
        harness_pid_stream.str() + std::string(XIOSIM_SHARED_MEMORY_KEY);
    std::string init_counter_key = harness_pid_stream.str() + std::string(XIOSIM_INIT_COUNTER_KEY);
    std::string shared_lock_key = harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);

    // Creates a new shared segment.
    global_shm = new managed_shared_memory(
        open_or_create, shared_memory_key.c_str(), DEFAULT_SHARED_MEMORY_SIZE);

    // Sets up a counter for multiprogramming. It is initialized to the number of
    // processes that are to be forked. When each forked process is about to start
    // the pin-instructed program, it will decrement this counter atomically. When
    // the counter reaches zero, the process is allowed to continue execution.
    // This also initializes a shared mutex for the forked processes to use for
    // synchronizing access to this counter.
    permissions perm;
    perm.set_unrestricted();
    int* counter =
        global_shm->find_or_construct<int>(init_counter_key.c_str())(harness_num_processes);
    (void)counter;
    named_mutex init_lock(open_or_create, shared_lock_key.c_str(), perm);

    SHARED_VAR_INIT(XIOSIM_LOCK, printing_lock);
    SHARED_VAR_INIT(int, num_processes, harness_num_processes - 1);
    SHARED_VAR_INIT(int, next_asid, 0);
    time_t initial_time = time(nullptr);
    SHARED_VAR_ARRAY_INIT(time_t, feeder_watchdogs, harness_num_processes-1, initial_time);
    InitIPCQueues();
    init_lock.unlock();

    // Track the pids of all children.
    harness_pids.reserve(harness_num_processes);

    // Create a process for timing simulator and store its pid.
    harness_pids[harness_num_processes - 1] =
        fork_timing_simulator(command_stream.str(), timing_filename, debug_timing);

    // Fork all the benchmark child processes.
    int nthprocess = 0;
    for (int program = 0; program < num_programs; program++) {
        cfg_t* program_cfg = cfg_getnsec(cfg, "program", program);
        int instances = cfg_getint(program_cfg, "instances");

        for (int process = 0; process < instances; process++) {
            harness_pids[nthprocess] = fork();
            std::string run_str = command_stream.str();

            int pid = cfg_getint(program_cfg, "pid");
            if (pid == -1) {
                // Append program command line arguments.
                std::stringstream ss;
                ss << "-- " << cfg_getstr(program_cfg, "exe") << " " << cfg_getstr(program_cfg, "args");
                run_str += ss.str();
            } else {
                // Add pin -pid flag before tool arguments.
                size_t feeder_opt_pos = run_str.find("-t ");
                std::stringstream ss;
                ss << "-pid " << pid << " ";
                run_str.insert(feeder_opt_pos, ss.str());
            }

            switch (harness_pids[nthprocess]) {
            case 0: {  // child
                setpgid(0, 0);
                int chret = chdir(cfg_getstr(program_cfg, "run_path"));
                if (chret != 0) {
                    std::cerr << "chdir failed with " << strerror(errno) << std::endl;
                    abort();
                }
                std::cerr << "[HARNESS] Feeder cmd: " << run_str << std::endl;
                int ret = system(run_str.c_str());
                if (WIFSIGNALED(ret))
                    abort();
                exit(WEXITSTATUS(ret));
                break;
            }
            case -1: {
                perror("Fork failed.");
                break;
            }
            default: {  // parent
                if (pid != -1)
                    harness_pids[nthprocess] = pid;
                std::cout << "[HARNESS] New producer: " << harness_pids[nthprocess] << std::endl;
                nthprocess++;
                break;
            }
            }
        }
    }

    wait_for_feeders(harness_num_processes - 1, has_pid_attach);

    std::cout << "[HARNESS] Letting timing_sim finish" << std::endl;
    /* Tell timing simulator to die quietly */
    ipc_message_t msg;
    msg.StopSimulation(true);
    SendIPCMessage(msg);

    pid_t timing_pid = harness_pids[harness_num_processes - 1];
    pid_t wait_res;
    int status;
    do {
        wait_res = waitpid(timing_pid, &status, 0);
        if (wait_res == -1) {
            std::cerr << "[HARNESS] waitpid(" << timing_pid << ") failed with: " << strerror(errno)
                      << std::endl;
            break;
        }
    } while (wait_res != timing_pid);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << "Process " << timing_pid << " failed." << std::endl;
    }

    remove_shared_memory();
    std::cout << "[HARNESS] Parent exiting." << std::endl;
    return 0;
}

#pragma GCC diagnostic pop
