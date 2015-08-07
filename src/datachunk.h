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
void dch_clear(DataChunk*);


/* Initializes chunk reference with the specified data and length.
 */
void dch_init(DataChunk*, const char *data, unsigned len);


/* Initializes chunk reference with the string
 */
void dch_initWithStr(DataChunk*, const char *str);


/* Moves chunk begin forward by size bytes. The chunk end location is
 * kept unchanged.
 * Returns true on success, false when size exceeds chunk length.
 */
bool dch_shift(DataChunk*, unsigned size);


/* Moves chunk begin after first occurrence of the specified character.
 * Returns true on success, false when the chunk does not contain
 * such character. When fail, the chunk is unchanged.
 *
 * Chunk end location stays unchanged.
 */
bool dch_shiftAfterChr(DataChunk*, char);


/* Moves chunk begin after first occurrence of the specified string.
 * Returns true on success, false when the chunk does not contain
 * such string.
 */
bool dch_shiftAfterStr(DataChunk*, const char*);


/* Moves begin of data chunk forward, skipping characters specified in str
 */
void dch_skipLeading(DataChunk*, const char *str);


/* Removes from chunk trailing characters specified in str.
 */
void dch_trimTrailing(DataChunk*, const char *str);


/* Removes from chunk leading and trailing white spaces.
 */
void dch_trimWS(DataChunk*);


/* Like dirname(), except that returns empty string instead of "."
 */
void dch_dirNameOf(const DataChunk *fileName, DataChunk *dirName);


/* Extracts sub-chunk of data, which starts at dch begin and continues to
 * (but not including the) first occurrence of the specified string.
 * The dch begin is shifted after end of the found string.
 * If the string does not appear in dch, the subChunk is set to whole dch and
 * dch is set to empty.
 * Returns true when the string was found, false otherwise.
 */
bool dch_extractTillStr(DataChunk *dch, DataChunk *subChunk, const char *str);


/* Like dch_extractTillStr, but the ending string is constructed as
 * concatenation of two strings passed as parameters.
 */
bool dch_extractTillStr2(DataChunk *dch, DataChunk *subChunk,
        const char *str1, const char *str2);


/* Like dch_extractTillStr, but additionally white spaces surrounding the
 * string are also removed from resulting chunks.
 */
bool dch_extractTillStrStripWS(DataChunk *dch, DataChunk *subChunk,
        const char *str);


/* Trims leading white spaces from dch and then extracts sub-chunk of data,
 * which starts at dch begin and continues to (but not including the)
 * first white space. The dch begin is shifted after end of white spaces
 * section. If no white space appears in dch, the subChunk is set to whole
 * dch and dch is set to empty.
 * Returns true when the dch contains any non-whitespace characters,
 * false otherwise.
 */
bool dch_extractTillWS(DataChunk *dch, DataChunk *subChunk);


/* Returns true when the data chunk is equal to the specified string,
 * i.e. the chunk length is equal to the string length and the chunk bytes
 * are equal to the string (zero byte terminating string is not considered
 * a part of string).
 */
bool dch_equalsStr(const DataChunk*, const char*);
bool dch_equalsStrIgnoreCase(const DataChunk*, const char*);


/* Returns true when the chunk initial part is equal to the specified string.
 */
bool dch_startsWithStr(const DataChunk*, const char*);
bool dch_startsWithStrIgnoreCase(const DataChunk*, const char*);


/* Returns index of first occurrence of the specified string within data chunk.
 * Returns -1 when the data chunk does not contain the string.
 */
int dch_indexOfStr(const DataChunk*, const char*);


/* Returns index of end of segment starting at idxFrom and consisting entirely
 * of c character occurrences.
 */
unsigned dch_endOfSpan(const DataChunk*, unsigned idxFrom, char c);


/* Returns index of end of segment starting at idxFrom and not containing
 * c character.
 */
unsigned dch_endOfCSpan(const DataChunk*, unsigned idxFrom, char c);


/* Returns a copy of the data chunk. The copy is terminated with zero byte.
 * The copy should be released by free().
 * Never returns NULL. For empty chunk the empty string is returned.
 */
char *dch_dupToStr(const DataChunk*);


/* Converts string in chunk to unsigned integer.
 * Returns true on success
 */
bool dch_toUInt(const DataChunk*, int base, unsigned*);

#endif /* DATACHUNK_H */
