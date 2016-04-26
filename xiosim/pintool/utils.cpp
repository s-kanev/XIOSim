#include <map>
#include <regex>
#include <queue>
#include "feeder.h"
#include "utils.h"

time_t last_time;

bool parse_sym(std::string sym_off, symbol_off_t& result) {
    auto re = std::regex("([^\\+]*)(?:\\+)?((?:0x)?[A-Fa-f0-9]+)?");
    std::smatch re_match;

    if (!std::regex_match(sym_off, re_match, re)) {
        std::cerr << "Couldn't parse symbol " << sym_off << std::endl;
        return false;
    }

    if (re_match.size() - 1 != 2) {
        std::cerr << "Knob parse RE has the wrong number of groups." << std::endl;
        return false;
    }

    ADDRINT off = 0;
    if (re_match[2].length() > 0)
        off = std::stol(re_match[2], nullptr, 0);
    result.symbol_name = re_match[1];
    result.offset = off;
    return true;
}

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
