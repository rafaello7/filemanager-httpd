#ifndef CMDLINE_H
#define CMDLINE_H


bool cmdline_parse(int argc, char *argv[]);


const char *cmdline_getConfigLoc(void);

unsigned cmdline_getLogLevel(void);

bool cmdline_isInetdMode(void);

#endif /* CMDLINE_H */
