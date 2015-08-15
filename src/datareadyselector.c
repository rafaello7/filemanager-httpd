#include <stdbool.h>
#include "datareadyselector.h"
#include "fmlog.h"
#include <sys/select.h>
#include <stdlib.h>
#include <fcntl.h>


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

void drs_setNonBlockingCloExecFlags(int fd)
{
    int fdFlags;

    if( (fdFlags = fcntl(fd, F_GETFL)) == -1 )
        log_fatal("fcntl(F_GETFL)");
    if( fcntl(fd, F_SETFL, fdFlags | O_NONBLOCK) < 0 )
        log_fatal("fcntl(F_SETFL)");
    if( (fdFlags = fcntl(fd, F_GETFD)) == -1 )
        log_fatal("fcntl(F_GETFD)");
    if( fcntl(fd, F_SETFD, fdFlags | FD_CLOEXEC) < 0 )
        log_fatal("fcntl(F_SETFD)");
}

