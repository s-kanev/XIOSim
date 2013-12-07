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
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/permissions.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>

#include "mpkeys.h"

#include "confuse.h"  // For parsing config files.

namespace xiosim {
namespace shared {

const std::string CFG_FILE_FLAG = "-benchmark_cfg";
const std::string NUM_PROCESSES_FLAG = "-num_processes";
const std::string LAST_PINTOOL_ARG = "-s";

pid_t *harness_pids;
int harness_num_processes = 0;

}  // namespace shared
}  // namespace xiosim

// Kills all child processes that were forked by the harness when the harness
// intercepts a SIGINT interrupt. This doesn't seem to work properly yet - Pin
// seems to be forking other child processes and the harness doesn't know thosd
// pids.
void kill_handler(int sig) {
  using namespace xiosim::shared;
  std::cout << "Caught SIGINT: Killing child processes." << std::endl;
  for (int i = 0; i < harness_num_processes; i++) {
    kill(harness_pids[i], SIGTERM);
  }
  exit(1);
}

// Modify the generic pintool command for the timing simulator.
std::string get_timing_sim_args(std::string harness_args) {
    std::string res = boost::replace_all_copy<std::string>(
        harness_args, "feeder_zesto", "timing_sim");

    auto pos = res.rfind("--");
    //XXX: get path from feeder_zesto.so
    res.replace(pos, res.length(), "-- ./timing_wait");
    std::cout << "timing_sim args: " << res << std::endl;
    return res;
}

// Fork the timing simulator process and return its pid.
pid_t fork_timing_simulator(std::string run_str) {
  pid_t timing_sim_pid = fork();
  switch (timing_sim_pid) {
    case 0: {   // child
      std::string timing_cmd = get_timing_sim_args(run_str);
      system(timing_cmd.c_str());
      exit(0);
      break;
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
  int harness_num_processes = 0;
  for (int i = 0; i < num_programs; i++) {
    cfg_t *program_cfg = cfg_getnsec(cfg, "program", i);
    int instances = cfg_getint(program_cfg, "instances");
    harness_num_processes += instances;
  }

  // Setup SIGINT handler to kill child processes as well.
  struct sigaction sig_int_handler;
  sig_int_handler.sa_handler = kill_handler;
  sigemptyset(&sig_int_handler.sa_mask);
  sig_int_handler.sa_flags = 0;
  sigaction(SIGINT, &sig_int_handler, NULL);

  // Concatenate the flags into a single string to be executed when setup is
  // complete. Exclude the num_processes flag part if present.
  std::stringstream command_stream;
  int command_start_pos = 3;  // There are three harness-specific arguments.
  for (int i = command_start_pos; i < argc; i++) {
    // If the next arg is "-s", add the num_processes flag. We need to tell the
    // pintool how many processes we have running so it can set up shared memory
    // on its own.
    if (strncmp(argv[i], LAST_PINTOOL_ARG.c_str(),
                LAST_PINTOOL_ARG.length()) == 0) {
      command_stream << " " << NUM_PROCESSES_FLAG << " "
                     << harness_num_processes << " ";
    }
    command_stream << argv[i] << " ";
  }
  command_stream << "-- ";  // For appending the benchmark program arguments.

  // Removes any shared data left from a previous run if they exist.
  shared_memory_object::remove(XIOSIM_SHARED_MEMORY_KEY);
  named_mutex::remove(XIOSIM_INIT_SHARED_LOCK);

  // Creates a new shared segment.
  permissions perm;
  perm.set_unrestricted();
  managed_shared_memory shm(open_or_create, XIOSIM_SHARED_MEMORY_KEY,
       DEFAULT_SHARED_MEMORY_SIZE);

  // Sets up a counter for multiprogramming. It is initialized to the number of
  // processes that are to be forked. When each forked process is about to start
  // the pin-instructed program, it will decrement this counter atomically. When
  // the counter reaches zero, the process is allowed to continue execution.
  // This also initializes a shared mutex for the forked processes to use for
  // synchronizing access to this counter.
  int *counter =
    shm.find_or_construct<int>(XIOSIM_INIT_COUNTER_KEY)(harness_num_processes);
  named_mutex init_lock(open_or_create, XIOSIM_INIT_SHARED_LOCK, perm);
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
          std::cout << run_str << std::endl;
          system(run_str.c_str());
          exit(0);
          break;
        }
        case -1:  {
          perror("Fork failed.");
          break;
        }
        default:  {  // parent
          nthprocess++;
          if (nthprocess == 0)
            std::cout << "Timing simulator: " << harness_pids[nthprocess] << std::endl;
          else
            std::cout << "New producer: " << harness_pids[nthprocess] << std::endl;
          break;
        }
      }
    }
  }

  std::cout << "Waiting for children to finish." << std::endl;
  for (int i = 0; i < harness_num_processes; i++) {
    while (waitpid(harness_pids[i], &status, 0) != harness_pids[i]);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      std::cerr << "Process " << harness_pids[i] << " failed." << std::endl;
    }
  }

  std::cout << "Parent exiting." << std::endl;
  delete[](harness_pids);
  return 0;
}
