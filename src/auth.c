#include <stdbool.h>
#include "auth.h"
#include "membuf.h"
#include "datachunk.h"
#include "md5calc.h"
#include "fmconfig.h"
#include "fmlog.h"
#include <string.h>
#include <time.h>
#include <stdio.h>


const char FM_REALM[] = "File Manager";


char *auth_getAuthResponseHeader(void)
{
    static unsigned long long nextNonce;
    MemBuf *authHeader = mb_new();
    char nonce[40];

    /* According to RFC2617, the nonce value is "uniquely generated
     * each time a 401 response is made". */
    if( nextNonce == 0 ) {
        nextNonce = time(NULL) * 1000ULL;
    }else{
        ++nextNonce;
    }
    sprintf(nonce, "%llx", nextNonce);
    mb_appendStrL(authHeader, "Digest realm=\"", FM_REALM, "\", "
            "nonce=\"", nonce, "\", " "qop=\"auth\"", NULL);
    log_debug("auth: resp nonce=%s", nonce);
    return mb_unbox_free(authHeader);
}

bool auth_isClientAuthorized(const char *authorization,
        const char *requestMethod)
{
    bool res = false;
    DataChunk dchLine, dchName, dchValue;
    DataChunk dchUsername, dchNonce, dchUri;
    DataChunk dchResponse, dchNc, dchCNonce;
    MemBuf *response, *a;
    char md5sum[40];

    /* Authorization header example:
     *  Digest username="abc", realm="File Manager", nonce="1234", uri="/",
     *   response="72dd44377dacd9ce557e0048b5ad5335", qop=auth, nc=00000001,
     *   cnonce="53a7a7e25e8587ea"
     */
    dch_initWithStr(&dchLine, authorization);
    if( ! dch_extractTillWS(&dchLine, &dchName) ||
            !dch_equalsStrIgnoreCase(&dchName, "Digest") )
        return false;
    dch_clear(&dchUsername);
    dch_clear(&dchNonce);
    dch_clear(&dchUri);
    dch_clear(&dchResponse);
    dch_clear(&dchNc);
    dch_clear(&dchCNonce);
    while( dch_extractParam(&dchLine, &dchName, &dchValue, ',') ) {
        if( dch_equalsStr(&dchName, "username") )
            dchUsername = dchValue;
        else if( dch_equalsStr(&dchName, "realm") ) {
            if( ! dch_equalsStr(&dchValue, FM_REALM) )
                return false;
        }else if( dch_equalsStr(&dchName, "nonce") )
            dchNonce = dchValue;
        else if( dch_equalsStr(&dchName, "uri") )
            dchUri = dchValue;
        else if( dch_equalsStr(&dchName, "response") )
            dchResponse = dchValue;
        else if( dch_equalsStr(&dchName, "nc") )
            dchNc = dchValue;
        else if( dch_equalsStr(&dchName, "cnonce") )
            dchCNonce = dchValue;
    }
    /* the "response=" value is MD5 sum of:
     *      MD5( unq(username) ":" unq(realm) ":" password ) ":"
     *          unq(nonce) ":" nc ":" unq(cnonce) ":" unq(response-qop) ":"
     *          MD5( requestMethod ":" unq(uri) )
     *
     * response-qop is "qop" value from "WWW-Authenticate" response header
     * (here: "auth")
     */
    response = mb_new();
    mb_resize(response, 32);    /* a place for MD5 of A1 */
    mb_appendStr(response, ":");
    mb_appendChunk(response, &dchNonce);
    mb_appendStr(response, ":");
    mb_appendChunk(response, &dchNc);
    mb_appendStr(response, ":");
    mb_appendChunk(response, &dchCNonce);
    mb_appendStr(response, ":auth:");
    a = mb_new();
    mb_appendStrL(a, requestMethod, ":", NULL);
    mb_appendChunk(a, &dchUri);
    md5_calculate(md5sum, mb_data(a), mb_dataLen(a));
    mb_free(a);
    mb_appendStr(response, md5sum);
    if( config_getDigestAuthCredential(dchUsername.data, dchUsername.len,
                md5sum) )
    {
        mb_setData(response, 0, md5sum, 32);
        md5_calculate(md5sum, mb_data(response), mb_dataLen(response));
        res = dch_equalsStr(&dchResponse, md5sum);
    }
    mb_free(response);
    return res;
}

