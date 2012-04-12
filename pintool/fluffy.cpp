/* 
 * Fluffy - looking for regions of interest when running under ILDJIT.
 * Copyright, Svilen Kanev, 2011
 */

#include <vector>
#include <map>

#include "feeder.h"

//XXX: This is single-threaded for now. Move to TLS for multithreading.

static std::vector<ADDRINT> start_counts;
static std::vector<ADDRINT> stop_counts;

static std::vector<ADDRINT> curr_start_counts;
static std::vector<ADDRINT> curr_stop_counts;

static std::vector<ADDRINT> slice_weights_times_1000;

/* ========================================================================== */
VOID FLUFFY_Init()
{
    if (KnobFluffy.Value().empty())
        return;

    ifstream annotation_file(KnobFluffy.Value().c_str());

    ADDRINT start, end, id;
    double weight;
    std::string command;
    char ch;
    while (true)
    {
        annotation_file >> id >> command;
        if (annotation_file.eof())
            break;
        if (command == "Start")
        {
            do { annotation_file >> ch; } while (ch != ':');
            annotation_file >> start;
            start_counts.push_back(start);
        }
        else if (command == "End")
        {
            do { annotation_file >> ch; } while (ch != ':');
            annotation_file >> end;
            stop_counts.push_back(end);
        }
        else if (command == "Weight")
        {
            do { annotation_file >> ch; } while (ch != ':');
            annotation_file >> weight;
            slice_weights_times_1000.push_back((ADDRINT)(weight*1000*100));
        }
        else
            ASSERTX(false);
    }
    ASSERTX(start_counts.size() == stop_counts.size());
    ADDRINT npoints = start_counts.size();

    for(ADDRINT i=0; i<npoints; i++) {
#ifdef ZESTO_PIN_DBG
        cerr << start_counts [i] << " " << stop_counts[i] << endl;
#endif
        curr_start_counts.push_back(0);
        curr_stop_counts.push_back(0);
    }
}

/* ========================================================================== */
VOID FLUFFY_StartInsn(THREADID tid, ADDRINT pc, ADDRINT phase)
{
    if ((curr_start_counts[phase]++) == start_counts[phase]) {
        PPointHandler(CONTROL_START, NULL, NULL, (VOID*)pc, tid);
    }
}

/* ========================================================================== */
VOID FLUFFY_StopInsn(THREADID tid, ADDRINT pc, ADDRINT phase)
{
    if ((curr_stop_counts[phase]++) == stop_counts[phase]) {
        PPointHandler(CONTROL_STOP, NULL, NULL, (VOID*)pc, tid);

        thread_state_t* tstate = get_tls(tid);
        tstate->slice_num++;
        tstate->slice_length = 0; //XXX: fix icount failing with all ildjit threads
        tstate->slice_weight_times_1000 = slice_weights_times_1000[phase];
    }
}

