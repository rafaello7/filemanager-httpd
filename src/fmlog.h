#ifndef FMLOG_H
#define FMLOG_H


bool log_isLevel(unsigned);


/* Prints out error message and exits.
 */
void log_fatal(const char *msg, ...);


void log_error(const char *msg, ...);
void log_warn(const char *msg, ...);
void log_debug(const char *msg, ...);


#endif /* FMLOG_H */
