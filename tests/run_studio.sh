#!/data/data/com.termux/files/usr/bin/bash
# V0.58 : Clair Studio (tools/studio/studio.cl), interroge comme le ferait l'editeur
# du navigateur : jeton, fichiers, compilation, lancement, entree, arret.
# V0.59 : cache de compilation, verification sans compiler, tests, sources des modules.
# V0.60 : Git : depot, etat, differences, versions, historique, restaurer, annuler, envoyer, recuperer.
# V0.61 : les figures de plot.show() et les images de plot.image().
# V0.62 : sous-dossiers (explorateur), reglages.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
[ -x build/web_client ] || clang -std=c11 -O2 tests/web_client.c -o build/web_client || exit 1
./build/clair tools/studio/studio.cl -o build/studio_test_bin || exit 1
rm -rf build/studio_test build/studio_projets
cp -r tools/studio build/studio_test
rm -rf build/studio_test/data            # le jeton et les reglages du vrai Studio ne servent pas ici
mkdir -p build/studio_projets
cd build/studio_test
# le dossier des programmes est dans build/, donc dans le depot de Clair : git ne doit pas le voir
GIT_CEILING_DIRECTORIES="$ROOT/build" CLAIR_STUDIO_OUVRIR=0 ../studio_test_bin "$ROOT/build/studio_projets" 0 > studio.out 2> studio.err &
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


# V0.65.1 : creation sans ecrasement.
[ "$($C POST /api/nouveau/nouveau_test.cl 'corps=' | code)" = 403 ] || echec "creation sans session"
$C POST /api/nouveau/nouveau_test.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "creation rapide"
$C GET /api/lire/nouveau_test.cl "$S" | corps | grep -q '^def main() -> int {' || echec "document lisible"
$C POST /api/nouveau/nouveau_test.cl 'corps=' "$S" | corps | grep -q '"ok":false' || echec "ecrasement refuse"
[ "$($C POST /api/nouveau/..%2Finterdit.cl 'corps=' "$S" | code)" = 403 ] || echec "chemin interdit"

# V0.65.2 : sauvegarde CSV.
[ "$($C POST /api/enregistrer-csv/essai.csv 'corps=time,temperature\n2024-01-01,12\n' | code)" = 403 ] || echec "CSV sans session"

$C POST /api/enregistrer-csv/essai.csv 'corps=time,temperature\n2024-01-01,12\n' "$S" | corps | grep -q '"ok":true' || echec "sauvegarde CSV"

[ -f "$ROOT/build/studio_projets/essai.csv" ] || echec "CSV absent"

$C GET /api/lire/essai.csv "$S" | corps | grep -q '^time,temperature' || echec "CSV illisible"

$C POST /api/enregistrer-csv/essai.csv 'corps=remplacement' "$S" | corps | grep -q '"ok":false' || echec "CSV existant ecrase"

[ "$($C POST /api/enregistrer-csv/..%2Fnon.csv 'corps=time,temperature' "$S" | code)" = 403 ] || echec "CSV hors dossier"

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

# V0.71.2 : un programme qui ecrit beaucoup : sa sortie arrive par gros morceaux (jusqu'a 1 Mo par appel), pas
# 64 Ko (avant : 22 appels ici, et une centaine dans la page, qui attendait 150 ms entre deux)
$C POST /api/ecrire/beaucoup.cl 'corps=def main() -> int {\n    for i in range(100000) {\n        print("ligne " + str(i) + " : du texte pour remplir")\n    }\n    return 0\n}\n' "$S" > /dev/null
ID="$($C POST /api/lancer/beaucoup.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
[ -n "$ID" ] || echec "lancer beaucoup.cl"
APPELS=0
: > beaucoup.out
for _ in $(seq 1 400); do
    APPELS=$((APPELS + 1))
    $C GET "/api/suivre/$ID" "$S" | corps > beaucoup.r
    sed -n 's/.*"sortie":"\(.*\)","fini".*/\1/p' beaucoup.r | tr -d '\n' >> beaucoup.out
    grep -q '"fini":true' beaucoup.r && break
done
LIGNES="$(grep -o 'remplir\\n' beaucoup.out | wc -l)"
[ "$LIGNES" -eq 100000 ] || echec "beaucoup de sortie : $LIGNES lignes au lieu de 100000"
[ "$APPELS" -le 12 ] || echec "beaucoup de sortie : $APPELS appels au lieu de 12 au plus (la sortie arrive par trop petits morceaux)"
$C POST /api/supprimer/beaucoup.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "supprimer beaucoup.cl"
rm -f beaucoup.out beaucoup.r

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

# V0.62 : des sous-dossiers : creer, lister, ecrire, lancer (dans son dossier, avec un module a cote), deplacer
$C POST /api/dossier/meteo 'corps=' "$S" | corps | grep -q '"ok":true' || echec "creer un dossier"
$C POST /api/dossier/meteo/donnees 'corps=' "$S" | corps | grep -q '"ok":true' || echec "creer un sous-dossier"
$C POST /api/dossier/meteo 'corps=' "$S" | corps | grep -q '"ok":false' || echec "un dossier qui existe deja"
$C POST /api/ecrire/meteo/outil.cl 'corps=def double(x: int) -> int {\n    return 2 * x\n}\n' "$S" > /dev/null
$C POST /api/ecrire/meteo/releve.txt 'corps=12' "$S" > /dev/null
$C POST /api/ecrire/meteo/station.cl 'corps=import outil\n\ndef main() -> int {\n    print(outil.double(int(strip(read_text("releve.txt")))))\n    return 0\n}\n' "$S" | corps | grep -q '"ok":true' || echec "ecrire dans un sous-dossier"
$C GET /api/fichiers "$S" | corps | grep -q '^\[{"nom":"meteo","dossier":true,' || echec "les dossiers d'abord : $($C GET /api/fichiers "$S" | corps)"
$C GET '/api/fichiers?d=meteo' "$S" | corps | grep -q '^\[{"nom":"donnees","dossier":true,.*{"nom":"outil.cl","dossier":false,.*"nom":"station.cl"' || echec "lister un sous-dossier : $($C GET '/api/fichiers?d=meteo' "$S" | corps)"
[ "$($C GET '/api/fichiers?d=..%2F..' "$S" | code)" = 403 ] || echec "lister hors du dossier"
[ "$($C GET '/api/fichiers?d=meteo%2F.cache' "$S" | code)" = 403 ] || echec "lister un dossier cache"
$C GET /api/lire/meteo/station.cl "$S" | corps | grep -q '^import outil$' || echec "lire dans un sous-dossier"
$C POST /api/verifier/meteo/station.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "verifier dans un sous-dossier"
ID="$($C POST /api/lancer/meteo/station.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
[ -n "$ID" ] || echec "lancer dans un sous-dossier"
for _ in $(seq 1 200); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    printf '%s' "$R" | grep -q '"fini":true' && break
    sleep 0.05
done
printf '%s' "$R" | grep -q '"sortie":"24\\n"' || echec "un programme d'un sous-dossier tourne dans son dossier : $R"
$C POST /api/renommer/meteo/releve.txt 'corps=meteo/donnees/releve.txt' "$S" | corps | grep -q '"ok":true' || echec "deplacer dans un sous-dossier"
[ -f "$ROOT/build/studio_projets/meteo/donnees/releve.txt" ] || echec "le fichier deplace"
$C POST /api/supprimer/meteo/donnees 'corps=' "$S" | corps | grep -q '"ok":false' || echec "un dossier plein ne se supprime pas"
$C POST /api/supprimer/meteo/donnees/releve.txt 'corps=' "$S" > /dev/null
$C POST /api/supprimer/meteo/donnees 'corps=' "$S" | corps | grep -q '"ok":true' || echec "supprimer un dossier vide"
[ "$($C POST /api/dossier/..%2Fdehors 'corps=' "$S" | code)" != 200 ] || echec "un dossier hors du dossier"
[ ! -e "$ROOT/build/dehors" ] || echec "un dossier a ete cree hors du dossier"
[ "$($C POST /api/renommer/meteo/outil.cl 'corps=../outil.cl' "$S" | code)" = 403 ] || echec "deplacer hors du dossier"
# V0.62 : les reglages de l'editeur
$C GET /api/reglages "$S" | corps | grep -qx '{}' || echec "reglages au depart"
$C POST /api/reglages 'corps={"theme":"sombre","taille":16}' "$S" > /dev/null
$C GET /api/reglages "$S" | corps | grep -qx '{"theme":"sombre","taille":16}' || echec "reglages gardes"
[ "$($C POST /api/reglages 'corps=pas du json' "$S" | code)" = 400 ] || echec "des reglages qui ne sont pas du JSON"

# V0.61 : les figures de plot.show() dans la console
$C POST /api/ecrire/courbe.cl 'corps=import plot\n\ndef main() -> int {\n    print("avant")\n    plot.line([0.0, 1.0, 2.0], [0.0, 1.0, 4.0])\n    plot.show()\n    plot.image("dessin.svg")\n    print("apres")\n    return 0\n}\n' "$S" > /dev/null
$C POST /api/ecrire/dessin.svg 'corps=<svg xmlns="http://www.w3.org/2000/svg"/>' "$S" > /dev/null
ID="$($C POST /api/lancer/courbe.cl 'corps=' "$S" | corps | sed -n 's/.*"id":\([0-9]*\).*/\1/p')"
[ -n "$ID" ] || echec "plot : lancer"
SORTIE=""
for _ in $(seq 1 200); do
    R="$($C GET "/api/suivre/$ID" "$S" | corps)"
    SORTIE="$SORTIE$(printf '%s' "$R" | sed -n 's/.*"sortie":"\(.*\)","fini".*/\1/p')"
    printf '%s' "$R" | grep -q '"fini":true' && break
    sleep 0.05
done
FIG="$(printf '%s' "$SORTIE" | sed -n 's/.*\\u001b\[clair:plot \([0-9_]*\.svg\)\].*/\1/p')"
[ -n "$FIG" ] || echec "plot : pas de figure dans la sortie : $SORTIE"
printf '%s' "$SORTIE" | grep -q '^avant\\n\\u001b\[clair:plot [0-9_]*\.svg\]\\n\\u001b\[clair:image dessin\.svg\]\\napres\\n$' || echec "plot : sortie : $SORTIE"
$C GET "/api/figure/$FIG" "$S" | corps | grep -q '^<svg xmlns="http://www.w3.org/2000/svg"' || echec "plot : la figure"
$C GET /api/image/dessin.svg "$S" | grep -qi '^content-type: image/svg+xml' || echec "plot : une image du dossier"
[ "$($C GET /api/figure/..%2F..%2Fx.svg "$S" | code)" = 404 ] || echec "plot : une figure hors du dossier"
[ "$($C GET /api/figure/abc.svg "$S" | code)" = 403 ] || echec "plot : un nom de figure invente"
[ "$($C GET /api/image/courbe.cl "$S" | code)" = 404 ] || echec "plot : un fichier qui n'est pas une image"
[ "$($C GET "/api/figure/$FIG" | code)" = 403 ] || echec "plot : une figure sans jeton"
$C GET /api/module/plot "$S" | grep -q '^def heatmap(values: list\[float\], nx: int, ny: int) -> int {' || echec "plot : la source du module"

# V0.60 : Git dans le dossier des programmes
if command -v git > /dev/null; then
    gj() { $C "$@" "$S" | corps; }
    gj GET /api/git/etat | grep -q '"git":true,"depot":false' || echec "git : pas encore de depot"
    gj POST /api/git/creer 'corps=' | grep -q '"ok":true' || echec "git : creer le depot"
    gj GET /api/git/etat | grep -q '"statut":"## No commits yet on main\\n?? bonjour.cl\\n' || echec "git : etat d'un depot neuf : $(gj GET /api/git/etat)"
    gj GET '/api/git/diff/bonjour.cl?nouveau=1' | grep -q '+def main() -> int {' || echec "git : un fichier nouveau"
    gj POST /api/git/identite 'corps=Ada Lovelace\nada@example.org' | grep -q '"ok":true' || echec "git : identite"
    gj POST /api/git/version 'corps=' | grep -q '"ok":false' || echec "git : une version sans message"
    gj POST /api/git/version 'corps=premiere version' | grep -q '"ok":true' || echec "git : premiere version"
    gj GET /api/git/historique | grep -q 'Ada Lovelace\\u001f.*\\u001fpremiere version' || echec "git : historique : $(gj GET /api/git/historique)"
    V1="$(gj GET /api/git/historique | sed -n 's/.*"sortie":"\([0-9a-f]*\)\\u001f.*/\1/p')"
    [ -n "$V1" ] || echec "git : numero de version"
    $C POST /api/ecrire/teste.cl 'corps=def carre(x: int) -> int {\n    return x * x * 1\n}\n' "$S" > /dev/null
    gj GET /api/git/etat | grep -q '\\n M teste.cl\\n' || echec "git : un fichier modifie"
    gj GET /api/git/diff/teste.cl | grep -q '+    return x \* x \* 1' || echec "git : diff"
    gj GET /api/git/lignes/teste.cl | grep -q '@@ -2,9 +2 @@' || echec "git : lignes changees : $(gj GET /api/git/lignes/teste.cl)"
    gj POST /api/git/version 'corps=carre fois 1' | grep -q '"ok":true' || echec "git : deuxieme version"
    gj GET "/api/git/voir/$V1" | grep -q 'premiere version' || echec "git : voir une version"
    gj POST "/api/git/restaurer/$V1/teste.cl" 'corps=' | grep -q '"ok":true' || echec "git : restaurer"
    [ "$(sed -n 2p "$ROOT/build/studio_projets/teste.cl")" = '    return x * x' ] || echec "git : le fichier restaure"
    gj POST /api/git/annuler/teste.cl 'corps=' | grep -q '"ok":true' || echec "git : annuler"
    [ "$(sed -n 2p "$ROOT/build/studio_projets/teste.cl")" = '    return x * x * 1' ] || echec "git : le fichier annule"
    # envoyer et recuperer, avec un depot distant dans un dossier
    rm -rf "$ROOT/build/studio_distant.git" "$ROOT/build/studio_autre"
    git init -q --bare -b main "$ROOT/build/studio_distant.git"
    gj POST /api/git/distant 'corps=ext::sh -c touch% /tmp/pirate' | grep -q '"ok":false' || echec "git : une adresse ext:: devrait etre refusee"
    gj POST /api/git/distant "corps=$ROOT/build/studio_distant.git" | grep -q '"ok":true' || echec "git : relier"
    gj POST /api/git/envoyer 'corps=' | grep -q '"ok":true' || echec "git : envoyer : $(gj POST /api/git/envoyer 'corps=')"
    git clone -q "$ROOT/build/studio_distant.git" "$ROOT/build/studio_autre"
    printf 'def main() -> int {\n    print("ailleurs")\n    return 0\n}\n' > "$ROOT/build/studio_autre/bonjour.cl"
    git -C "$ROOT/build/studio_autre" -c user.name=B -c user.email=b@example.org commit -q -am "ailleurs"
    git -C "$ROOT/build/studio_autre" push -q
    gj POST /api/git/recuperer 'corps=' | grep -q '"ok":true' || echec "git : recuperer"
    grep -q 'ailleurs' "$ROOT/build/studio_projets/bonjour.cl" || echec "git : le fichier recupere"
    # rien hors du dossier, pas d'option deguisee en fichier ou en version
    [ "$(gj GET /api/git/diff/..%2F..%2Fsrc%2Fmain.c | head -c 1)" != '{' ] || echec "git : un chemin hors du dossier"
    [ "$($C GET /api/git/diff/--output=x "$S" | code)" = 403 ] || echec "git : une option a la place d'un fichier"
    [ "$($C POST /api/git/restaurer/--help/teste.cl 'corps=' "$S" | code)" = 403 ] || echec "git : une option a la place d'une version"
    [ "$($C GET /api/git/etat | code)" = 403 ] || echec "git : sans jeton"
fi

# renommer, supprimer, et rien en dehors du dossier
$C POST /api/renommer/essai.cl 'corps=autre.cl' "$S" | corps | grep -q '"ok":true' || echec "renommer"
$C POST /api/renommer/autre.cl 'corps=bonjour.cl' "$S" | corps | grep -q '"ok":false' || echec "renommer sur un fichier qui existe"
$C POST /api/supprimer/autre.cl 'corps=' "$S" | corps | grep -q '"ok":true' || echec "supprimer"
[ ! -e "$ROOT/build/studio_projets/autre.cl" ] || echec "autre.cl devrait etre supprime"
[ "$($C GET /api/lire/..%2F..%2Fsrc%2Fmain.c "$S" | code)" = 403 ] || echec "un fichier hors du dossier"
[ "$($C GET /api/lire/.secret "$S" | code)" = 403 ] || echec "un fichier cache"
[ "$($C POST /api/ecrire/pirate.cl 'corps=x' "$S" 'Sec-Fetch-Site: cross-site' | code)" = 403 ] || echec "une page d'un autre site"

# V0.65 : choisir et changer de dossier.
[ "$($C GET /api/selection-dossiers | code)" = 403 ] || echec "selection sans session"
$C GET /api/selection-dossiers "$S" | corps | grep -q '"racines":' || echec "emplacements"
$C GET "/api/selection-dossiers?d=$ROOT/build/studio_projets" "$S" | corps | grep -q '"dossiers":' || echec "exploration"
[ "$($C GET '/api/selection-dossiers?d=%2Fetc' "$S" | code)" = 403 ] || echec "navigation interdite"
[ "$($C POST /api/selection-dossier 'corps=/etc' "$S" | code)" = 403 ] || echec "selection interdite"

mkdir -p "$ROOT/build/studio_projets/choix_v065"
$C POST /api/selection-dossier "corps=$ROOT/build/studio_projets/choix_v065" "$S" | corps | grep -q '"ok":true' || echec "changement de dossier"
$C POST /api/ecrire/essai_selection.cl 'corps=def main() -> int {\n    return 0\n}\n' "$S" | corps | grep -q '"ok":true' || echec "ecriture"
[ -f "$ROOT/build/studio_projets/choix_v065/essai_selection.cl" ] || echec "fichier absent"
$C POST /api/selection-dossier "corps=$ROOT/build/studio_projets" "$S" | corps | grep -q '"ok":true' || echec "retour dossier"
[ "$($C GET /api/lire/essai_selection.cl "$S" | code)" = 404 ] || echec "ancien dossier actif"

# V0.65.3 : enregistrer dans un dossier choisi.
mkdir -p "$ROOT/build/studio_projets/export_csv"

$C POST "/api/enregistrer-csv/mesures.csv?d=$ROOT/build/studio_projets/export_csv" 'corps=time,pluie\n2024-01-01,1\n' "$S" | corps | grep -q '"ok":true' || echec "CSV dans un dossier choisi"

[ -f "$ROOT/build/studio_projets/export_csv/mesures.csv" ] || echec "CSV destination absente"

$C POST "/api/enregistrer-csv/mesures.csv?d=$ROOT/build/studio_projets/export_csv" 'corps=remplacement' "$S" | corps | grep -q '"ok":false' || echec "CSV destination ecrase"

[ "$( $C GET /api/fichiers "$S" | code )" = 200 ] || echec "projet modifie par le CSV"

[ "$( $C POST '/api/enregistrer-csv/dehors.csv?d=/etc' 'corps=donnees' "$S" | code )" = 403 ] || echec "CSV dossier interdit"

kill $PID 2> /dev/null
wait $PID 2> /dev/null

# V0.71.6 : au demarrage, le Studio ouvre l'appli Clair Studio (CLAIR_STUDIO_APPLI=1, mis par studio.sh quand
# elle est installee), sinon le navigateur. am et termux-open-url sont imites : rien ne s'ouvre vraiment.
mkdir -p ../ouvrir_bin
for o in am termux-open-url; do
    printf '#!%s\necho "%s $*" >> "%s"\n' "$(command -v sh)" "$o" "$PWD/ouvert.txt" > "../ouvrir_bin/$o"
    chmod +x "../ouvrir_bin/$o"
done
ouvrir() {
    rm -f ouvert.txt
    env PATH="$PWD/../ouvrir_bin:$PATH" GIT_CEILING_DIRECTORIES="$ROOT/build" CLAIR_STUDIO_OUVRIR=1 CLAIR_STUDIO_APPLI="$1" \
        ../studio_test_bin "$ROOT/build/studio_projets" 0 > ouvrir.out 2> ouvrir.err &
    P2=$!
    for _ in $(seq 1 60); do
        [ -s ouvert.txt ] && break
        sleep 0.05
    done
    sleep 0.2
    kill $P2 2> /dev/null
    wait $P2 2> /dev/null
}
ouvrir 1
grep -q "^am start -n clair.studio/.Studio -a android.intent.action.VIEW -d http://127.0.0.1:[0-9]*/?t=$JETON\$" ouvert.txt || echec "ouverture dans l'appli : $(cat ouvert.txt 2> /dev/null)"
ouvrir 0
grep -q "^termux-open-url http://127.0.0.1:[0-9]*/?t=$JETON\$" ouvert.txt || echec "ouverture dans le navigateur : $(cat ouvert.txt 2> /dev/null)"
echo "studio : OK"
