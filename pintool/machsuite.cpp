#include "feeder.h"
#include "machsuite.h"

KNOB<BOOL>
    KnobMachsuite(KNOB_MODE_WRITEONCE, "pintool", "machsuite", "false", "Add machsuite hooks");

VOID Machsuite_BeginROI(THREADID tid, ADDRINT pc) {
    StartSimSlice(1);
    ResumeSimulation(true);
}

VOID Machsuite_EndROI(THREADID tid, ADDRINT pc) {
    PauseSimulation();
    EndSimSlice(1, 0, 1000 * 100);
}

VOID AddMachsuiteCallbacks(IMG img) {
    RTN rtn;
    rtn = RTN_FindByName(img, "run_benchmark");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": run_benchmark" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(Machsuite_BeginROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }

    rtn = RTN_FindByName(img, "run_benchmark");
    if (RTN_Valid(rtn)) {
        cerr << IMG_Name(img) << ": run_benchmark" << endl;

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_AFTER,
                       AFUNPTR(Machsuite_EndROI),
                       IARG_THREAD_ID,
                       IARG_CALL_ORDER,
                       CALL_ORDER_LAST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
