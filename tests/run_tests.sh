#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

printf '%s\n' '=================================================='
printf '%s\n' ' CLAIR V0.51 - BUILD + TESTS'
printf '%s\n' '=================================================='

cmake -S . -B build
cmake --build build -j2

# CLAIR_SIMD_V3_BEGIN
if ! bash tests/run_simd_v3.sh; then
    echo 'SIMD V3 regression failed' >&2
    exit 1
fi

# Ce test inspecte l'assembleur NEON : il n'a de sens que sur ARM64.
case "$(uname -m)" in
    aarch64|arm64)
        if ! bash tests/run_simd_v4.sh; then
            echo "SIMD V4 regression failed" >&2
            exit 1
        fi
        ;;
    *)
        echo "SIMD V4 NEON : ignore (machine $(uname -m), ARM64 requis)"
        ;;
esac

if ! bash tests/run_simd_v4_shadow.sh; then
    echo "SIMD V4 SHADOW FAILED"
    exit 1
fi

if ! bash tests/run_simd_v4_safety.sh; then
    echo "SIMD V4 SAFETY FAILED"
    exit 1
fi

if ! bash tests/run_int_div_checked.sh; then
    echo "Integer division regression failed" >&2
    exit 1
fi
# CLAIR_SIMD_V3_END

./build/clair tests/hello.cl -o build/test_hello
out_hello="$(./build/test_hello)"
[ "$out_hello" = "Hello from Clair" ]
printf 'hello.cl      : OK -> %s\n' "$out_hello"

./build/clair tests/math.cl -o build/test_math
out_math="$(./build/test_math)"
[ "$out_math" = "70" ]
printf 'math.cl       : OK -> %s\n' "$out_math"

./build/clair tests/functions.cl -o build/test_functions
out_functions="$(./build/test_functions)"
expected_functions="$(printf '30\n42')"
[ "$out_functions" = "$expected_functions" ]
printf '%s\n' 'functions.cl  : OK -> 30 / 42'

./build/clair tests/conditions.cl -o build/test_conditions
out_conditions="$(./build/test_conditions)"
expected_conditions="$(printf '12\n9\n1\n2\n100\n101\n102')"
[ "$out_conditions" = "$expected_conditions" ]
printf '%s\n' 'conditions.cl : OK -> 12 / 9 / 1 / 2 / 100 / 101 / 102'

./build/clair tests/while.cl -o build/test_while
out_while="$(./build/test_while)"
expected_while="$(printf '15\n3\n2\n1\n9')"
[ "$out_while" = "$expected_while" ]
printf '%s\n' 'while.cl      : OK -> 15 / 3 / 2 / 1 / 9'

./build/clair tests/for_range.cl -o build/test_for_range
out_for_range="$(./build/test_for_range)"
expected_for_range="$(printf '0\n1\n2\n3\n4\n14\n5\n3\n1\n2\n4\n6\n6')"
[ "$out_for_range" = "$expected_for_range" ]
printf '%s\n' 'for_range.cl  : OK -> range(1/2/3 args), negative step, nested loops'

./build/clair tests/range_zero.cl -o build/test_range_zero
if ./build/test_range_zero >build/range_zero.stdout 2>build/range_zero.stderr; then
    printf '%s\n' 'range_zero.cl : ECHEC -> un step nul aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: range() step cannot be zero' build/range_zero.stderr
printf '%s\n' 'range_zero.cl : OK -> step nul refuse a l execution'

./build/clair tests/break_continue.cl -o build/test_break_continue
out_break_continue="$(./build/test_break_continue)"
expected_break_continue="$(printf '1\n3\n4\n0\n2\n3\n4\n0\n1\n10\n11\n20\n21')"
[ "$out_break_continue" = "$expected_break_continue" ]
printf '%s\n' 'break_continue.cl : OK -> while, for, continue, break, boucles imbriquees'

if ./build/clair tests/break_outside_loop.cl -o build/test_break_outside >build/break_outside.stdout 2>build/break_outside.stderr; then
    printf '%s\n' 'break_outside_loop.cl : ECHEC -> break hors boucle aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "'break' is only valid inside a loop" build/break_outside.stderr
printf '%s\n' 'break_outside_loop.cl : OK -> break hors boucle refuse'

if ./build/clair tests/continue_outside_loop.cl -o build/test_continue_outside >build/continue_outside.stdout 2>build/continue_outside.stderr; then
    printf '%s\n' 'continue_outside_loop.cl : ECHEC -> continue hors boucle aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "'continue' is only valid inside a loop" build/continue_outside.stderr
printf '%s\n' 'continue_outside_loop.cl : OK -> continue hors boucle refuse'

./build/clair tests/bool_logic.cl -o build/test_bool_logic
out_bool_logic="$(./build/test_bool_logic)"
expected_bool_logic="$(printf '1\n2\n3\n4\n1\n0\n5\n6\n7')"
[ "$out_bool_logic" = "$expected_bool_logic" ]
printf '%s\n' 'bool_logic.cl : OK -> bool, logique, priorites, court-circuit, params/returns'

./build/clair tests/float_numeric.cl -o build/test_float_numeric
out_float_numeric="$(./build/test_float_numeric)"
expected_float_numeric="$(printf '7.5\n2.5\n7\n9\n6\n2.5\n-1.5\n1')"
[ "$out_float_numeric" = "$expected_float_numeric" ]
printf '%s\n' 'float_numeric.cl : OK -> float, promotions int->float, calcul mixte, division, unaire, comparaisons'

if ./build/clair tests/invalid_float_to_int.cl -o build/test_invalid_float_to_int >build/invalid_float_to_int.stdout 2>build/invalid_float_to_int.stderr; then
    printf '%s\n' 'invalid_float_to_int.cl : ECHEC -> float vers int implicite aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "cannot initialize int 'x' with float" build/invalid_float_to_int.stderr
printf '%s\n' 'invalid_float_to_int.cl : OK -> conversion implicite float -> int refusee'

if ./build/clair tests/invalid_bool_numeric.cl -o build/test_invalid_bool_numeric >build/invalid_bool_numeric.stdout 2>build/invalid_bool_numeric.stderr; then
    printf '%s\n' 'invalid_bool_numeric.cl : ECHEC -> bool + int aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'arithmetic operators require int or float operands' build/invalid_bool_numeric.stderr
printf '%s\n' 'invalid_bool_numeric.cl : OK -> bool dans arithmetique refuse'

if ./build/clair tests/invalid_range_float.cl -o build/test_invalid_range_float >build/invalid_range_float.stdout 2>build/invalid_range_float.stderr; then
    printf '%s\n' 'invalid_range_float.cl : ECHEC -> range(float) aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'range() arguments must be int' build/invalid_range_float.stderr
printf '%s\n' 'invalid_range_float.cl : OK -> range() reste strictement entier'

if ./build/clair tests/invalid_float_argument.cl -o build/test_invalid_float_argument >build/invalid_float_argument.stdout 2>build/invalid_float_argument.stderr; then
    printf '%s\n' 'invalid_float_argument.cl : ECHEC -> argument float vers int aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "argument 1 of 'take_int' expects int, got float" build/invalid_float_argument.stderr
printf '%s\n' 'invalid_float_argument.cl : OK -> conversion float -> int refusee dans les appels'

if ./build/clair tests/invalid_float_return.cl -o build/test_invalid_float_return >build/invalid_float_return.stdout 2>build/invalid_float_return.stderr; then
    printf '%s\n' 'invalid_float_return.cl : ECHEC -> retour float vers int aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'return expects int, got float' build/invalid_float_return.stderr
printf '%s\n' 'invalid_float_return.cl : OK -> conversion float -> int refusee au retour'

./build/clair tests/string_basic.cl -o build/test_string_basic
out_string_basic="$(./build/test_string_basic)"
expected_string_basic="$(printf 'Hello\nClair\n1\n2\n5\nHello\nChanged')"
[ "$out_string_basic" = "$expected_string_basic" ]
printf '%s\n' 'string_basic.cl : OK -> string, print, params/returns, ==, !=, len(), affectation'

./build/clair tests/string_concat.cl -o build/test_string_concat
out_string_concat="$(./build/test_string_concat)"
expected_string_concat="$(printf 'Hello Clair\n11\nAB\n1\nxyz\n40')"
[ "$out_string_concat" = "$expected_string_concat" ]
printf '%s\n' 'string_concat.cl : OK -> +, chainage, len(), comparaison, parametres/retours'

if ./build/clair tests/invalid_string_to_int.cl -o build/test_invalid_string_to_int >build/invalid_string_to_int.stdout 2>build/invalid_string_to_int.stderr; then
    printf '%s\n' 'invalid_string_to_int.cl : ECHEC -> string vers int aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "cannot initialize int 'x' with string" build/invalid_string_to_int.stderr
printf '%s\n' 'invalid_string_to_int.cl : OK -> string vers int refuse'

if ./build/clair tests/invalid_string_numeric.cl -o build/test_invalid_string_numeric >build/invalid_string_numeric.stdout 2>build/invalid_string_numeric.stderr; then
    printf '%s\n' 'invalid_string_numeric.cl : ECHEC -> addition string aurait du etre refusee en V0.9' >&2
    exit 1
fi
grep -Fq "operator '+' requires two numbers or two strings" build/invalid_string_numeric.stderr
printf '%s\n' 'invalid_string_numeric.cl : OK -> string + non-string refuse'

if ./build/clair tests/invalid_string_order.cl -o build/test_invalid_string_order >build/invalid_string_order.stdout 2>build/invalid_string_order.stderr; then
    printf '%s\n' 'invalid_string_order.cl : ECHEC -> comparaison ordonnee string aurait du etre refusee' >&2
    exit 1
fi
grep -Fq 'ordered comparisons require int or float operands' build/invalid_string_order.stderr
printf '%s\n' 'invalid_string_order.cl : OK -> < <= > >= reserves aux nombres'

if ./build/clair tests/invalid_len_type.cl -o build/test_invalid_len_type >build/invalid_len_type.stdout 2>build/invalid_len_type.stderr; then
    printf '%s\n' 'invalid_len_type.cl : ECHEC -> len(int) aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'len() expects string, list or dictionary, got int' build/invalid_len_type.stderr
printf '%s\n' 'invalid_len_type.cl : OK -> len() exige string, list ou dictionary'

if ./build/clair tests/invalid_string_argument.cl -o build/test_invalid_string_argument >build/invalid_string_argument.stdout 2>build/invalid_string_argument.stderr; then
    printf '%s\n' 'invalid_string_argument.cl : ECHEC -> int vers string en argument aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "argument 1 of 'take_string' expects string, got int" build/invalid_string_argument.stderr
printf '%s\n' 'invalid_string_argument.cl : OK -> argument non-string refuse'

if ./build/clair tests/invalid_string_return.cl -o build/test_invalid_string_return >build/invalid_string_return.stdout 2>build/invalid_string_return.stderr; then
    printf '%s\n' 'invalid_string_return.cl : ECHEC -> retour int vers string aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'return expects string, got int' build/invalid_string_return.stderr
printf '%s\n' 'invalid_string_return.cl : OK -> retour non-string refuse'

./build/clair tests/list_int.cl -o build/test_list_int
out_list_int="$(./build/test_list_int)"
expected_list_int="$(printf '10\n30\n3\n10\n20\n6\n200\n0\n7\n2')"
[ "$out_list_int" = "$expected_list_int" ]
printf '%s\n' 'list_int.cl : OK -> list[int], litteraux, indexation, len(), params/returns, affectation'

./build/clair tests/list_stress.cl -o build/test_list_stress
out_list_stress="$(./build/test_list_stress)"
[ "$out_list_stress" = "40" ]
printf '%s\n' 'list_stress.cl : OK -> 40 allocations de listes suivies'

./build/clair tests/list_bounds.cl -o build/test_list_bounds
if ./build/test_list_bounds >build/list_bounds.stdout 2>build/list_bounds.stderr; then
    printf '%s\n' 'list_bounds.cl : ECHEC -> index hors limites aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: list index 3 out of bounds for length 3' build/list_bounds.stderr
printf '%s\n' 'list_bounds.cl : OK -> index hors limites refuse a l execution'

./build/clair tests/list_negative_index.cl -o build/test_list_negative_index
if ./build/test_list_negative_index >build/list_negative_index.stdout 2>build/list_negative_index.stderr; then
    printf '%s\n' 'list_negative_index.cl : ECHEC -> index negatif trop grand aurait du echouer' >&2
    exit 1
fi
[ "$(cat build/list_negative_index.stdout)" = "10" ]
grep -Fq 'Clair runtime error: list index -2 out of bounds for length 1' build/list_negative_index.stderr
printf '%s\n' 'list_negative_index.cl : OK -> xs[-1] est le dernier element, xs[-2] hors limites refuse'

if ./build/clair tests/invalid_list_element.cl -o build/test_invalid_list_element >build/invalid_list_element.stdout 2>build/invalid_list_element.stderr; then
    printf '%s\n' 'invalid_list_element.cl : ECHEC -> element float dans list[int] aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'list[int] literal expects int elements, got float' build/invalid_list_element.stderr
printf '%s\n' 'invalid_list_element.cl : OK -> elements non-int refuses'

if ./build/clair tests/invalid_list_index_type.cl -o build/test_invalid_list_index_type >build/invalid_list_index_type.stdout 2>build/invalid_list_index_type.stderr; then
    printf '%s\n' 'invalid_list_index_type.cl : ECHEC -> index float aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'list index must be int, got float' build/invalid_list_index_type.stderr
printf '%s\n' 'invalid_list_index_type.cl : OK -> index non-int refuse'

if ./build/clair tests/invalid_index_non_list.cl -o build/test_invalid_index_non_list >build/invalid_index_non_list.stdout 2>build/invalid_index_non_list.stderr; then
    printf '%s\n' 'invalid_index_non_list.cl : ECHEC -> indexation string aurait du etre refusee' >&2
    exit 1
fi
grep -Fq 'indexing expects a list or dictionary, got string' build/invalid_index_non_list.stderr
printf '%s\n' 'invalid_index_non_list.cl : OK -> indexation reservee aux listes et dictionnaires'

if ./build/clair tests/invalid_list_type.cl -o build/test_invalid_list_type >build/invalid_list_type.stdout 2>build/invalid_list_type.stderr; then
    printf '%s\n' 'invalid_list_type.cl : ECHEC -> liste imbriquee aurait du etre refusee en V0.13' >&2
    exit 1
fi
grep -Fq 'list element type must be int, float, float32, bool, string or u8 ... u64, i8 ... i32' build/invalid_list_type.stderr
printf '%s\n' 'invalid_list_type.cl : OK -> listes imbriquees reportees'

if ./build/clair tests/invalid_list_equality.cl -o build/test_invalid_list_equality >build/invalid_list_equality.stdout 2>build/invalid_list_equality.stderr; then
    printf '%s\n' 'invalid_list_equality.cl : ECHEC -> egalite de listes aurait du etre refusee en V0.14' >&2
    exit 1
fi
grep -Fq 'list or dictionary equality is not supported in Clair V0.15' build/invalid_list_equality.stderr
printf '%s\n' 'invalid_list_equality.cl : OK -> egalite de listes et dictionnaires reportee'


./build/clair tests/list_mutable.cl -o build/test_list_mutable
out_list_mutable="$(./build/test_list_mutable)"
expected_list_mutable="$(printf '25\n40\n4\n5\n100\n50\n1\n8')"
[ "$out_list_mutable" = "$expected_list_mutable" ]
printf '%s\n' 'list_mutable.cl : OK -> index assignment, append, aliasing, params, empty list'

./build/clair tests/list_append_stress.cl -o build/test_list_append_stress
out_list_append_stress="$(./build/test_list_append_stress)"
expected_list_append_stress="$(printf '100\n0\n50\n99')"
[ "$out_list_append_stress" = "$expected_list_append_stress" ]
printf '%s\n' 'list_append_stress.cl : OK -> 100 append avec croissance dynamique'

./build/clair tests/string_list_combined.cl -o build/test_string_list_combined
out_string_list_combined="$(./build/test_string_list_combined)"
expected_string_list_combined="$(printf 'Hello Clair\n9\n2')"
[ "$out_string_list_combined" = "$expected_string_list_combined" ]
printf '%s\n' 'string_list_combined.cl : OK -> runtimes string et list compatibles'

./build/clair tests/list_set_bounds.cl -o build/test_list_set_bounds
if ./build/test_list_set_bounds >build/list_set_bounds.stdout 2>build/list_set_bounds.stderr; then
    printf '%s\n' 'list_set_bounds.cl : ECHEC -> ecriture hors limites aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: list index 3 out of bounds for length 3' build/list_set_bounds.stderr
printf '%s\n' 'list_set_bounds.cl : OK -> ecriture hors limites refusee a l execution'

./build/clair tests/list_set_negative_index.cl -o build/test_list_set_negative_index
if ./build/test_list_set_negative_index >build/list_set_negative_index.stdout 2>build/list_set_negative_index.stderr; then
    printf '%s\n' 'list_set_negative_index.cl : ECHEC -> ecriture index negatif trop grand aurait du echouer' >&2
    exit 1
fi
[ "$(cat build/list_set_negative_index.stdout)" = "9" ]
grep -Fq 'Clair runtime error: list index -2 out of bounds for length 1' build/list_set_negative_index.stderr
printf '%s\n' 'list_set_negative_index.cl : OK -> xs[-1] = v ecrit le dernier element, xs[-2] hors limites refuse'

if ./build/clair tests/invalid_list_set_non_list.cl -o build/test_invalid_list_set_non_list >build/invalid_list_set_non_list.stdout 2>build/invalid_list_set_non_list.stderr; then
    printf '%s\n' 'invalid_list_set_non_list.cl : ECHEC -> ecriture indexee sur int aurait du etre refusee' >&2
    exit 1
fi
grep -Fq 'indexed assignment expects a list or dictionary, got int' build/invalid_list_set_non_list.stderr
printf '%s\n' 'invalid_list_set_non_list.cl : OK -> ecriture indexee reservee aux listes et dictionnaires'

if ./build/clair tests/invalid_list_set_index_type.cl -o build/test_invalid_list_set_index_type >build/invalid_list_set_index_type.stdout 2>build/invalid_list_set_index_type.stderr; then
    printf '%s\n' 'invalid_list_set_index_type.cl : ECHEC -> index float en ecriture aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'list index must be int, got float' build/invalid_list_set_index_type.stderr
printf '%s\n' 'invalid_list_set_index_type.cl : OK -> index d ecriture doit etre int'

if ./build/clair tests/invalid_list_set_value.cl -o build/test_invalid_list_set_value >build/invalid_list_set_value.stdout 2>build/invalid_list_set_value.stderr; then
    printf '%s\n' 'invalid_list_set_value.cl : ECHEC -> valeur float dans list[int] aurait du etre refusee' >&2
    exit 1
fi
grep -Fq 'list[int] item assignment expects int, got float' build/invalid_list_set_value.stderr
printf '%s\n' 'invalid_list_set_value.cl : OK -> ecriture list[int] exige int'

if ./build/clair tests/invalid_list_append_non_list.cl -o build/test_invalid_list_append_non_list >build/invalid_list_append_non_list.stdout 2>build/invalid_list_append_non_list.stderr; then
    printf '%s\n' 'invalid_list_append_non_list.cl : ECHEC -> append sur int aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'append() expects a list, got int' build/invalid_list_append_non_list.stderr
printf '%s\n' 'invalid_list_append_non_list.cl : OK -> append reserve aux listes'

if ./build/clair tests/invalid_list_append_value.cl -o build/test_invalid_list_append_value >build/invalid_list_append_value.stdout 2>build/invalid_list_append_value.stderr; then
    printf '%s\n' 'invalid_list_append_value.cl : ECHEC -> append float dans list[int] aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'list[int].append() expects int, got float' build/invalid_list_append_value.stderr
printf '%s\n' 'invalid_list_append_value.cl : OK -> append list[int] exige int'



./build/clair tests/list_generic.cl -o build/test_list_generic
out_list_generic="$(./build/test_list_generic)"
expected_list_generic="$(printf '1
4
3
3
5.5
1
1
3
Hello CD
3
X
0
0
Clair
0
1.5
right')"
[ "$out_list_generic" = "$expected_list_generic" ]
printf '%s\n' 'list_generic.cl : OK -> list[float], list[bool], list[string], mutations, contexte, retours'


./build/clair tests/list_generic_stress.cl -o build/test_list_generic_stress
out_list_generic_stress="$(./build/test_list_generic_stress)"
expected_list_generic_stress="$(printf '100\n99\n100\n1\n0\n100\nab')"
[ "$out_list_generic_stress" = "$expected_list_generic_stress" ]
printf '%s\n' 'list_generic_stress.cl : OK -> 100 append float/bool/string et chaines allouees'

./build/clair tests/list_generic_bounds.cl -o build/test_list_generic_bounds
if ./build/test_list_generic_bounds >build/list_generic_bounds.stdout 2>build/list_generic_bounds.stderr; then
    printf '%s\n' 'list_generic_bounds.cl : ECHEC -> index string hors limites aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: list index 1 out of bounds for length 1' build/list_generic_bounds.stderr
printf '%s\n' 'list_generic_bounds.cl : OK -> bornes communes a tous les types de listes'

if ./build/clair tests/invalid_list_mixed.cl -o build/test_invalid_list_mixed >build/invalid_list_mixed.stdout 2>build/invalid_list_mixed.stderr; then
    printf '%s\n' 'invalid_list_mixed.cl : ECHEC -> int dans list[bool] aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'list[bool] literal expects bool elements, got int' build/invalid_list_mixed.stderr
printf '%s\n' 'invalid_list_mixed.cl : OK -> elements incompatibles refuses'

if ./build/clair tests/invalid_list_string_append.cl -o build/test_invalid_list_string_append >build/invalid_list_string_append.stdout 2>build/invalid_list_string_append.stderr; then
    printf '%s\n' 'invalid_list_string_append.cl : ECHEC -> int dans list[string] aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'list[string].append() expects string, got int' build/invalid_list_string_append.stderr
printf '%s\n' 'invalid_list_string_append.cl : OK -> append respecte le type element'

if ./build/clair tests/invalid_list_cross_assign.cl -o build/test_invalid_list_cross_assign >build/invalid_list_cross_assign.stdout 2>build/invalid_list_cross_assign.stderr; then
    printf '%s\n' 'invalid_list_cross_assign.cl : ECHEC -> list[float] vers list[int] aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "cannot assign list[float] to list[int] variable 'ints'" build/invalid_list_cross_assign.stderr
printf '%s\n' 'invalid_list_cross_assign.cl : OK -> types de listes restent distincts'

if ./build/clair tests/invalid_empty_list_context.cl -o build/test_invalid_empty_list_context >build/invalid_empty_list_context.stdout 2>build/invalid_empty_list_context.stderr; then
    printf '%s\n' 'invalid_empty_list_context.cl : ECHEC -> [] sans contexte aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'empty list literal requires an expected list type' build/invalid_empty_list_context.stderr
printf '%s\n' 'invalid_empty_list_context.cl : OK -> liste vide exige un contexte type'

./build/clair tests/class_basic.cl -o build/test_class_basic
out_class_basic="$(./build/test_class_basic)"
expected_class_basic="$(printf 'Alice\n30\n9\n1\ndev\nBob\n40\n9.5\n2\n41\nCore\nBob\n41')"
[ "$out_class_basic" = "$expected_class_basic" ]
printf '%s\n' 'class_basic.cl : OK -> Class, constructeur, champs, mutation, aliasing, objets imbriques'

./build/clair tests/class_stress.cl -o build/test_class_stress
out_class_stress="$(./build/test_class_stress)"
expected_class_stress="$(printf '99\n100')"
[ "$out_class_stress" = "$expected_class_stress" ]
printf '%s\n' 'class_stress.cl : OK -> 101 allocations objets suivies'

if ./build/clair tests/invalid_class_constructor_count.cl -o build/test_invalid_class_constructor_count >build/invalid_class_constructor_count.stdout 2>build/invalid_class_constructor_count.stderr; then
    printf '%s\n' 'invalid_class_constructor_count.cl : ECHEC -> arite constructeur aurait du etre refusee' >&2
    exit 1
fi
grep -Fq "class 'Person' constructor expects 2 argument(s), got 1" build/invalid_class_constructor_count.stderr
printf '%s\n' 'invalid_class_constructor_count.cl : OK -> arite constructeur controlee'

if ./build/clair tests/invalid_class_constructor_type.cl -o build/test_invalid_class_constructor_type >build/invalid_class_constructor_type.stdout 2>build/invalid_class_constructor_type.stderr; then
    printf '%s\n' 'invalid_class_constructor_type.cl : ECHEC -> type constructeur aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "constructor argument 2 for 'Person.age' expects int, got float" build/invalid_class_constructor_type.stderr
printf '%s\n' 'invalid_class_constructor_type.cl : OK -> types constructeur controles'

if ./build/clair tests/invalid_class_field.cl -o build/test_invalid_class_field >build/invalid_class_field.stdout 2>build/invalid_class_field.stderr; then
    printf '%s\n' 'invalid_class_field.cl : ECHEC -> champ inexistant aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "class 'Person' has no field 'name'" build/invalid_class_field.stderr
printf '%s\n' 'invalid_class_field.cl : OK -> champ inexistant refuse'

if ./build/clair tests/invalid_class_field_assignment.cl -o build/test_invalid_class_field_assignment >build/invalid_class_field_assignment.stdout 2>build/invalid_class_field_assignment.stderr; then
    printf '%s\n' 'invalid_class_field_assignment.cl : ECHEC -> mauvais type champ aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "field 'Person.age' expects int, got float" build/invalid_class_field_assignment.stderr
printf '%s\n' 'invalid_class_field_assignment.cl : OK -> type des champs controle en ecriture'

if ./build/clair tests/invalid_class_unknown_type.cl -o build/test_invalid_class_unknown_type >build/invalid_class_unknown_type.stdout 2>build/invalid_class_unknown_type.stderr; then
    printf '%s\n' 'invalid_class_unknown_type.cl : ECHEC -> type inconnu aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "unknown type 'Missing' for field 'Box.item'" build/invalid_class_unknown_type.stderr
printf '%s\n' 'invalid_class_unknown_type.cl : OK -> type de champ inconnu refuse'

if ./build/clair tests/invalid_class_duplicate_field.cl -o build/test_invalid_class_duplicate_field >build/invalid_class_duplicate_field.stdout 2>build/invalid_class_duplicate_field.stderr; then
    printf '%s\n' 'invalid_class_duplicate_field.cl : ECHEC -> champ duplique aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "duplicate field 'Person.age'" build/invalid_class_duplicate_field.stderr
printf '%s\n' 'invalid_class_duplicate_field.cl : OK -> champs dupliques refuses'

if ./build/clair tests/invalid_class_equality.cl -o build/test_invalid_class_equality >build/invalid_class_equality.stdout 2>build/invalid_class_equality.stderr; then
    printf '%s\n' 'invalid_class_equality.cl : ECHEC -> egalite objets aurait du etre refusee en V0.14' >&2
    exit 1
fi
grep -Fq 'object equality is not supported in Clair V0.14' build/invalid_class_equality.stderr
printf '%s\n' 'invalid_class_equality.cl : OK -> egalite objets reportee'

if ./build/clair tests/invalid_class_duplicate_top_level.cl -o build/test_invalid_class_duplicate_top_level >build/invalid_class_duplicate_top_level.stdout 2>build/invalid_class_duplicate_top_level.stderr; then
    printf '%s\n' 'invalid_class_duplicate_top_level.cl : ECHEC -> conflit Class/fonction aurait du etre refuse' >&2
    exit 1
fi
grep -Fq "duplicate top-level name 'Thing'" build/invalid_class_duplicate_top_level.stderr
printf '%s\n' 'invalid_class_duplicate_top_level.cl : OK -> namespace constructeur/fonction protege'

./build/clair tests/class_methods.cl -o build/test_class_methods
out_class_methods="$(./build/test_class_methods)"
expected_class_methods="$(printf '1\n5\n15\n25\n0\n100\n101\n102\nHello Alice\n1\n0\n31\n26\n27\n40\nHello Bob\n10\n6')"
[ "$out_class_methods" = "$expected_class_methods" ]
printf '%s\n' 'class_methods.cl : OK -> methodes, self, appels en instruction, mutation chainee'

expect_compile_error() {
    local name="$1" message="$2"
    if ./build/clair "tests/$name.cl" -o "build/test_$name" >"build/$name.stdout" 2>"build/$name.stderr"; then
        printf '%s\n' "$name.cl : ECHEC -> aurait du etre refuse" >&2
        exit 1
    fi
    grep -Fq "$message" "build/$name.stderr"
    printf '%s\n' "$name.cl : OK -> refuse"
}

expect_compile_error invalid_method_unknown "class 'Person' has no method 'fly'"
expect_compile_error invalid_method_arg_count "method 'Counter.add' expects 1 argument(s), got 2"
expect_compile_error invalid_method_arg_type "argument 1 of 'Counter.add' expects int, got float"
expect_compile_error invalid_method_duplicate "duplicate method 'Counter.reset'"
expect_compile_error invalid_method_field_conflict "method 'Counter.count' conflicts with a field of the same name"
expect_compile_error invalid_method_on_non_object "method call 'foo' expects a class object, got int"
expect_compile_error invalid_method_field_call "'Counter.count' is a field, not a method"
expect_compile_error invalid_append_on_call "append() needs a list variable or field"
expect_compile_error invalid_assignment_target "invalid assignment target"

./build/clair tests/c_name_collisions.cl -o build/test_c_name_collisions
out_c_name_collisions="$(./build/test_c_name_collisions)"
expected_c_name_collisions="$(printf '5\n2\n42\nhi!\n6\n10\n9\n3\n3')"
[ "$out_c_name_collisions" = "$expected_c_name_collisions" ]
printf '%s\n' 'c_name_collisions.cl : OK -> fonctions abs/exit/free/printf/strlen et variables char/default/NULL/size_t'

./build/clair tests/loop_fast_path.cl -o build/test_loop_fast_path
out_loop_fast_path="$(./build/test_loop_fast_path)"
expected_loop_fast_path="$(printf '7.5\n10.5\n6\n560\n110\n260\n6\n70\n3\nacd\n1\n9\n21\n6\n7\n7\n7\n22\n10000000000\n-9223372036854775808\n5\n9')"
[ "$out_loop_fast_path" = "$expected_loop_fast_path" ]
grep -Fq 'clair_rt_affine_ok(' build/test_loop_fast_path.generated.c
printf '%s\n' 'loop_fast_path.cl : OK -> boucles rapides sans verification, alias, pas negatif, constantes 64 bits'

expect_runtime_error() {
    local name="$1" stdout_expected="$2" message="$3"
    ./build/clair "tests/$name.cl" -o "build/test_$name"
    if ./build/test_$name </dev/null >"build/$name.stdout" 2>"build/$name.stderr"; then
        printf '%s\n' "$name.cl : ECHEC -> aurait du s arreter sur une erreur" >&2
        exit 1
    fi
    [ "$(cat "build/$name.stdout")" = "$(printf '%b' "$stdout_expected")" ]
    grep -Fq "$message" "build/$name.stderr"
    printf '%s\n' "$name.cl : OK -> erreur au bon moment"
}

expect_runtime_error loop_fast_path_bounds '0\n1\n2\n3\n4' 'list index 4 out of bounds for length 4'
expect_runtime_error loop_fast_path_overflow '0\n5\n1' 'list index 4611686018427387904 out of bounds for length 2'

./build/clair tests/math_builtins.cl -o build/test_math_builtins
out_math_builtins="$(./build/test_math_builtins)"
expected_math_builtins="$(printf '4\n1.4142135623731\n1024\n-3\n3\n7\n2.5\n3\n9.5\n7\n-7\n3.5\n1\n2\n-2\n2\n1.5\n1000\n2.5\n0.5\n0.665240955774822\n0\n2\n4\n5\n4')"
[ "$out_math_builtins" = "$expected_math_builtins" ]
grep -Fq 'clair_data_y[i] = sqrt(clair_data_x[i]);' build/test_math_builtins.generated.c
printf '%s\n' 'math_builtins.cl : OK -> sqrt, exp, tanh, pow, abs, min, max, int(), float(), %, 1e-3, sigmoid, softmax'

expect_compile_error invalid_math_arity "sqrt() expects 1 argument(s), got 2"
expect_compile_error invalid_math_type "argument 1 of sqrt() expects int or float, got string"
expect_compile_error invalid_mod_type "operator '%' requires int or float operands"
expect_runtime_error mod_zero_runtime '1' 'Clair runtime error: modulo by zero'
expect_runtime_error int_range_runtime '' 'Clair runtime error: int() argument out of range'

./build/clair tests/matrix_basic.cl -o build/test_matrix_basic
out_matrix_basic="$(./build/test_matrix_basic)"
expected_matrix_basic="$(printf '2\n3\n0\n6.5\n2\n5\n4\n11\n6\n0.731058578630005\n0.731058578630005\n9\n0\n5')"
[ "$out_matrix_basic" = "$expected_matrix_basic" ]
grep -Fq 'clair_data_c[i * clair_cols_c + j] = clair_data_c[i * clair_cols_c + j] + aik * clair_data_b[k * clair_cols_b + j];' build/test_matrix_basic.generated.c
printf '%s\n' 'matrix_basic.cl : OK -> matrix[float]/[int], m[i][j], rows/cols, couche dense, produit, boucles rapides'

expect_compile_error invalid_matrix_context "matrix() needs its type from context"
expect_compile_error invalid_matrix_row "m[i] alone is a matrix row: write m[i][j]"
expect_compile_error invalid_matrix_value "matrix[float] element assignment expects float, got string"
expect_compile_error invalid_matrix_len "len() of a matrix is ambiguous: use rows(m) or cols(m)"
expect_runtime_error matrix_bounds_runtime '0\n1\n2' 'Clair runtime error: matrix index [2][0] out of bounds for shape 2x2'
expect_runtime_error matrix_negative_runtime '' 'Clair runtime error: matrix() size cannot be negative'

./build/clair tests/field_writes.cl -o build/test_field_writes
out_field_writes="$(./build/test_field_writes)"
expected_field_writes="$(printf '3\n0.5\n8.5\n14.5\n2\n2\n7\n9\n42\n3\n0\n1')"
[ "$out_field_writes" = "$expected_field_writes" ]
grep -Fq 's += clair_data_self__w[i * clair_cols_self__w + j] * clair_data_x[j];' build/test_field_writes.generated.c
printf '%s\n' 'field_writes.cl : OK -> obj.items[i] = x, obj.w[i][j] = x, obj.items.append(x), boucles rapides sur self.w'

./build/clair tests/xor_training.cl -o build/test_xor_training
out_xor_training="$(./build/test_xor_training)"
expected_xor_training="$(printf '3\n97\n97\n4\n1')"
[ "$out_xor_training" = "$expected_xor_training" ]
grep -Fq 'if (clair_cols_self__w1 == 2) {' build/test_xor_training.generated.c
printf '%s\n' 'xor_training.cl : OK -> reseau de neurones entraine sur XOR par descente de gradient'

./build/clair tests/while_fast_path.cl -o build/test_while_fast_path
out_while_fast_path="$(./build/test_while_fast_path)"
expected_while_fast_path="$(printf '55\n3\n7.5\n24\n6\n6\n15\n40\n3')"
[ "$out_while_fast_path" = "$expected_while_fast_path" ]
grep -Fq 'while (i < clair_stop_i) {' build/test_while_fast_path.generated.c
printf '%s\n' 'while_fast_path.cl : OK -> boucles while rapides, continue, break, borne modifiee, matrice'
expect_runtime_error while_fast_path_bounds '-1\n1' 'Clair runtime error: list index -4 out of bounds for length 3'

./build/clair tests/nest_fast_path.cl -o build/test_nest_fast_path
out_nest_fast_path="$(./build/test_nest_fast_path)"
expected_nest_fast_path="$(printf '6\n12\n38\n0\n13\n11\n18')"
[ "$out_nest_fast_path" = "$expected_nest_fast_path" ]
grep -Fq 'clair_rt_affine2_ok(b, 0, n, 0, n, 1, 0, n)' build/test_nest_fast_path.generated.c
printf '%s\n' 'nest_fast_path.cl : OK -> double boucle verifiee une seule fois (produit de matrices)'
expect_runtime_error nest_fast_path_bounds '0\n1\n2' 'Clair runtime error: list index 4 out of bounds for length 4'

./build/clair tests/loop_fast_path.cl -o build/test_sans_verif --sans-verif
[ "$(./build/test_sans_verif)" = "$expected_loop_fast_path" ]
head -n 1 build/test_sans_verif.generated.c | grep -Fq -- '--sans-verif'
if grep -Fq 'clair_rt_affine' build/test_sans_verif.generated.c; then
    printf '%s\n' 'sans_verif : ECHEC -> verifications encore presentes' >&2
    exit 1
fi
./build/clair tests/mod_zero_runtime.cl -o build/test_sans_verif_mod --sans-verif
if ./build/test_sans_verif_mod >build/sans_verif_mod.stdout 2>build/sans_verif_mod.stderr; then
    printf '%s\n' 'sans_verif : ECHEC -> modulo par zero non detecte' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: modulo by zero' build/sans_verif_mod.stderr
printf '%s\n' '--sans-verif : OK -> meme resultat, aucune verification de bornes, division par zero toujours verifiee'

./build/clair tests/for_each.cl -o build/test_for_each
out_for_each="$(./build/test_for_each)"
expected_for_each="$(printf '31\n1.5\nClair\nest\nrapide\n2\n12\n180\n961\n10\n8\n8\n15\n100\n30\n4\n5\n2\n0\n3\n6\n50')"
[ "$out_for_each" = "$expected_for_each" ]
grep -Fq 'int64_t v = clair_items_v[clair_k_v];' build/test_for_each.generated.c
grep -Fq 'int64_t v = ((int64_t *)grow->data)[clair_k_v];' build/test_for_each.generated.c
printf '%s\n' 'for_each.cl : OK -> for v in liste, comme Python (liste qui grandit, liste remplacee, break, continue)'
expect_compile_error invalid_for_each_type "for ... in needs a list, got int"
expect_compile_error invalid_for_each_matrix "for ... in needs a list, got matrix[float]: use for i in range(rows(m))"
expect_compile_error invalid_for_each_element "operator '+' requires two numbers or two strings"

./build/clair tests/syntaxe_python.cl -o build/test_syntaxe_python
[ "$(./build/test_syntaxe_python)" = "$(cat tests/syntaxe_python.expected)" ]
grep -Fq '    n += 5;' build/test_syntaxe_python.generated.c
grep -Fq 'if (clair_rt_matmul_ok(a, b, c, a->rows, b->cols, a->cols)) {' build/test_syntaxe_python.generated.c
grep -Fq 'clair_rt_layer_f64(w->rows, w->cols, clair_layer_couche_1, &clair_layer);' build/test_syntaxe_python.generated.c
printf '%s\n' 'syntaxe_python.cl : OK -> += -= *= /= %=, enumerate, tranches xs[a:b:c] ; identique a Python, boucles += reconnues'
expect_runtime_error slice_step_zero_runtime '2' 'Clair runtime error: slice step cannot be zero'
expect_compile_error invalid_compound_call "'+=' would call suivant() twice: put its result in a variable first"
expect_compile_error invalid_slice_assign "a slice xs[a:b] cannot be assigned: change the elements one by one"
expect_compile_error invalid_enumerate "for i, v in ... needs enumerate(list)"
expect_compile_error invalid_slice_type "slices [a:b] work on lists and strings, got matrix[float]"

./build/clair tests/float32.cl -o build/test_float32
[ "$(./build/test_float32)" = "$(cat tests/float32.expected)" ]
grep -Fq 'float *clair_data_y = (float *)y->data;' build/test_float32.generated.c
grep -Fq 'axpy(0.25f, x, y)' build/test_float32.generated.c
grep -Fq 'sqrtf(s)' build/test_float32.generated.c
printf '%s\n' 'float32.cl : OK -> float32 identique a NumPy : calculs, listes, matrices, couche dense, boucles rapides en float'
expect_compile_error invalid_float32_to_int "cannot initialize int 'n' with float32"

./build/clair tests/noyaux_float32.cl -o build/test_noyaux_float32
[ "$(./build/test_noyaux_float32)" = "$(cat tests/noyaux_float32.expected)" ]
for threads in 1 2 3 7; do
    [ "$(CLAIR_THREADS=$threads ./build/test_noyaux_float32)" = "$(cat tests/noyaux_float32.expected)" ]
done
[ "$(grep -c 'clair_rt_matmul_f32(' build/test_noyaux_float32.generated.c)" = "3" ]
grep -Fq 'clair_rt_layer_f32(w->rows, w->cols, clair_layer_couche_1, &clair_layer);' build/test_noyaux_float32.generated.c
grep -Fq 'clair_rt_matvec_f32(' build/test_noyaux_float32.generated.c
grep -Fq 'float acc[4][16];' build/test_noyaux_float32.generated.c
printf '%s\n' 'noyaux_float32.cl : OK -> produits de matrices et couches denses float32 en blocs, sur 1, 2, 3 ou 7 coeurs, identiques au bit pres aux boucles simples'

./build/clair tests/hasard.cl -o build/test_hasard
[ "$(./build/test_hasard)" = "$(cat tests/hasard.expected)" ]
grep -Fq 'clair_rt_gauss(0.0, 0.125)' build/test_hasard.generated.c
./build/clair tests/hasard_sans_graine.cl -o build/test_hasard_sans_graine
[ "$(./build/test_hasard_sans_graine)" != "$(./build/test_hasard_sans_graine)" ]
printf '%s\n' 'hasard.cl : OK -> seed, random, randint, uniform, gauss, shuffle, choice : memes tirages que Python ; sans seed(), different a chaque lancement'
expect_runtime_error randint_empty_runtime '3' 'Clair runtime error: randint() empty range'
expect_runtime_error choice_empty_runtime '7' 'Clair runtime error: choice() from an empty list'
expect_compile_error invalid_randint_float "argument 1 of randint() expects an int, got float"
expect_compile_error invalid_shuffle_type "argument 1 of shuffle() expects a list, got int"

./build/clair tests/fichiers.cl -o build/test_fichiers
[ "$(./build/test_fichiers)" = "$(cat tests/fichiers.expected)" ]
grep -Fq 'clair_rt_load_csv("tests/donnees/fleurs.csv", 1)' build/test_fichiers.generated.c
printf '%s\n' 'fichiers.cl : OK -> load_csv, save_csv, read_lines, split, strip, int() et float() de chaines ; identique a Python'
expect_runtime_error csv_missing_runtime '1' "Clair runtime error: cannot open file 'tests/donnees/absent.csv'"
expect_runtime_error csv_bad_value_runtime '2' "Clair runtime error: load_csv(): line 2, value 2 of 'tests/donnees/mauvais.csv' is not a number: 'x'"
expect_runtime_error csv_ragged_runtime '3' "Clair runtime error: load_csv(): line 2 of 'tests/donnees/bancal.csv' has 1 values instead of 2"
expect_runtime_error float_string_runtime '4.5' "Clair runtime error: could not convert string to float: 'abc'"
expect_runtime_error int_string_runtime '12' "Clair runtime error: invalid literal for int() with base 10: '1.5'"
expect_runtime_error split_empty_runtime '2' 'Clair runtime error: split() empty separator'
./build/clair tests/briques_python.cl -o build/test_briques_python
[ "$(./build/test_briques_python)" = "$(cat tests/briques_python.expected)" ]
grep -Fq 'static ClairList clair_fn_comprehension_1(ClairList xs) {' build/test_briques_python.generated.c
grep -Fq 'clair_rt_list_has_i64(xs, 4)' build/test_briques_python.generated.c
printf '%s\n' 'briques_python.cl : OK -> xs[-1], x in xs, [x * x for x in xs if ...], for k in dico ; identique a Python'
expect_runtime_error dict_changed_runtime 'a' 'Clair runtime error: dictionary changed size during iteration'
expect_compile_error invalid_in_type "'in' cannot look for float in list[int]: convert it first, e.g. int(x)"
expect_compile_error invalid_in_right "'in' needs a list, a string or a dictionary on the right, got int"
expect_compile_error invalid_comprehension_list "[... for ...] makes a list of numbers, bool or strings, got list[int]"
expect_compile_error invalid_comprehension_cond "the condition of [... if ...] must be a bool, got int"
expect_compile_error invalid_load_csv_kind 'argument 2 of load_csv() must be written "float", "float32" or "int"'

./build/clair tests/absolue.cl -o build/test_absolue
[ "$(./build/test_absolue)" = "$(cat tests/absolue.expected)" ]
grep -Fq 'double * p = ((double *)xs->data);' build/test_absolue.generated.c
grep -Fq 'float * a = (float *)clair_rt_alloc(1000, sizeof(float), 64);' build/test_absolue.generated.c
grep -Fq 'y[i] = y[i] + a * x[i];' build/test_absolue.generated.c
printf '%s\n' 'absolue.cl : OK -> absolue { }, ptr[T], address, alloc aligne, fill, copy, bytes, absolue def ; identique a Python'
expect_runtime_error alloc_negative_runtime '1' 'Clair runtime error: alloc() size cannot be negative'
expect_runtime_error alloc_alignment_runtime '' 'Clair runtime error: alloc() alignment must be a power of two'
expect_runtime_error copy_negative_runtime '2.5' 'Clair runtime error: copy() count cannot be negative'
expect_compile_error invalid_ptr_outside "a ptr variable is only allowed inside an absolue { } block"
expect_compile_error invalid_absolue_call "f() is an absolue def: call it inside an absolue { } block"
expect_compile_error invalid_absolue_append "xs.append() in this absolue block could move the data that address(xs) points to"
expect_compile_error invalid_absolue_replace "xs = ... in this absolue block would free the data that address(xs) points to"
expect_compile_error invalid_alloc_type "alloc() needs its type: write let p: ptr[float32] = alloc(n)"
expect_compile_error invalid_ptr_param "f() takes or returns a ptr: write absolue def f(...)"
expect_compile_error invalid_ptr_field "field 'Tampon.p' cannot be a ptr: a ptr only lives inside absolue code"
expect_compile_error invalid_ptr_float_offset "operator '+' cannot use ptr[float] and float (a ptr moves with p + k, p - k)"

./build/clair tests/entiers_machine.cl -o build/test_entiers_machine
[ "$(./build/test_entiers_machine)" = "$(cat tests/entiers_machine.expected)" ]
grep -Fq 'return (uint8_t)(a * 31 + b);' build/test_entiers_machine.generated.c
grep -Fq 'x ^= x << 13;' build/test_entiers_machine.generated.c
grep -Fq 'return clair_rt_shl_u32(x, r) | clair_rt_shr_u32(x, 32 - r);' build/test_entiers_machine.generated.c
printf '%s\n' 'entiers_machine.cl : OK -> u8 a u64, i8 a i32, & | ^ ~ << >>, 0x 0b 0o ; identique a Python (calculs qui font le tour)'
expect_runtime_error machine_div_zero_runtime '44' 'Clair runtime error: division by zero'
expect_runtime_error machine_shift_negative_runtime '128' 'Clair runtime error: negative shift count'
expect_runtime_error machine_int_range_runtime '9223372036854775807' 'Clair runtime error: int() argument out of range'
expect_compile_error invalid_machine_mix "operator '+' cannot mix u32 and i32: convert one of them first, e.g. int(x)"
expect_compile_error invalid_machine_literal "integer 256 does not fit in u8"
expect_compile_error invalid_machine_negative "u16 cannot hold the negative integer -1"
expect_compile_error invalid_bit_float "operator '&' works on integers (int, u8 ... u64, i8 ... i32), got float and int"
expect_compile_error invalid_machine_narrow "cannot initialize u8 'petit' with int"
expect_compile_error invalid_number_literal "invalid number 0x1G"
expect_compile_error invalid_leading_zero "invalid number 007"

./build/clair tests/matmul_blocks.cl -o build/test_matmul_blocks
out_matmul_blocks="$(./build/test_matmul_blocks)"
expected_matmul_blocks="$(printf '0\n0\n0\n0\n0\n0\n0\n0\n5.59825743440233\n0')"
[ "$out_matmul_blocks" = "$expected_matmul_blocks" ]
grep -Fq 'if (clair_rt_matmul_ok(a, b, c, a->rows, b->cols, a->cols)) {' build/test_matmul_blocks.generated.c
grep -Fq 'if (clair_rt_matmul_list_ok(a, k, b, n, c, n, m, n, k)) {' build/test_matmul_blocks.generated.c
printf '%s\n' 'matmul_blocks.cl : OK -> produit de matrices en blocs, identique au bit pres, repli si c = a'
expect_runtime_error matmul_blocks_bounds '7' 'Clair runtime error: matrix index [0][2] out of bounds for shape 2x2'

./build/clair tests/big_integers.cl -o build/test_big_integers
[ "$(./build/test_big_integers)" = "$(cat tests/big_integers.expected)" ]
grep -Fq 'r = clair_i1024_mul(r, clair_i1024_from_i64(i));' build/test_big_integers.generated.c
printf '%s\n' 'big_integers.cl : OK -> int128, int256, int512, int1024 : 100!, fibonacci(300), 2^1000, classes, conversions'
expect_runtime_error big_overflow_runtime '1' 'Clair runtime error: int128 overflow'
expect_runtime_error big_div_zero_runtime '2' 'Clair runtime error: division by zero'
expect_runtime_error big_narrow_runtime '123456789012345678' 'Clair runtime error: int() argument out of range'
expect_compile_error invalid_int_literal_too_large "integer 9223372036854775808 is too large for int (64 bits): use int128, int256, int512 or int1024"
expect_compile_error invalid_big_literal_too_large "integer 170141183460469231731687303715884105728 is too large for int128"
expect_compile_error invalid_big_float_mix "operator '+' cannot mix int256 and float: convert with float(x)"

./build/clair tests/big_unsigned.cl -o build/test_big_unsigned
[ "$(./build/test_big_unsigned)" = "$(cat tests/big_unsigned.expected)" ]
grep -Fq 'r = clair_u1024_mul(r, clair_u1024_from_i64(i));' build/test_big_unsigned.generated.c
grep -Fq 'clair_u128_print(clair_u128_pow(b, 30));' build/test_big_unsigned.generated.c
printf '%s\n' 'big_unsigned.cl : OK -> uint128 a uint1024 : 160!, 200 parmi 100, generateur 128 bits ; abs, min, max, pow exacts'
expect_runtime_error big_unsigned_negative_runtime '2' 'Clair runtime error: uint128 overflow: negative result'
expect_runtime_error big_unsigned_from_negative_runtime '7' 'Clair runtime error: uint256 cannot hold a negative value'
expect_runtime_error big_pow_negative_runtime '9' 'Clair runtime error: pow() exponent must not be negative'
expect_compile_error invalid_unsigned_negative "uint128 cannot hold the negative integer -5"
expect_compile_error invalid_unsigned_mix "operator '+' cannot mix uint128 and int256: convert one of them with int256(x) or uint256(x)"
expect_compile_error invalid_unsigned_minus "unary '-' is not allowed on uint128 (never negative): convert with int256(x)"

./build/clair tests/matvec_rows.cl -o build/test_matvec_rows
[ "$(./build/test_matvec_rows)" = "$(cat tests/matvec_rows.expected)" ]
[ "$(grep -c 'clair_rt_matvec_f64(.*clair_sums);' build/test_matvec_rows.generated.c)" = "1" ]
[ "$(grep -c 'clair_rt_layer_f64(.*&clair_layer);' build/test_matvec_rows.generated.c)" = "2" ]
printf '%s\n' 'matvec_rows.cl : OK -> couche dense 4 lignes a la fois, identique au bit pres, champs, liste a plat, repli si y = x'
expect_runtime_error matvec_rows_bounds '' 'Clair runtime error: list index 2 out of bounds for length 2'

./build/clair tests/plusieurs_coeurs.cl -o build/test_plusieurs_coeurs
[ "$(./build/test_plusieurs_coeurs)" = "$(cat tests/plusieurs_coeurs.expected)" ]
for threads in 1 2 3 7; do
    [ "$(CLAIR_THREADS=$threads ./build/test_plusieurs_coeurs)" = "$(cat tests/plusieurs_coeurs.expected)" ]
done
[ "$(grep -c 'clair_rt_matvec_f64(.*clair_sums);' build/test_plusieurs_coeurs.generated.c)" = "2" ]
[ "$(grep -c 'clair_rt_layer_f64(.*&clair_layer);' build/test_plusieurs_coeurs.generated.c)" = "2" ]
grep -Fq 'pthread_create(' build/test_plusieurs_coeurs.generated.c
printf '%s\n' "plusieurs_coeurs.cl : OK -> produits et couches sur $(nproc 2>/dev/null || echo ?) coeurs, sigmoide comprise, meme resultat avec 1, 2, 3 ou 7 coeurs"
expect_runtime_error plusieurs_coeurs_bounds '1' 'Clair runtime error: list index 3 out of bounds for length 3'

./build/clair tests/memory_freeing.cl -o build/test_memory_freeing
[ "$(./build/test_memory_freeing)" = "$(cat tests/memory_freeing.expected)" ]
clang -O2 tests/max_memory.c -o build/max_memory
memory_mb="$(./build/max_memory ./build/test_memory_freeing)"
[ "$memory_mb" -lt 200 ]
grep -Fq 'clair_rt_list_free(grads);' build/test_memory_freeing.generated.c
printf '%s\n' "memory_freeing.cl : OK -> listes et matrices rendues pendant le programme ($memory_mb Mo au lieu d'environ 1200)"

./build/clair tests/input_int.cl -o build/test_input_int
out_input_int="$(printf '10\n20\n' | ./build/test_input_int)"
[ "$out_input_int" = "100" ]
printf '%s\n' 'input_int.cl : OK -> lecture int runtime et calcul sur valeurs inconnues a la compilation'

./build/clair tests/input_int_invalid_runtime.cl -o build/test_input_int_invalid_runtime
if printf 'abc\n' | ./build/test_input_int_invalid_runtime >build/input_int_invalid.stdout 2>build/input_int_invalid.stderr; then
    printf '%s\n' 'input_int_invalid_runtime.cl : ECHEC -> entree invalide aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: input_int() expected a valid integer' build/input_int_invalid.stderr
printf '%s\n' 'input_int_invalid_runtime.cl : OK -> entree non entiere refusee a l execution'

if ./build/clair tests/invalid_input_int_arity.cl -o build/test_invalid_input_int_arity >build/invalid_input_int_arity.stdout 2>build/invalid_input_int_arity.stderr; then
    printf '%s\n' 'invalid_input_int_arity.cl : ECHEC -> input_int(1) aurait du etre refuse' >&2
    exit 1
fi
grep -Fq 'input_int() expects 0 arguments, got 1' build/invalid_input_int_arity.stderr
printf '%s\n' 'invalid_input_int_arity.cl : OK -> input_int() exige zero argument'

printf '%s\n' '=================================================='
./build/clair tests/big_numbers_v040.cl -o build/test_big_numbers_v040
[ "$(./build/test_big_numbers_v040 | tail -n 1)" = 'CLAIR V0.40 : TESTS OK' ]
if ./build/clair tests/big_numbers_invalid_v040.cl -o build/test_big_numbers_invalid_v040 >/dev/null 2>&1; then
    echo 'ECHEC : bigint + float doit etre refuse' >&2
    exit 1
fi
printf '%s\n' 'big_numbers_v040.cl : OK -> entiers et flottants dynamiques'

# V0.42 : compile depuis un autre dossier (plus besoin de src/ a cote)
(cd tests && ../build/clair big_numbers_v040.cl -o ../build/test_big_ailleurs) > build/big_ailleurs.log 2>&1
[ "$(./build/test_big_ailleurs | tail -n 1)" = 'CLAIR V0.40 : TESTS OK' ]
./build/clair tests/big_numbers_memory_v040.cl -o build/test_big_numbers_memory_v040
./build/test_big_numbers_memory_v040 | grep -Fq 'RESULTATS EXACTS'
./build/clair tests/grands_nombres_memoire.cl -o build/test_grands_nombres_memoire
[ "$(./build/test_grands_nombres_memoire)" = "$(cat tests/grands_nombres_memoire.expected)" ]
grep -Fq 'return clair_rt_num_keep(&__clair_num_frame, ' build/test_grands_nombres_memoire.generated.c
grep -Fq 'clair_rt_num_track_fields(' build/test_grands_nombres_memoire.generated.c
grep -Fq '__builtin_offsetof(ClairClass_Compte, solde)' build/test_grands_nombres_memoire.generated.c
grep -Fq '__builtin_offsetof(ClairClass_Compte, taux)' build/test_grands_nombres_memoire.generated.c
printf '%s\n' 'grands_nombres_memoire.cl : OK -> recursion, boucles, break, continue, masquage, alias, champs ; identique a Python'
./build/clair tests/grands_nombres_longue_boucle.cl -o build/test_grands_nombres_longue_boucle
clang -std=c11 -O2 tests/memoire_max.c -o build/memoire_max
out_longue="$(./build/memoire_max ./build/test_grands_nombres_longue_boucle 2> build/longue_boucle.kio)"
[ "$out_longue" = "$(printf '%s\n%s' 82818552207703176060326495834588033747 82818552207703176060326495834588033747)" ]
kio_longue="$(cat build/longue_boucle.kio)"
if [ "$kio_longue" -gt 100000 ]; then
    printf '%s\n' "grands_nombres_longue_boucle.cl : ECHEC -> $kio_longue Kio de memoire (moins de 100000 attendus)" >&2
    exit 1
fi
printf '%s\n' "grands_nombres_longue_boucle.cl : OK -> 3 millions de calculs dans une fonction et une methode, $kio_longue Kio au plus"
./build/clair tests/masquage_init.cl -o build/test_masquage_init
[ "$(./build/test_masquage_init)" = "$(cat tests/masquage_init.expected)" ]
printf '%s\n' 'masquage_init.cl : OK -> let x = x + 1 dans un bloc lit le x du dessus'

# V0.43 : str(), format(), boucles triangulaires et a deux bouts
./build/clair tests/texte_nombres.cl -o build/test_texte_nombres
[ "$(./build/test_texte_nombres)" = "$(cat tests/texte_nombres.expected)" ]
printf '%s\n' 'texte_nombres.cl : OK -> str() et format() identiques a Python (et NumPy pour float32)'
./build/clair tests/boucles_bornes.cl -o build/test_boucles_bornes
[ "$(./build/test_boucles_bornes)" = "$(cat tests/boucles_bornes.expected)" ]
grep -Fq 'clair_rt_edge_lo(1, 1, 0, n)' build/test_boucles_bornes.generated.c
grep -Fq 'if (lo >= hi || (lo >= 0 && xs->len <= (size_t)INT64_MAX && hi < (int64_t)xs->len)) {' build/test_boucles_bornes.generated.c
printf '%s\n' 'boucles_bornes.cl : OK -> for j in range(i + 1, n) et while lo < hi verifies une seule fois'
expect_runtime_error triangle_bounds_runtime '' 'Clair runtime error: list index 3 out of bounds for length 3'
expect_runtime_error two_ends_bounds_runtime '1' 'Clair runtime error: list index 3 out of bounds for length 3'
expect_compile_error invalid_format_spec 'format(x, ".3x"): write [width][.precision] then f, e, g or d'
expect_compile_error invalid_format_int_d "format(x, \"d\"): 'd' writes an integer: use .Nf for a float"
expect_compile_error invalid_format_dynamic 'argument 2 of format() must be written in the program'
expect_compile_error invalid_str_list 'str() cannot write list[int]'

# V0.44 : bigint avec int, indices qui montent et descendent prouves une fois
./build/clair tests/bigint_avec_int.cl -o build/test_bigint_avec_int
[ "$(./build/test_bigint_avec_int)" = "$(cat tests/bigint_avec_int.expected)" ]
grep -Fq 'clair_rt_bigint_addmul_int(a, a, (int64_t)(9))' build/test_bigint_avec_int.generated.c
printf '%s\n' 'bigint_avec_int.cl : OK -> a + 1, a * k, 1000000 / a, a + a * 9 en un passage ; identique a Python'
./build/clair tests/indices_prouves.cl -o build/test_indices_prouves
[ "$(./build/test_indices_prouves)" = "$(cat tests/indices_prouves.expected)" ]
grep -Fq '(__int128)k >= 0 && (__int128)k <= ((__int128)n - 1)' build/test_indices_prouves.generated.c
printf '%s\n' 'indices_prouves.cl : OK -> k qui monte et descend, prouve entre 0 et n - 1 avant la boucle'
expect_runtime_error range_bounds_runtime '1\n3\n6' 'Clair runtime error: list index 3 out of bounds for length 3'

# V0.45 : fonctions mathematiques des grands nombres
./build/clair tests/grands_nombres_maths.cl -o build/test_grands_nombres_maths
[ "$(./build/test_grands_nombres_maths)" = "$(cat tests/grands_nombres_maths.expected)" ]
grep -Fq 'clair_rt_bigfloat_sqrt(x)' build/test_grands_nombres_maths.generated.c
grep -Fq 'clair_rt_bigint_powmod(' build/test_grands_nombres_maths.generated.c
printf '%s\n' 'grands_nombres_maths.cl : OK -> sqrt, sin, cos, exp, log, atan2, pow, pi(1000), isqrt, gcd, pow modulaire ; identique a mpmath et Python'
expect_runtime_error bigfloat_domain_runtime '2e0' 'Clair runtime error: math domain error'
expect_runtime_error bigfloat_log_zero_runtime '' 'Clair runtime error: math domain error'
expect_runtime_error isqrt_negative_runtime '' 'Clair runtime error: isqrt() argument must be nonnegative'
expect_runtime_error powmod_zero_runtime '' 'Clair runtime error: pow() 3rd argument cannot be 0'
expect_runtime_error bigint_to_int_runtime '1000000000000000000' 'Clair runtime error: int() argument out of range'
expect_compile_error invalid_bigint_sqrt 'sqrt() of a bigint: convert it first, e.g. sqrt(bigfloat(x, 50))'
expect_compile_error invalid_pi_digits 'pi() expects the number of digits, e.g. pi(50)'
expect_compile_error invalid_bigfloat_mod "operator '%' is not defined for bigfloat"

# V0.43 : les jeux de tests des langages, sorties officielles sur de petites tailles
jeu() {
    local name="$1" size="$2" expected="$3"
    ./build/clair "tools/jeux/$name.cl" -o "build/jeu_$name"
    [ "$(./build/jeu_$name <<< "$size")" = "$(printf '%b' "$expected")" ] ||
        { printf '%s\n' "tools/jeux/$name.cl : ECHEC -> sortie differente pour $size" >&2; exit 1; }
    printf '%s\n' "jeu $name ($size) : OK -> sortie officielle"
}
jeu nbody 1000 '-0.169075164\n-0.169087605'
jeu spectral 100 '1.274219991'
jeu fannkuch 7 '228\nPfannkuchen(7) = 16'
jeu pidigits 27 '3141592653\t:10\n5897932384\t:20\n6264338   \t:27'
jeu nqueen 8 '92'
jeu plbmatmul 100 '-9.335833'

# V0.46 : lire l'entree, ecrire des octets, [x] * n
./build/clair tests/entree_sortie.cl -o build/test_entree_sortie
printf 'Guillaume\n7\nligne a\r\nligne b\n\nfin' > build/entree_sortie.txt   # fin de ligne Windows, pas de fin a la fin
./build/test_entree_sortie < build/entree_sortie.txt > build/entree_sortie.out
cmp -s build/entree_sortie.out tests/entree_sortie.expected
grep -Fq 'clair_rt_read_lines(NULL)' build/test_entree_sortie.generated.c
grep -Fq 'clair_rt_affine_ok(comp, 255, 0' build/test_entree_sortie.generated.c
printf '%s\n' 'entree_sortie.cl : OK -> input, read_lines, write, octets, encode, decode, ord, chr, [x] * n, fichiers ; identique a Python'
expect_runtime_error input_eof_runtime 'avant' 'Clair runtime error: EOF when reading a line'
expect_runtime_error decode_zero_runtime '3' 'Clair runtime error: decode(): a 0 byte cannot be in a string'
expect_runtime_error ord_runtime '233' 'Clair runtime error: ord() expected a character, but string of length 2 found'
expect_runtime_error chr_runtime '\xf4\x8f\xbf\xbf' 'Clair runtime error: chr() arg not in range(0x110000)'
expect_runtime_error write_bytes_runtime 'abc' 'Clair runtime error: write(): 5 bytes asked from a list of length 3'
expect_runtime_error read_text_zero_runtime '6' "Clair runtime error: read_text(): a 0 byte is not text, use read_bytes() for 'build/zero.bin'"
expect_compile_error invalid_write_int 'argument 1 of write() expects a string or a list[u8], got int'
expect_compile_error invalid_decode_string 'argument 1 of decode() expects a list[u8], got string'
expect_compile_error invalid_list_repeat_float 'a list can only be repeated by an int (xs * n), got float'
expect_compile_error invalid_write_count_string 'argument 1 of write() expects a list[u8] (write(b, n) writes its first n bytes), got string'

# fasta ecrit des millions de lettres, reverse-complement les relit : sommes de controle
./build/clair tools/jeux/fasta.cl -o build/jeu_fasta
./build/clair tools/jeux/revcomp.cl -o build/jeu_revcomp
[ "$(./build/jeu_fasta <<< 1000 | md5sum | cut -c1-32)" = '60cbd78a7793bcc8032ef153b4a37b56' ] ||
    { printf '%s\n' 'tools/jeux/fasta.cl : ECHEC -> sortie differente pour 1000' >&2; exit 1; }
printf '%s\n' 'jeu fasta (1000) : OK -> sortie de l algorithme officiel'
[ "$(./build/jeu_fasta <<< 1000 | ./build/jeu_revcomp | md5sum | cut -c1-32)" = 'c071aa7e007a9770b2fb4304f55a17e5' ] ||
    { printf '%s\n' 'tools/jeux/revcomp.cl : ECHEC -> sortie differente pour fasta 1000' >&2; exit 1; }
printf '%s\n' 'jeu revcomp (fasta 1000) : OK -> sortie de l algorithme officiel'

# V0.47 : import, from ... import, as ; les modules math, phys, astro et des fichiers .cl
./build/clair tests/modules/importe.cl -o build/test_importe 2> build/importe.stderr
./build/test_importe > build/importe.out
cmp -s build/importe.out tests/modules/importe.expected
grep -Fq 'clair_fn_orbite__mod__periode(' build/test_importe.generated.c
grep -Fq '6.67430e-11' build/test_importe.generated.c
if grep -q 'orbite seul' build/importe.out; then printf '%s\n' 'importe.cl : ECHEC -> le main du module a tourne' >&2; exit 1; fi
printf '%s\n' 'importe.cl : OK -> import math as m, phys, astro, modules qui s importent l un l autre, classe d un module, alias ; identique a Python'
./build/clair tests/modules/orbite.cl -o build/test_orbite_seul
[ "$(./build/test_orbite_seul | head -n 1)" = 'orbite seul' ]
printf '%s\n' 'orbite.cl : OK -> un module lance seul garde son main'
mkdir -p build/modules
expect_compile_error modules/sans_module "import error: no module named 'absent' (no file tests/modules/absent.cl)"
expect_compile_error modules/math_inconnu "import error: module 'math' has no 'sqr' (in main() of math_inconnu.cl, line 3)"
expect_compile_error modules/constante_appelee 'import error: phys.G is a constant, not a function'
expect_compile_error modules/fonction_sans_prefixe "undefined function or class 'periode' (did you mean orbite.periode(...)?)"
expect_compile_error modules/nom_absent "import error: cannot import 'vitesse' from 'orbite'"
expect_compile_error modules/constante_changee 'import error: phys.c: the values of a module cannot be changed'

# V0.48 : les unites verifiees a la compilation, puis oubliees
./build/clair tests/unites.cl -o build/test_unites
[ "$(./build/test_unites)" = "$(cat tests/unites.expected)" ]
grep -Fq 'double dv1 = ' build/test_unites.generated.c
printf '%s\n' 'unites.cl : OK -> transfert de Hohmann Terre-Mars, km convertis, N, J, W, deg, listes ; identique a Python'
expect_compile_error invalid_unite_addition 'unit error: cannot add m and s (in main, line 4)'
expect_compile_error invalid_unite_nombre 'unit error: 1.5 has no unit next to m: write 1.5[m] (in main, line 3)'
expect_compile_error invalid_unite_sinus 'unit error: sin() needs a number without unit, it receives m (in main, line 4)'
expect_compile_error invalid_unite_argument 'unit error: argument 1 of aire (r) is in m, it receives s (in main, line 6)'
expect_compile_error invalid_unite_km 'unit error: cannot add km and m (in main, line 4)'
expect_compile_error invalid_unite_retour 'unit error: the value returned is in m/s, it receives m*s (in vitesse, line 2)'
expect_compile_error invalid_unite_constante 'unit error: x is in m, it receives m^3/(kg*s^2) (in main, line 3)'
expect_compile_error invalid_unite_ecrite 'parse error at 2:17: empty unit'

# V0.49 : chaque erreur dit sa ligne, et son fichier dans un module
expect_compile_error ligne_erreur "semantic error: undefined variable 'y' (line 5)"
expect_compile_error modules/erreur_module "semantic error: undefined variable 'z' (outil_casse.cl, line 5)"
expect_compile_error modules/unite_module 'unit error: cannot add s and m (in orbite_unites.periode, orbite_unites.cl, line 9)'
expect_compile_error modules/nom_absent "import error: cannot import 'vitesse' from 'orbite' (in nom_absent.cl, line 1)"

# V0.51 : les classes dont les champs ne changent jamais sont des valeurs (structures du C) ;
# les objets qui ne sortent pas de leur fonction sont rangés sur la pile
./build/clair tests/classes_valeur.cl -o build/test_classes_valeur
[ "$(./build/test_classes_valeur)" = "$(cat tests/classes_valeur.expected)" ]
G=build/test_classes_valeur.generated.c
grep -Fq 'ClairClass_Vec p = ((ClairClass_Vec){1.0, 2.0});' $G
grep -Fq 'static ClairClass_Vec clair_fn_Vec__plus(ClairClass_Vec self, ClairClass_Vec o) {' $G
grep -Fq 'ClairClass_Segment seg = ((ClairClass_Segment){((ClairClass_Vec){0.0, 0.0}), ((ClairClass_Vec){4.0, 2.0}), "AB"});' $G
grep -Fq 'ClairClass_Compteur clair_obj_c = {0, 5};' $G
grep -Fq 'ClairClass_Compteur clair_obj_t = {i, 1};' $G
grep -Fq 'ClairClass_Gros clair_obj_gros = {1, 2, 3, 4, 5, 6, 7, 8, 9};' $G
grep -Fq 'ClairClass_Compteur * d = ((ClairClass_Compteur *)clair_rt_object_make(' $G
grep -Fq 'ClairClass_Compteur * e = ((ClairClass_Compteur *)clair_rt_object_make(' $G
grep -Fq 'ClairClass_Compteur * f = ((ClairClass_Compteur *)clair_rt_object_make(' $G
grep -Fq 'ClairClass_Compte * compte = ((ClairClass_Compte *)clair_rt_num_track_fields(' $G
printf '%s\n' 'classes_valeur.cl : OK -> classes valeur, objets sur la pile, alias, champ, valeur rendue, bigint ; identique a Python'
./build/clair tools/jeux/vecteurs.cl -o build/jeu_vecteurs
[ "$(./build/jeu_vecteurs <<< 1000 | md5sum | cut -c1-32)" = '63bfc5b0334eb19debb6db1b63688bd8' ] ||
    { printf '%s\n' 'tools/jeux/vecteurs.cl : ECHEC -> sortie differente du C pour 1000' >&2; exit 1; }
if grep -q 'clair_rt_object_make(&(ClairClass_Vec)' build/jeu_vecteurs.generated.c; then printf '%s\n' 'jeu vecteurs : ECHEC -> des Vec en memoire' >&2; exit 1; fi
printf '%s\n' 'jeu vecteurs (1000) : OK -> meme sortie que le C, aucun objet Vec en memoire'

printf '%s\n' ' TOUS LES TESTS SONT OK'
printf '%s\n' '=================================================='
