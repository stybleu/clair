/*
 * V0.53: a measure, 9.81 ± 0.02.
 *
 * A measure keeps its value and, for each independent source of uncertainty it
 * comes from (a measurement written with ±, a constant of phys), how much that
 * source moves it: c = (d value / d source) * (uncertainty of the source).
 * The uncertainty is sqrt(sum of c^2) (first order, as the GUM does), and a
 * source met twice is counted once: x - x is exactly 0 ± 0.
 *
 * Copied into the C of the programs that use measures (tools/embed_file.cmake).
 */
#define CLAIR_MEASURE_K 16             /* sources kept apart; more are merged two by two */

typedef struct {
    double v;                          /* the value */
    int n;                             /* how many sources */
    uint64_t id[CLAIR_MEASURE_K];      /* the sources, in increasing order */
    double c[CLAIR_MEASURE_K];         /* what each source gives to the uncertainty, with its sign */
} ClairMeasure;
