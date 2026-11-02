#!/data/data/com.termux/files/usr/bin/bash
# V0.58 : Clair Studio (tools/studio/studio.cl), interroge comme le ferait l'editeur
# du navigateur : jeton, fichiers, compilation, lancement, entree, arret.
# V0.59 : cache de compilation, verification sans compiler, tests, sources des modules.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
[ -x build/web_client ] || clang -std=c11 -O2 tests/web_client.c -o build/web_client || exit 1
./build/clair tools/studio/studio.cl -o build/studio_test_bin || exit 1
rm -rf build/studio_test build/studio_projets
cp -r tools/studio build/studio_test
mkdir -p build/studio_projets
cd build/studio_test
CLAIR_STUDIO_OUVRIR=0 ../studio_test_bin "$ROOT/build/studio_projets" 0 > studio.out 2> studio.err &
PID=$!
trap 'kill $PID 2> /dev/null' EXIT
PORT=""
for _ in $(seq 1 100); do
    PORT="$(sed -n 's|.*http://127\.0\.0\.1:\([0-9]*\)/$|\1|p' studio.err | head -n 1)"
    [ -n "$PORT" ] && break
    sleep 0.05
done
JETON="$(sed -n 's|.*/?t=\([0-9a-f]*\)$|\1|p' studio.out)"
if [ -z "$PORT" ] || [ -z "$JETON" ]; then
    echo "le Studio ne demarre pas :" >&2
    cat studio.out studio.err >&2
    exit 1
fi
C="../web_client $PORT"
echec() { echo "studio : $1" >&2; exit 1; }
corps() { sed '1,/^$/d'; }
code() { head -n 1 | cut -d' ' -f1; }

[ "$($C GET / | code)" = 403 ] || echec "sans jeton, la page devrait etre refusee"
[ "$($C GET /api/fichiers | code)" = 403 ] || echec "sans jeton, l'API devrait etre refusee"
[ "$($C GET "/?t=mauvais" | code)" = 403 ] || echec "un mauvais jeton devrait etre refuse"
SESSION="$($C GET "/?t=$JETON" | sed -n 's/^set-cookie: \(clair_session=[^;]*\);.*/\1/p')"
[ -n "$SESSION" ] || echec "le jeton ne donne pas de session"
S="Cookie: $SESSION"
$C GET / "$S" | grep -q '<title>Clair Studio</title>' || echec "page de l'editeur"
$C GET /api/fichiers "$S" | corps | grep -q '"nom":"bonjour.cl"' || echec "bonjour.cl n'est pas dans le dossier"

# ecrire, lire, lancer
$C POST /api/ecrire/essai.cl 'corps=def main() -> int {\n    print("un")\n    print(6 * 7)\n    return 0\n}\n' "$S" | corps | grep -q '"ok":true' || echec "ecrire"
[ "$($C GET /api/lire/essai.cl "$S" | corps | head -n 2 | tail -n 1)" = '    print("un")' ] || echec "lire"
ID="$($C POST /api/lancer/essai.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
[ -n "$ID" ] || echec "lancer"
SORTIE=""
for _ in $(seq 1 200); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    SORTIE="$SORTIE$(printf '%s' "$R" | sed -n 's/.*"sortie":"\(.*\)","fini".*/\1/p')"
    printf '%s' "$R" | grep -q '"fini":true' && break
    sleep 0.05
done
printf '%s' "$R" | grep -q '"code":0' || echec "le programme n'a pas fini avec le code 0 : $R"
[ "$SORTIE" = 'un\n42\n' ] || echec "sortie : $SORTIE"
$C GET /api/c/essai.cl "$S" | grep -q 'int main(void)' || echec "le C"

# une erreur de compilation, avec sa ligne
$C POST /api/ecrire/faux.cl 'corps=def main() -> int {\n    let x: int = 3\n    let y: string = x + "a"\n    return 0\n}\n' "$S" > /dev/null
$C POST /api/lancer/faux.cl 'corps=' "$S" | corps | grep -q '"ok":false,"erreurs":".*(line 3)' || echec "erreur de compilation"

# un programme qui demande, puis une boucle sans fin arretee
$C POST /api/ecrire/demande.cl 'corps=def main() -> int {\n    let n: string = input()\n    print("Bonjour " + n)\n    return 0\n}\n' "$S" > /dev/null
ID="$($C POST /api/lancer/demande.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
$C POST "/api/entree/$ID" 'corps=Ada\n' "$S" | corps | grep -q '"ok":true' || echec "entree"
for _ in $(seq 1 200); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    printf '%s' "$R" | grep -q 'Bonjour Ada' && break
    sleep 0.05
done
printf '%s' "$R" | grep -q 'Bonjour Ada' || echec "programme qui demande : $R"
$C POST /api/ecrire/boucle.cl 'corps=def main() -> int {\n    let i: int = 0\n    while true {\n        i += 1\n    }\n    return 0\n}\n' "$S" > /dev/null
ID="$($C POST /api/lancer/boucle.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
sleep 0.3
$C GET "/api/suivre/$ID" "$S" | corps | grep -q '"fini":false' || echec "la boucle devrait tourner"
$C POST "/api/arreter/$ID" 'corps=' "$S" | corps | grep -q '"ok":true' || echec "arreter"
for _ in $(seq 1 100); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    printf '%s' "$R" | grep -q '"fini":true' && break
    sleep 0.05
done
printf '%s' "$R" | grep -q '"code":137' || echec "la boucle arretee : $R"

# V0.59 : pas de recompilation quand rien n'a change ; le meme programme modifie est recompile
$C POST /api/ecrire/vite.cl 'corps=def main() -> int {\n    print("v1")\n    return 0\n}\n' "$S" > /dev/null
$C POST /api/lancer/vite.cl 'corps=' "$S" | corps | grep -q '"cache":false' || echec "premiere compilation"
$C POST /api/lancer/vite.cl 'corps=' "$S" | corps | grep -q '"cache":true' || echec "le meme programme devrait venir du cache"
$C POST /api/ecrire/vite.cl 'corps=def main() -> int {\n    print("v2")\n    return 0\n}\n' "$S" > /dev/null
ID="$($C POST /api/lancer/vite.cl 'corps=' "$S" | corps | grep '"cache":false' | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
[ -n "$ID" ] || echec "un programme modifie devrait etre recompile"
for _ in $(seq 1 200); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    printf '%s' "$R" | grep -q 'v2' && break
    sleep 0.05
done
printf '%s' "$R" | grep -q 'v2' || echec "le programme recompile : $R"

# V0.59 : les erreurs sans compiler (clair --check)
$C POST /api/verifier/vite.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "verifier un programme juste"
$C POST /api/verifier/faux.cl 'corps=' "$S" | corps | grep -q '"ok":false,"erreurs":".*(line 3)' || echec "verifier un programme faux"

# V0.59 : les tests du fichier
$C POST /api/ecrire/teste.cl 'corps=def carre(x: int) -> int {\n    return x * x\n}\n\ntest "carre" {\n    assert_equal(carre(3), 9)\n}\n\ntest "faux" {\n    assert_equal(carre(2), 5)\n}\n' "$S" > /dev/null
ID="$($C POST /api/tester/teste.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
[ -n "$ID" ] || echec "tester"
SORTIE=""
for _ in $(seq 1 200); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    SORTIE="$SORTIE$(printf '%s' "$R" | sed -n 's/.*"sortie":"\(.*\)","fini".*/\1/p')"
    printf '%s' "$R" | grep -q '"fini":true' && break
    sleep 0.05
done
printf '%s' "$R" | grep -q '"code":1' || echec "des tests rates : code 1 : $R"
printf '%s' "$SORTIE" | grep -q 'test carre ... ok' || echec "tests : $SORTIE"
printf '%s' "$SORTIE" | grep -q 'got 4, expected 5 (line 10)' || echec "tests : $SORTIE"
printf '%s' "$SORTIE" | grep -q 'test result: FAILED. 1 passed; 1 failed' || echec "tests : $SORTIE"
$C POST /api/tester/vite.cl 'corps=' "$S" | corps | grep -q '"ok":false,"erreurs":".*has no test' || echec "un fichier sans test"

# V0.59 : les modules pour l'autocompletion
$C GET /api/module/os "$S" | grep -q '^def run(command: list\[string\]) -> RunResult {' || echec "la source du module os"
[ "$($C GET /api/module/secret "$S" | code)" = 404 ] || echec "un module qui n'existe pas"
[ "$($C GET /api/module/os | code)" = 403 ] || echec "la source d'un module sans jeton"

# renommer, supprimer, et rien en dehors du dossier
$C POST /api/renommer/essai.cl/autre.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "renommer"
$C POST /api/renommer/autre.cl/bonjour.cl 'corps=' "$S" | corps | grep -q '"ok":false' || echec "renommer sur un fichier qui existe"
$C POST /api/supprimer/autre.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "supprimer"
[ ! -e "$ROOT/build/studio_projets/autre.cl" ] || echec "autre.cl devrait etre supprime"
[ "$($C GET /api/lire/..%2F..%2Fsrc%2Fmain.c "$S" | code)" = 404 ] || echec "un fichier hors du dossier"
[ "$($C GET /api/lire/.secret "$S" | code)" = 403 ] || echec "un fichier cache"
[ "$($C POST /api/ecrire/pirate.cl 'corps=x' "$S" 'Sec-Fetch-Site: cross-site' | code)" = 403 ] || echec "une page d'un autre site"
kill $PID 2> /dev/null
wait $PID 2> /dev/null
echo "studio : OK"
