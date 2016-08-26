#ifndef DATAPROCESSINGRESULT_H
#define DATAPROCESSINGRESULT_H

enum DataProcessingResultState {
    DPR_READY,
    DPR_AWAIT_READ,
    DPR_AWAIT_WRITE
};

typedef struct {
    bool closeConn;
    enum DataProcessingResultState reqState;
    int reqAwaitFd;
    enum DataProcessingResultState respState;
    int respAwaitFd;
} DataProcessingResult;


void dpr_init(DataProcessingResult*);


void dpr_setReqState(DataProcessingResult*,
        enum DataProcessingResultState, int awaitFd);


void dpr_setRespState(DataProcessingResult*,
        enum DataProcessingResultState, int awaitFd);


/* Sets closeConn member value to true
 */
void dpr_setCloseConn(DataProcessingResult*);


#endif /* DATAPROCESSINGRESULT_H */
