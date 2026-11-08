#ifndef CLAIR_UNITS_H
#define CLAIR_UNITS_H
#include "ast.h"
#include <stddef.h>

/*
 * V0.48: units checked when compiling, erased afterwards.
 *
 *   let d: float[m] = 2.25e11
 *   let t: float[s] = 3.0e7
 *   let v: float[m/s] = d / t          // checked: m / s
 *   let x = d + t                      // refused: m + s
 *   let g: float[m/s^2] = 9.81
 *   let r: float[m] = 6.4e6 + 400.0[km] * 1000.0[m/km]
 *
 * A unit is a product of symbols with whole exponents.  The SI derived units
 * (N, J, W, Pa, Hz, C, V, ...) are written in base units (m, kg, s, A, K, mol,
 * cd); any other symbol (km, h, au, deg...) is a unit of its own: km and m are
 * never mixed, 1000.0[m/km] converts.  Only values whose unit is written are
 * checked; a float without a unit is not.  The C code is exactly the same as
 * without units.
 */

/*
 * The canonical text of a unit written in the program ("m^3/(kg*s^2)" gives
 * "kg^-1 m^3 s^-2", a number without unit gives "1"), or NULL with a message
 * in err.
 */
char *units_normalize(const char *text, char *err, size_t err_size);

/* The canonical text written for people: "m^3/(kg*s^2)". */
void units_display(const char *canonical, char *out, size_t out_size);

/* Checks the units of a program that passed the semantic pass; 1 when right. */
int units_check(AstNode *program);

#endif
