#ifndef AUTH_H
#define AUTH_H


/* The file manager "realm" value used in authorization header.
 */
extern const char FM_REALM[];


/* Returns WWW-Authenticate header value to add to "401 Unauthorized"
 * response message
 */
char *auth_getAuthResponseHeader(void);


/* Parameters:
 *      authorization   - "Authorization" header field value.
 *      requestMethod   - request method, i.e. "GET", "POST", etc.
 * Returns true when the client authorization has passed, false otherwise.
 */
bool auth_isClientAuthorized(const char *authorization,
        const char *requestMethod);


#endif /* AUTH_H */
