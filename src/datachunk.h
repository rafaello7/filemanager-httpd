#ifndef DATACHUNK_H
#define DATACHUNK_H

typedef struct {
    const char *data;
    unsigned len;
} DataChunk;

void dchClear(DataChunk*);

void dchSet(DataChunk*, const char *data, unsigned len);

int dchShift(DataChunk*, unsigned size);

int dchShiftAfterChr(DataChunk*, char);

int dchShiftAfterStr(DataChunk*, const char*);

/* Shifts data chunk position skipping characters specified in str
 */
void dchSkip(DataChunk*, const char *str);

/* Extracts sub-chunk of data starting at dch begin till (but not including)
 * first occurrence of the specified string in dch. The dch begin is shifted
 * after the found string.
 * If the string does not appear in dch, the dch and subChunk are unchanged.
 * Returns non-zero when the string was found, 0 otherwise.
 */
int dchExtractTillStr(DataChunk *dch, const char *str, DataChunk *subChunk);

int dchEqualsStr(const DataChunk*, const char*);

int dchStartsWithStr(const DataChunk*, const char*);

int dchIndexOfStr(const DataChunk*, const char*);


char *dchDupToStr(const DataChunk*);


#endif /* DATACHUNK_H */
