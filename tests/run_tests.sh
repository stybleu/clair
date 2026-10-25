#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

printf '%s\n' '=================================================='
printf '%s\n' ' CLAIR V0.29 - BUILD + TESTS'
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
    printf '%s\n' 'list_negative_index.cl : ECHEC -> index negatif aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: list index -1 out of bounds for length 1' build/list_negative_index.stderr
printf '%s\n' 'list_negative_index.cl : OK -> index negatif refuse a l execution'

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
grep -Fq 'list element type must be int, float, bool or string' build/invalid_list_type.stderr
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
    printf '%s\n' 'list_set_negative_index.cl : ECHEC -> ecriture index negatif aurait du echouer' >&2
    exit 1
fi
grep -Fq 'Clair runtime error: list index -1 out of bounds for length 1' build/list_set_negative_index.stderr
printf '%s\n' 'list_set_negative_index.cl : OK -> ecriture index negatif refusee'

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
    if ./build/test_$name >"build/$name.stdout" 2>"build/$name.stderr"; then
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
grep -Fq 's = s + clair_data_self__w[i * clair_cols_self__w + j] * clair_data_x[j];' build/test_field_writes.generated.c
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
expect_runtime_error while_fast_path_bounds '-1\n1' 'Clair runtime error: list index -1 out of bounds for length 3'

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

./build/clair tests/matvec_rows.cl -o build/test_matvec_rows
[ "$(./build/test_matvec_rows)" = "$(cat tests/matvec_rows.expected)" ]
[ "$(grep -c 'for (; clair_row + 4 <= ' build/test_matvec_rows.generated.c)" = "3" ]
printf '%s\n' 'matvec_rows.cl : OK -> couche dense 4 lignes a la fois, identique au bit pres, champs, liste a plat, repli si y = x'
expect_runtime_error matvec_rows_bounds '' 'Clair runtime error: list index 2 out of bounds for length 2'

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
printf '%s\n' ' TOUS LES TESTS SONT OK'
printf '%s\n' '=================================================='
