#ifndef DATAREADYSELECTOR_H
#define DATAREADYSELECTOR_H

typedef struct DataReadySelector DataReadySelector;

DataReadySelector *drs_new(void);


void drs_setReadFd(DataReadySelector*, int fd);


void drs_setWriteFd(DataReadySelector*, int fd);


bool drs_clearReadFd(DataReadySelector*, int fd);

bool drs_clearWriteFd(DataReadySelector*, int fd);

void drs_select(DataReadySelector*);


/* Sets O_NONBLOCK and FD_CLOEXEC flags on the file descriptor
 */
void drs_setNonBlockingCloExecFlags(int fd);

#endif /* DATAREADYSELECTOR_H */
