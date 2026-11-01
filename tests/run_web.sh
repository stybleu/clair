#!/data/data/com.termux/files/usr/bin/bash
# V0.55 : le module web.  Lance tests/web/site.cl (un serveur), l'interroge avec
# build/web_client et compare les reponses a tests/web/reponses.expected.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

clang -std=c11 -O2 tests/web_client.c -o build/web_client || exit 1
rm -rf build/web_site
cp -r tests/web build/web_site
./build/clair build/web_site/site.cl -o build/web_site/site || exit 1

cd build/web_site
echo 0 | ./site > serveur.out 2> serveur.err &
PID=$!
trap 'kill $PID 2> /dev/null' EXIT
PORT=""
for _ in $(seq 1 100); do
    PORT="$(sed -n 's|.*http://127\.0\.0\.1:\([0-9]*\)/.*|\1|p' serveur.err | head -n 1)"
    [ -n "$PORT" ] && break
    sleep 0.05
done
if [ -z "$PORT" ]; then
    echo "le serveur ne demarre pas :" >&2
    cat serveur.err >&2
    exit 1
fi
C="../web_client $PORT"
MP='--LIMITE\r\nContent-Disposition: form-data; name="legende"\r\n\r\nma photo\r\n--LIMITE\r\nContent-Disposition: form-data; name="photo"; filename="a.txt"\r\nContent-Type: text/plain\r\n\r\ncontenu\r\ndu fichier\r\n--LIMITE--\r\n'
{
    echo '### pages et parametres'
    $C GET /
    $C GET '/bonjour/Ada%20%3Cb%3E'
    $C GET /carre/12
    $C GET /carre/abc
    $C GET /carre/special
    $C GET /moitie/3
    $C GET /mot/42
    $C GET /mot/abc
    $C GET /somme/10/3
    $C GET /somme/10/3/
    $C GET '/recherche?q=clair+web&x=%C3%A9t%C3%A9'
    $C GET /recherche
    echo '### formulaires, donnees, cookies'
    $C GET /formulaire
    $C POST /formulaire 'corps=nom=Ada&message=Bonjour+%C3%A0+tous' 'Content-Type: application/x-www-form-urlencoded'
    $C POST /formulaire 'corps=nom=Grace&message=<script>' 'Content-Type: application/x-www-form-urlencoded'
    $C GET /livre
    $C POST /api/info 'corps={"a": 1}' 'User-Agent: Testeur "1"' 'Content-Type: application/json'
    $C GET /visite 'Cookie: autre=1; visites=4'
    $C GET /visite
    $C POST /compteur
    $C POST /compteur
    $C GET /compteur
    $C POST /envoi "corps=$MP" 'Content-Type: multipart/form-data; boundary=LIMITE'
    $C PUT /statut
    echo '### erreurs (le serveur continue)'
    $C GET /erreur
    $C GET /cle
    $C GET /teapot
    $C GET /casse
    $C GET /carre/7
    echo '### fichiers de static/'
    $C GET /style.css
    $C GET /static/style.css
    $C HEAD /static/style.css
    $C GET /doc
    $C GET /fichier
    $C GET /.cache
    $C GET /static/../site.cl
    $C POST /style.css
    echo '### requetes mal faites'
    $C raw 'n importe quoi\r\n\r\n'
    $C raw 'GET / HTTP/2.0\r\n\r\n'
    $C raw 'GET /carre/5 HTTP/1.0\r\n\r\n'
    $C raw 'BREW / HTTP/1.1\r\n\r\n'
    $C raw 'GET /%00 HTTP/1.1\r\n\r\n'
    $C raw 'POST /compteur HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n'
    $C raw 'POST /compteur HTTP/1.1\r\nContent-Length: 99999999999\r\n\r\n'
    echo '### memoire'
    $C repete 300 /boucle/40
    $C GET /boucle/12
} > ../web_reponses.txt 2>&1
cd "$ROOT"
if ! diff tests/web/reponses.expected build/web_reponses.txt > build/web_diff.txt; then
    echo "web : les reponses different de tests/web/reponses.expected :" >&2
    head -n 20 build/web_diff.txt >&2
    exit 1
fi
[ "$(cat build/web_site/recu.txt)" = "$(printf 'contenu\r\ndu fichier')" ] || { echo "web : fichier envoye mal recu" >&2; exit 1; }
[ "$(cat build/web_site/data/compteur)" = 2 ] || { echo "web : data/compteur faux" >&2; exit 1; }

# la memoire du serveur ne grandit pas avec les requetes
cd build/web_site
kio() { sed -n 's/^VmRSS:[^0-9]*\([0-9]*\).*/\1/p' /proc/$PID/status; }
../web_client "$PORT" repete 2000 /boucle/200 > /dev/null
AVANT="$(kio)"
../web_client "$PORT" repete 20000 /boucle/200 > /dev/null
APRES="$(kio)"
cd "$ROOT"
if [ $((APRES - AVANT)) -gt 2000 ]; then
    echo "web : la memoire du serveur grandit : $AVANT Kio puis $APRES Kio apres 20000 requetes" >&2
    exit 1
fi

# PHP : avec php-cgi ou php s'ils sont installes
if command -v php-cgi > /dev/null || command -v php > /dev/null; then
    out_php="$(build/web_client "$PORT" GET '/bonjour.php?nom=Ada%20%26%20Bob')"
    printf '%s\n' "$out_php" | grep -Fq 'PHP dit bonjour a Ada &amp; Bob' || { echo "web : PHP : $out_php" >&2; exit 1; }
    out_php="$(build/web_client "$PORT" POST '/bonjour.php' 'corps=x=42' 'Content-Type: application/x-www-form-urlencoded')"
    printf '%s\n' "$out_php" | grep -Fq 'poste : 42' || { echo "web : PHP POST : $out_php" >&2; exit 1; }
    echo "web PHP : OK"
else
    build/web_client "$PORT" GET /bonjour.php | grep -Fq 'PHP is not installed (in Termux: pkg install php)' || { echo "web : sans PHP" >&2; exit 1; }
    echo "web PHP : pas de PHP ici, la page le dit (pkg install php)"
fi
kill $PID 2> /dev/null
wait $PID 2> /dev/null
grep -c '^\[' build/web_site/serveur.err | sed 's/^/web : requetes servies : /'
echo "web : OK, memoire $AVANT Kio puis $APRES Kio apres 20000 requetes de plus"
