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
void dch_Clear(DataChunk*);


/* Initializes chunk reference with the specified data and length.
 */
void dch_Init(DataChunk*, const char *data, unsigned len);


/* Initializes chunk reference with the string
 */
void dch_InitWithStr(DataChunk*, const char *str);


/* Moves chunk begin forward by size bytes. The chunk end location is
 * kept unchanged.
 * Returns non-zero on success, zero when size exceeds chunk length.
 */
int dch_Shift(DataChunk*, unsigned size);


/* Moves chunk begin after first occurrence of the specified character.
 * Returns non-zero on success, zero when the chunk does not contain
 * such character. When fail, the chunk is unchanged.
 *
 * Chunk end location stays unchanged.
 */
int dch_ShiftAfterChr(DataChunk*, char);


/* Moves chunk begin after first occurrence of the specified string.
 * Returns non-zero on success, zero when the chunk does not contain
 * such string.
 */
int dch_ShiftAfterStr(DataChunk*, const char*);


/* Moves begin of data chunk forward, skipping characters specified in str
 */
void dch_SkipLeading(DataChunk*, const char *str);


/* Removes from chunk trailing characters specified in str.
 */
void dch_TrimTrailing(DataChunk*, const char *str);


/* Removes from chunk leading and trailing white spaces.
 */
void dch_TrimWS(DataChunk*);


/* Extracts sub-chunk of data, which starts at dch begin and continues to
 * (but not including the) first occurrence of the specified string.
 * The dch begin is shifted after end of the found string.
 * If the string does not appear in dch, the subChunk is set to whole dch and
 * dch is set to empty.
 * Returns non-zero when the string was found, 0 otherwise.
 */
int dch_ExtractTillStr(DataChunk *dch, DataChunk *subChunk, const char *str);


/* Like dch_ExtractTillStr, but the ending string is constructed as
 * concatenation of two strings passed as parameters.
 */
int dch_ExtractTillStr2(DataChunk *dch, DataChunk *subChunk,
        const char *str1, const char *str2);


/* Like dch_ExtractTillStr, but additionally white spaces surrounding the
 * string are also removed from resulting chunks.
 */
int dch_ExtractTillStrStripWS(DataChunk *dch, DataChunk *subChunk,
        const char *str);


/* Trims leading white spaces from dch and then extracts sub-chunk of data,
 * which starts at dch begin and continues to (but not including the)
 * first white space. The dch begin is shifted after end of white spaces
 * section. If no white space appears in dch, the subChunk is set to whole
 * dch and dch is set to empty.
 * Returns non-zero when the dch contains any non-whitespace characters,
 * 0 otherwise.
 */
int dch_ExtractTillWS(DataChunk *dch, DataChunk *subChunk);


/* Returns non-zero when the data chunk is equal to the specified string,
 * i.e. the chunk length is equal to the string length and the chunk bytes
 * are equal to the string (zero byte terminating string is not considered
 * a part of string).
 */
int dch_EqualsStr(const DataChunk*, const char*);


/* Returns non-zero when the chunk initial part is equal to the specified
 * string.
 */
int dch_StartsWithStr(const DataChunk*, const char*);


/* Returns index of first occurrence of the specified string within data chunk.
 * Returns -1 when the data chunk does not contain the string.
 */
int dch_IndexOfStr(const DataChunk*, const char*);


/* Returns a copy of the data chunk. The copy is terminated with zero byte.
 * The copy should be released by free().
 */
char *dch_DupToStr(const DataChunk*);


/* Converts string in chunk to unsigned integer.
 * Returns non-zero on success
 */
int dch_ToUInt(const DataChunk*, int base, unsigned*);

#endif /* DATACHUNK_H */
