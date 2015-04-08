#include "feeder.h"
#include "parsec.h"

KNOB<BOOL> KnobParsec(KNOB_MODE_WRITEONCE, "pintool", "parsec", "false", "Add parsec hooks");

VOID Parsec_BeginROI(THREADID tid, ADDRINT pc) {
    StartSimSlice(1);
    ResumeSimulation(true);
}

VOID Parsec_EndROI(THREADID tid, ADDRINT pc) {
    PauseSimulation();
    EndSimSlice(1, 0, 1000 * 100);
}

VOID AddParsecCallbacks(IMG img) {
    RTN rtn;
    rtn = RTN_FindByName(img, "__parsec_roi_begin");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": __parsec_roi_begin" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(Parsec_BeginROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "__parsec_roi_end");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": __parsec_roi_end" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(Parsec_EndROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
