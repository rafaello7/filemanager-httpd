#ifndef DATAREADYSELECTOR_H
#define DATAREADYSELECTOR_H


typedef struct DataReadySelector DataReadySelector;


DataReadySelector *drs_new(void);


void drs_setReadFd(DataReadySelector*, int fd);


void drs_setWriteFd(DataReadySelector*, int fd);


/* Invokes select() with file descriptors set since previous call
 * of the drs_select
 */
void drs_select(DataReadySelector*);


/* Sets O_NONBLOCK and FD_CLOEXEC flags on the file descriptor
 */
void drs_setNonBlockingCloExecFlags(int fd);

#endif /* DATAREADYSELECTOR_H */
