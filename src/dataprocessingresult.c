#include <stdbool.h>
#include "dataprocessingresult.h"


void dpr_init(DataProcessingResult *dpr)
{
    dpr->closeConn = false;
    dpr->reqState = DPR_READY;
    dpr->reqAwaitFd = -1;
    dpr->respState = DPR_READY;
    dpr->respAwaitFd = -1;
}

void dpr_setReqState(DataProcessingResult *dpr,
        enum DataProcessingResultState state, int awaitFd)
{
    dpr->reqState = state;
    dpr->reqAwaitFd = awaitFd;
}

void dpr_setRespState(DataProcessingResult *dpr,
        enum DataProcessingResultState state, int awaitFd)
{
    dpr->respState = state;
    dpr->respAwaitFd = awaitFd;
}

void dpr_setCloseConn(DataProcessingResult *dpr)
{
    dpr->closeConn = true;
}
