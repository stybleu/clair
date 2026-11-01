#ifndef CLAIR_WEB_H
#define CLAIR_WEB_H

#include "ast.h"

/*
 * V0.55: checks the @web.route(...) of the program against their functions and
 * writes __web_handle(), the function the server calls for each request.
 * Runs before modules_resolve.  Returns 0 after an error (already printed).
 */
int web_prepare(AstNode *program);

#endif
