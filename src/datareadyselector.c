#include <stdbool.h>
#include "datareadyselector.h"
#include "fmlog.h"
#include <sys/select.h>
#include <stdlib.h>


struct DataReadySelector {
    fd_set readFds;
    fd_set writeFds;
    int numFds;
};


DataReadySelector *drs_new(void)
{
    DataReadySelector *drs = malloc(sizeof(DataReadySelector));

    FD_ZERO(&drs->readFds);
    FD_ZERO(&drs->writeFds);
    drs->numFds = 0;
    return drs;
}

void drs_setReadFd(DataReadySelector *drs, int fd)
{
    FD_SET(fd, &drs->readFds);
    if( fd >= drs->numFds )
        drs->numFds = fd + 1;
}

void drs_setWriteFd(DataReadySelector *drs, int fd)
{
    FD_SET(fd, &drs->writeFds);
    if( fd >= drs->numFds )
        drs->numFds = fd + 1;
}

#if 0
bool drs_clearReadFd(DataReadySelector *drs, int fd)
{
    bool isSet = FD_ISSET(fd, &drs->readFds);

    if( isSet )
        FD_CLR(fd, &drs->readFds);
    return isSet;
}

bool drs_clearWriteFd(DataReadySelector *drs, int fd)
{
    bool isSet = FD_ISSET(fd, &drs->writeFds);

    if( isSet )
        FD_CLR(fd, &drs->writeFds);
    return isSet;
}
#endif

void drs_select(DataReadySelector *drs)
{
    if( select(drs->numFds, &drs->readFds, &drs->writeFds, NULL, NULL) < 0 )
        log_fatal("select");
    FD_ZERO(&drs->readFds);
    FD_ZERO(&drs->writeFds);
    drs->numFds = 0;
}

