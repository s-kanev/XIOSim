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

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/permissions.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>

#include "pin.H"

#include "shared_map.h"
#include "shared_unordered_map.h"

#include "../interface.h"
#include "multiprocess_shared.h"
#include "ipc_queues.h"

boost::interprocess::managed_shared_memory *global_shm;
SHARED_VAR_DEFINE(XIOSIM_LOCK, printing_lock);

#include "confuse.h"  // For parsing config files.

namespace xiosim {
namespace shared {

const std::string CFG_FILE_FLAG = "-benchmark_cfg";
const std::string HARNESS_PID_FLAG = "-harness_pid";
const std::string LAST_PINTOOL_ARG = "-s";

pid_t *harness_pids;
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
  std::string init_counter_key = std::string("sem.") +
      harness_pid_stream.str() + std::string(XIOSIM_INIT_COUNTER_KEY);
  std::string shared_lock_key = std::string("sem.") +
      harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);

  if (!shared_memory_object::remove(shared_memory_key.c_str())) {
    std::cerr << "Warning: Could not remove shared memory object "
              << shared_memory_key << std::endl;
  }
  if (!shared_memory_object::remove(init_counter_key.c_str())) {
    std::cerr << "Warning: Could not remove shared memory object "
              << init_counter_key << std::endl;
  }
  if (!shared_memory_object::remove(shared_lock_key.c_str())) {
    std::cerr << "Warning: Could not remove shared memory object "
              << shared_lock_key << std::endl;
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
  stringstream output;
  if (WEXITSTATUS(sig) == SIGINT)
    output << "Caught SIGINT, ";
  else
    output << "Detected signal " << sig << ", ";
  output << "killing child processes." << std::endl;
  std::cout << output.str();
  for (int i = 0; i < harness_num_processes; i++) {
    pid_t pgid = getpgid(harness_pids[i]);
    killpg(pgid, SIGKILL);
  }
  remove_shared_memory();
  exit(1);
}

// Modify the generic pintool command for the timing simulator.
std::string get_timing_sim_args(std::string harness_args) {
    size_t feeder_opt_end_pos = harness_args.find("-t ") +
                            strlen("-t ");
    size_t feeder_so_pos = harness_args.find("feeder_zesto.so");
    std::string feeder_path = harness_args.substr(feeder_opt_end_pos, feeder_so_pos - feeder_opt_end_pos);

    std::string res = boost::replace_all_copy<std::string>(
        harness_args, "feeder_zesto", "timing_sim");

    auto pos = res.rfind("--");
    res.replace(pos, res.length(), "-- " + feeder_path + "timing_wait");
    std::cout << "timing_sim args: " << res << std::endl;
    return res;
}

// Fork the timing simulator process and return its pid.
pid_t fork_timing_simulator(std::string run_str) {
  pid_t timing_sim_pid = fork();
  switch (timing_sim_pid) {
    case 0: {   // child
      std::string timing_cmd = get_timing_sim_args(run_str);
      int ret = system(timing_cmd.c_str());
      if (WEXITSTATUS(ret) != 0)
        std::cerr << "Execv failed: " << timing_cmd << std::endl;
      exit(0);
    }
    case 1: {
      perror("Fork failed.");
    }
    default: {  // parent
      std::cout << "Timing simulator: " << timing_sim_pid << std::endl;
    }
  }
  return timing_sim_pid;
}


int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Insufficient command line arguments." << std::endl;
    exit(-1);
  }

  using namespace boost::interprocess;
  using namespace xiosim::shared;

  // Get the argument to the cfg_filename flag.
  std::string cfg_filename = "";
  if (strncmp(CFG_FILE_FLAG.c_str(), argv[1],
              CFG_FILE_FLAG.length()) == 0) {
    cfg_filename = argv[2];
  } else {
    std::cerr << "Must specify a valid benchmark configuration file with the "
             << "-benchmark_cfg flag." << std::endl;
    exit(1);
  }

  // Parse the benchmark configuration file.
  cfg_opt_t program_opts[] {
      CFG_STR("exec_path", "", CFGF_NONE),
      CFG_STR("command_line_args", "", CFGF_NONE),
      CFG_INT("instances", 1, CFGF_NONE),
      CFG_END()
  };
  cfg_opt_t opts[] {
      CFG_SEC("program", program_opts, CFGF_MULTI | CFGF_TITLE),
      CFG_END()
  };
  cfg_t *cfg = cfg_init(opts, 0);
  cfg_parse(cfg, cfg_filename.c_str());

  // Compute the total number of benchmark processes that will be forked.
  int num_programs = cfg_size(cfg, "program");
  for (int i = 0; i < num_programs; i++) {
    cfg_t *program_cfg = cfg_getnsec(cfg, "program", i);
    int instances = cfg_getint(program_cfg, "instances");
    harness_num_processes += instances;
  }

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
  int command_start_pos = 3; // There are three harness-specific arguments.
  bool inserted_harness = false;
  for (int i = command_start_pos; i < argc; i++) {
    // If the next arg is "-s", add the harness_pid flag.
    if ((strncmp(argv[i], LAST_PINTOOL_ARG.c_str(),
                LAST_PINTOOL_ARG.length()) == 0 ) &&
         (strnlen(argv[i], LAST_PINTOOL_ARG.length()+1) ==
                LAST_PINTOOL_ARG.length())) {
      command_stream << " " << HARNESS_PID_FLAG << " " << harness_pid << " ";
      inserted_harness = true;
    }
    command_stream << argv[i] << " ";
  }
  if (!inserted_harness) {
    std:: cerr << "Failed to add harness_pid to tool args!" << std::endl;
    abort();
  }
  command_stream << "-- ";  // For appending the benchmark program arguments.

  std::cerr << "HARNESS CMD: " << command_stream.str() << std::endl;

  // Shared object keys are prefixed by the harness pid that created them.
  std::stringstream harness_pid_stream;
  harness_pid_stream << harness_pid;
  std::string shared_memory_key =
      harness_pid_stream.str() + std::string(XIOSIM_SHARED_MEMORY_KEY);
  std::string init_counter_key =
      harness_pid_stream.str() + std::string(XIOSIM_INIT_COUNTER_KEY);
  std::string shared_lock_key =
      harness_pid_stream.str() + std::string(XIOSIM_INIT_SHARED_LOCK);

  // Creates a new shared segment.
  permissions perm;
  perm.set_unrestricted();
  global_shm = new managed_shared_memory(open_or_create, XIOSIM_SHARED_MEMORY_KEY,
       DEFAULT_SHARED_MEMORY_SIZE);

  // Sets up a counter for multiprogramming. It is initialized to the number of
  // processes that are to be forked. When each forked process is about to start
  // the pin-instructed program, it will decrement this counter atomically. When
  // the counter reaches zero, the process is allowed to continue execution.
  // This also initializes a shared mutex for the forked processes to use for
  // synchronizing access to this counter.
  permissions perm;
  perm.set_unrestricted();
  int *counter =
    global_shm->find_or_construct<int>(XIOSIM_INIT_COUNTER_KEY)(harness_num_processes-1);
  named_mutex init_lock(open_or_create, XIOSIM_INIT_SHARED_LOCK, perm);

  SHARED_VAR_INIT(XIOSIM_LOCK, printing_lock);
  InitIPCQueues();

  init_lock.unlock();

  // Track the pids of all children.
  harness_num_processes++;  // For the timing simulator.
  harness_pids = new pid_t[harness_num_processes];

  // Create a process for timing simulator and store its pid.
  harness_pids[harness_num_processes - 1] = fork_timing_simulator(
      command_stream.str());

  // Fork all the benchmark child processes.
  int status;
  int nthprocess = 0;
  for (int program = 0; program < num_programs; program++) {
    cfg_t *program_cfg = cfg_getnsec(cfg, "program", program);
    int instances = cfg_getint(program_cfg, "instances");

    for (int process = 0; process < instances; process++) {
      harness_pids[nthprocess] = fork();
      std::string run_str = command_stream.str();

      // Append program command line arguments.
      std::stringstream ss;
      ss << cfg_getstr(program_cfg, "exec_path") << " " <<
            cfg_getstr(program_cfg, "command_line_args");
      run_str += ss.str();

      switch (harness_pids[nthprocess]) {
        case 0:  {  // child
          int ret = system(run_str.c_str());
          if (WIFSIGNALED(ret))
            abort();
          exit(WEXITSTATUS(ret));
          break;
        }
        case -1:  {
          perror("Fork failed.");
          break;
        }
        default:  {  // parent
          std::cout << "New producer: " << harness_pids[nthprocess] << std::endl;
          nthprocess++;
          break;
        }
      }
    }
  }

  // Waits for the children process to finish. If any of the children, including
  // the timing simulator process, terminate unexpectedly, then ALL children are
  // killed immediately. Otherwise, it waits for only the producer children to
  // finish normally. The timing simulator is instructed to terminate
  // separately later.
  std::cout << "[HARNESS] Waiting for feeder children to finish." << std::endl;
  for (int i = 0; i < harness_num_processes-1; i++) {
    // Wait for any child process to finish.
    pid_t terminated_process = wait(&status);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      std::cerr << "Process " << terminated_process << " failed." << std::endl;
      kill_children(status);
    }
  }

  std::cout << "[HARNESS] Letting timing_sim finish" << std::endl;
  ipc_message_t msg;
  msg.StopSimulation(true);
  SendIPCMessage(msg);

  while (waitpid(harness_pids[0], &status, 0) != harness_pids[0]);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::cerr << "Process " << harness_pids[0] << " failed." << std::endl;
  }

  std::cout << "[HARNESS] Parent exiting." << std::endl;
  delete[](harness_pids);
  return 0;
}
