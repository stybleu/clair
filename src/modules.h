#ifndef CLAIR_MODULES_H
#define CLAIR_MODULES_H
#include "ast.h"

/*
 * V0.47: import and from ... import.
 *
 *   import math as m              m.sqrt(x), m.pi
 *   import phys                   phys.G, phys.c
 *   from astro import GM_mars     GM_mars
 *   import orbite                 orbite.cl next to the program: orbite.f(x)
 *
 * math, phys and astro live in the compiler: a constant becomes a number in
 * the C code and m.sqrt(x) the same call as sqrt(x).  A file module is read,
 * its classes and functions join the program (a function f of orbite is
 * named orbite.f; classes keep their name), and its own main() is left out,
 * so a module can keep a main() to test itself.  Every reference is
 * rewritten here; the import statements are then removed, so the semantic
 * pass and code generation only see ordinary calls and numbers.
 *
 * Returns 1 when everything is resolved, 0 after printing an error.
 */
int modules_resolve(AstNode *program, const char *input_path);

#endif
