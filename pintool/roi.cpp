#include <list>
#include <string>

#include "feeder.h"
#include "roi.h"

using namespace std;

KNOB<BOOL> KnobROI(KNOB_MODE_WRITEONCE, "pintool", "roi", "false", "Add ROI support");
KNOB<string> KnobROIBegin(KNOB_MODE_WRITEONCE,
                          "pintool",
                          "roi_begin",
                          "xiosim_roi_begin",
                          "Function name that starts simulated ROI");
KNOB<string> KnobROIEnd(KNOB_MODE_WRITEONCE,
                        "pintool",
                        "roi_end",
                        "xiosim_roi_end",
                        "Function name that ends simulated ROI");

/* Instrumentaion for beginROI: start a new simulation slice, and
 * start producing instructions from the feeder. */
static void BeginROI(THREADID tid, ADDRINT pc) {
    {
        std::lock_guard<XIOSIM_LOCK> l(*printing_lock);
        cerr << "tid: " << dec << tid << " ip: " << hex << pc << " ";
        cerr << "Start" << endl;
    }

    StartSimSlice(1);
    ResumeSimulation(true);
}

/* Instrumentaion for endROI: stop producing instructions, and
 * end simulation slice. */
static void EndROI(THREADID tid, ADDRINT pc) {
    {
        std::lock_guard<XIOSIM_LOCK> l(*printing_lock);
        cerr << "tid: " << dec << tid << " ip: " << hex << pc << " ";
        cerr << "Stop" << endl;
    }

    PauseSimulation();
    EndSimSlice(1, 0, 1000 * 100);
}

void AddROICallbacks(IMG img) {
    if (!KnobROI.Value())
        return;

    /* Add instrumentation before beginROI. */
    string begin_name = KnobROIBegin.Value();
    RTN rtn = RTN_FindByName(img, begin_name.c_str());
    if (RTN_Valid(rtn)) {
#ifdef ROI_DEBUG
        cerr << IMG_Name(img) << ": " << begin_name << endl;
#endif

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(BeginROI),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    /* Add instrumentation after endROI. */
    string end_name = KnobROIEnd.Value();
    rtn = RTN_FindByName(img, end_name.c_str());
    if (RTN_Valid(rtn)) {
#ifdef ROI_DEBUG
        cerr << IMG_Name(img) << ": " << end_name << endl;
#endif

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(EndROI),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
