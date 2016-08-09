#include <string>

#include "BufferManagerProducer.h"
#include "feeder.h"
#include "speculation.h"

#include "trash_hook.h"

const std::string hook_name = "xiosim_trash_cache";

static void BeforeTrashHook(THREADID tid, ADDRINT pc) {
    if (ExecMode != EXECUTION_MODE_SIMULATE)
        return;

    if (!CheckIgnoreConditions(tid, pc))
        return;

    thread_state_t* tstate = get_tls(tid);
    if (speculation_mode) {
        FinishSpeculation(tstate);
        return;
    }

    auto handshake = xiosim::buffer_management::GetBuffer(tstate->tid);
    handshake->flags.is_cache_trash = true;
}

void AddTrashCallbacks(IMG img) {
    RTN rtn = RTN_FindByName(img, hook_name.c_str());
    if (RTN_Valid(rtn)) {
#ifdef TRASH_DEBUG
        cerr << IMG_Name(img) << ": " << hook_name << endl;
#endif

        RTN_Open(rtn);
        RTN_InsertCall(rtn,
                       IPOINT_BEFORE,
                       AFUNPTR(BeforeTrashHook),
                       IARG_THREAD_ID,
                       IARG_INST_PTR,
                       IARG_CALL_ORDER,
                       CALL_ORDER_FIRST,
                       IARG_END);
        RTN_Close(rtn);
    }
}
