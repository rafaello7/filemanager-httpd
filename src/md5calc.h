#ifndef MD5CALC_H
#define MD5CALC_H


/* Calculates MD5 sum of bytes.
 * The result is 32-byte string (plus '\0' byte) in format same as
 * printed by md5sum program.
 */
void md5_calculate(char *result, const char *bytes, unsigned count);


#endif /* MD5CALC_H */
