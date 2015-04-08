#include <map>
#include <queue>
#include "feeder.h"
#include "utils.h"

time_t last_time;

VOID printElapsedTime() {
    time_t elapsed_time = time(NULL) - last_time;
    time_t hours = elapsed_time / 3600;
    time_t minutes = (elapsed_time % 3600) / 60;
    time_t seconds = ((elapsed_time % 3600) % 60);
    cerr << hours << "h" << minutes << "m" << seconds << "s" << endl;
    last_time = time(NULL);
}

VOID printMemoryUsage(THREADID tid) {
    lk_lock(printing_lock, tid + 1);
    int myPid = getpid();
    char str[50];
    sprintf(str, "%d", myPid);

    ifstream fin;
    fin.open(("/proc/" + string(str) + "/status").c_str());
    string line;
    while (getline(fin, line)) {
        if (line.find("VmSize") != string::npos) {
            cerr << tid << ":" << line << endl;
            break;
        }
    }
    fin.close();
    lk_unlock(printing_lock);
}
