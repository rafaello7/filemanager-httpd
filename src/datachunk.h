#ifndef DATACHUNK_H
#define DATACHUNK_H


/* A reference to data chunk within byte buffer
 */
typedef struct {
    const char *data;   /* the data begin */
    unsigned len;       /* the data length */
} DataChunk;


/* Initializes chunk with empty data.
 */
void dchClear(DataChunk*);


/* Initializes chunk reference with the specified data and length.
 */
void dchInit(DataChunk*, const char *data, unsigned len);


/* Moves chunk begin forward by size bytes. The chunk end location is
 * kept unchanged.
 * Returns non-zero on success, zero when size exceeds chunk length.
 */
int dchShift(DataChunk*, unsigned size);


/* Moves chunk begin after first occurrence of the specified character.
 * Returns non-zero on success, zero when the chunk does not contain
 * such character. When fail, the chunk is unchanged.
 *
 * Chunk end location stays unchanged.
 */
int dchShiftAfterChr(DataChunk*, char);


/* Moves chunk begin after first occurrence of the specified string.
 * Returns non-zero on success, zero when the chunk does not contain
 * such string.
 */
int dchShiftAfterStr(DataChunk*, const char*);


/* Moves begin of data chunk forward, skipping characters specified in str
 */
void dchSkip(DataChunk*, const char *str);


/* Extracts sub-chunk of data, which starts at dch begin and continues to
 * (but not including the) first occurrence of the specified string.
 * The dch begin is shifted after end of the found string.
 * If the string does not appear in dch, the dch and subChunk are unchanged.
 * Returns non-zero when the string was found, 0 otherwise.
 */
int dchExtractTillStr(DataChunk *dch, DataChunk *subChunk, const char *str);


/* Like dchExtractTillStr, but the ending string is constructed as
 * concatenation of two strings passed as parameters.
 */
int dchExtractTillStr2(DataChunk *dch, DataChunk *subChunk,
        const char *str1, const char *str2);

/* Returns non-zero when the data chunk is equal to the specified string,
 * i.e. the chunk length is equal to the string length and the chunk bytes
 * are equal to the string (zero byte terminating string is not considered
 * a part of string).
 */
int dchEqualsStr(const DataChunk*, const char*);


/* Returns non-zero when the chunk initial part is equal to the specified
 * string.
 */
int dchStartsWithStr(const DataChunk*, const char*);


/* Returns index of first occurrence of the specified string within data chunk.
 * Returns -1 when the data chunk does not contain the string.
 */
int dchIndexOfStr(const DataChunk*, const char*);


/* Returns a copy of the data chunk. The copy is terminated with zero byte.
 * The copy should be released by free().
 */
char *dchDupToStr(const DataChunk*);


#endif /* DATACHUNK_H */
