#ifndef DATAREADYSELECTOR_H
#define DATAREADYSELECTOR_H

typedef struct DataReadySelector DataReadySelector;

DataReadySelector *drs_new(void);


void drs_setReadFd(DataReadySelector*, int fd);


void drs_setWriteFd(DataReadySelector*, int fd);


bool drs_clearReadFd(DataReadySelector*, int fd);

bool drs_clearWriteFd(DataReadySelector*, int fd);

void drs_select(DataReadySelector*);

#endif /* DATAREADYSELECTOR_H */
