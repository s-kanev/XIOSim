// Harness utility that launches XIOSIM for a multiprogrammed workload. Muliple
// Pin processes running a pintool for a workload are forked. The harness
// creates a shared memory segment for XIOSIM to use as interprocess
// communication between the separate Pin processes.
//
// To usei:
//   harness -num_processes n setarch i686 ... pin -pin_args ...
//     ./pin_tool.o -pin_tool_flags ...
//
// In other words, run the pintool like usual with all of the necessary flags,
// but pass the entire command string to the harness executable along with a
// num_processes flag that tells the harness how many instances of Pin to fork.
//
// If the num_processes flag is present, it MUST be the first flag passed to the
// harness. If the flag is absent, it defaults to 1.
//
// Author: Sam Xi

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/permissions.hpp>

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mpkeys.h"

namespace xiosim {
namespace shared {

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

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Insufficient command line arguments." << std::endl;
    exit(-1);
  }

  using namespace boost::interprocess;
  using namespace xiosim::shared;

  // Get the argument to the num_processes flag if present.
  harness_num_processes = 1;
  bool flag_present = false;
  std::stringstream convert(argv[2]);
  if (strncmp(NUM_PROCESSES_FLAG.c_str(), argv[1],
              NUM_PROCESSES_FLAG.length()) == 0) {
    flag_present = true;
    if (!(convert >> harness_num_processes)) {
      std::cerr << "Invalid argument to num_processes flag." << std::endl;
      exit(-1);
    }
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
  int command_start_pos = flag_present ? 3 : 1;
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

  // Removes any shared data left from a previous run if they exist.
  shared_memory_object::remove(XIOSIM_SHARED_MEMORY_KEY.c_str());
  named_mutex::remove(XIOSIM_INIT_SHARED_LOCK.c_str());

  // Creates a new shared segment.
  permissions perm;
  perm.set_unrestricted();
  // managed_shared_memory shm(open_or_create, XIOSIM_SHARED_MEMORY_KEY.c_str(),
  //     DEFAULT_SHARED_MEMORY_SIZE, (void*) 0x0, perm);

  // Sets up a counter for multiprogramming. It is initialized to the number of
  // processes that are to be forked. When each forked process is about to start
  // the pin-instructed program, it will decrement this counter atomically. When
  // the counter reaches zero, the process is allowed to continue execution.
  // This also initializes a shared mutex for the forked processes to use for
  // synchronizing access to this counter.
  // int *counter =
  //  shm.find_or_construct<int>(XIOSIM_INIT_COUNTER_KEY.c_str())(harness_num_processes);
  named_mutex init_lock(open_or_create, XIOSIM_INIT_SHARED_LOCK.c_str(), perm);
  init_lock.unlock();

  int status;
  harness_pids = new pid_t[harness_num_processes];
  for (int i = 0; i < harness_num_processes; i++) {
    harness_pids[i] = fork();

    switch (harness_pids[i]) {
      case 0:  { // child
        system(command_stream.str().c_str());
        exit(0);
        break;
      }
      case -1:  {
        perror("Fork failed.");
        break;
      }
      default:  {  // parent
        std::cout << "New process " << harness_pids[i] << std::endl;
        break;
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
