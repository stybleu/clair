# Clair V0.55

## Un serveur web, aussi simple que Flask

```text
import web

@web.route("/")
def accueil() -> string {
    return "<h1>Bonjour</h1>"
}

@web.route("/bonjour/<nom>")
def bonjour(nom: string) -> string {
    return "<p>Bonjour " + web.escape(nom) + "</p>"
}

@web.route("/carre/<n>")
def carre(n: int) -> string {
    return str(n * n)
}

def main() -> int {
    web.run(8080)
    return 0
}
```

```text
clair site.cl -o site && ./site
termux-open-url http://127.0.0.1:8080/
```

Le serveur est écrit en C et copié dans le programme : rien à installer, pas de bibliothèque à
ajouter. Il tourne sur le téléphone, dans Termux, et le navigateur l'ouvre à
`http://127.0.0.1:8080/`. `web.run_public(8080)` le montre aussi aux autres appareils du même Wi-Fi
(leurs adresses sont affichées au démarrage).

### Les routes

`@web.route(chemin)` ou `@web.route(chemin, "GET POST")` se met au-dessus d'un `def` : cette fonction
répond quand le navigateur demande ce chemin.

- Une partie du chemin écrite `<nom>` est donnée au paramètre `nom` de la fonction. Son type dit ce
  qu'elle accepte : `string`, `int` ou `float`. `/carre/abc` n'arrive pas à `carre(n: int)` : c'est
  une page 404, comme avec Flask.
- Un paramètre de type `Request` reçoit la requête.
- La fonction rend un `string` (une page HTML) ou un `Response`.
- Les méthodes : `GET` (par défaut, et `HEAD` avec), `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`.
  Un chemin connu demandé avec une autre méthode répond 405, avec les méthodes permises.
- Le plus précis passe d'abord : `/carre/special` avant `/carre/<n>`, un `int` avant un `float`
  avant un `string`.
- On peut aussi écrire `from web import route, run` puis `@route("/")`.

**Tout est vérifié à la compilation**, ce que Flask ne fait pas :

```text
web error: f() has no parameter x for <x> of "/a/<x>" (line 3)
web error: the parameter x of f() is not in the path "/a": add <x>, or give it the type Request (line 3)
web error: the parameter x of f() comes from the path: it is a string, an int or a float, not list[int] (line 3)
web error: f() answers a request: it gives a string (a page) or a Response, not int (line 3)
web error: POST /a/<y> is already answered by f() of line 3 (line 8)
web error: unknown method 'FETCH' in @web.route: GET, POST, PUT, DELETE, PATCH or OPTIONS (line 3)
```

### La requête

```text
@web.route("/contact", "GET POST")
def contact(req: Request) -> Response {
    if req.method == "POST" {
        web.add_line("messages", req.field("nom", "?") + " : " + req.field("texte", ""))
        return web.redirect("/merci")
    }
    return web.html(web.page("contact.html", {}))
}
```

| | |
|---|---|
| `req.method`, `req.path`, `req.body`, `req.ip` | la méthode, le chemin, le corps, l'adresse du client |
| `req.arg("q", "")` | `?q=...` dans l'adresse (le second texte : quand il n'y est pas) |
| `req.field("nom", "")` | un champ d'un formulaire envoyé |
| `req.cookie("nom", "")`, `req.header("User-Agent")` | un cookie, un en-tête |
| `req.args`, `req.form`, `req.cookies`, `req.headers` | les mêmes, en `dict[string, string]` |
| `req.files["photo"]` | un fichier envoyé par un formulaire : `web.keep(req.files["photo"], "photo.jpg")` le garde |

### La réponse

| | |
|---|---|
| `"<h1>...</h1>"` | une page HTML (200) |
| `web.html(s)`, `web.text(s)`, `web.json(s)` | une page, du texte, du JSON |
| `web.redirect("/ailleurs")` | aller à une autre adresse (302) |
| `web.error(404, "pas ici")` | une page d'erreur |
| `web.file("photo.jpg")` | un fichier, son type selon son extension |
| `web.with_status(r, 201)`, `web.with_header(r, nom, valeur)`, `web.with_cookie(r, nom, valeur)` | la même réponse, changée |
| `web.escape(s)`, `web.quote(s)`, `web.json_string(s)` | un texte écrit pour HTML, pour une adresse, pour JSON |

### Les modèles de pages (templates/)

`web.page("page.html", {"nom": nom})` lit `templates/page.html` et y met les valeurs, comme Jinja
(Flask) ou Django :

```text
{% extends "base.html" %}
{% block contenu %}
<p>Bonjour {{ nom }}</p>                      la valeur, protegee : < devient &lt;
{{ code|safe }}                               la valeur telle quelle
{% if notes %}...{% elif x %}...{% else %}...{% endif %}
{% for n in notes %}<li>{{ n }}</li>{% endfor %}      une ligne de la valeur, un tour
{% include "menu.html" %}
{# un commentaire #}
{% endblock %}
```

`extends` prend la page de base et remplace ses `{% block %}`. Le fichier est relu à chaque fois :
une page changée se voit sans relancer le serveur.

### Les fichiers (static/), PHP, les données (data/)

- Les fichiers de `static/` sont servis tels quels : `static/style.css` à `/static/style.css` et
  à `/style.css`, `static/doc/index.html` à `/doc`. Rien en dehors de `static/`, aucun fichier caché.
- **Les pages PHP** de `static/` (`static/info.php`) sont données à PHP (`pkg install php`) :
  `php-cgi` s'il est là, sinon `php`. `$_GET`, `$_POST`, `$_COOKIE` et `$_SERVER` marchent ; avec
  `php` seul, `header()` et `setcookie()` ne font rien (il faut `php-cgi`).
- `web.load("compteur", "0")`, `web.save("compteur", "5")`, `web.add_line("livre", texte)`,
  `web.lines("livre")` : des textes gardés dans `data/` entre deux lancements.

### Quand une route a une erreur

Une erreur pendant une requête (un index hors de la liste, une clé absente d'un dictionnaire, un
modèle mal écrit...) n'arrête pas le serveur : le navigateur reçoit une page 500 avec le message et
sa ligne, le terminal aussi, et la requête suivante est servie. Avec `web.run_public`, la page 500
ne montre pas le message aux autres appareils.

### La mémoire

Tout ce qu'une requête fabrique (textes, objets, listes, dictionnaires) est rendu à la fin de la
requête : Clair n'a pas de variable globale, rien ne peut la garder. Le serveur reste dans la même
mémoire quel que soit le nombre de requêtes : 2,4 Mo avant et après 20 000 requêtes.

### Comment c'est fait

- Le module `web` est écrit en Clair (`src/clair_web.cl`) et intégré au compilateur ; sa partie C
  (`src/clair_web_rt.c` : le réseau, HTTP, les fichiers, PHP, les modèles) est copiée dans le C des
  programmes qui font `import web`. Les autres programmes ne changent pas : leur C est identique à
  la V0.54.1.
- `src/web.c` lit les `@web.route`, les vérifie, et écrit en Clair la fonction qui répond aux
  requêtes. Le `@` avant un `def` est nouveau dans la langue (il ne sert qu'aux routes).
- Une requête à la fois : c'est fait pour un site perso, un outil, une API sur le téléphone.

## Exemple

`exemples/web/` : un carnet de notes (accueil avec formulaire, page de base avec `extends`, CSS,
calcul montré par un modèle, API JSON, page PHP).

```text
cd ~/clair/exemples/web && ../../build/clair app.cl -o app && ./app
```

## Tests

- `tests/web/site.cl`, interrogé par `tests/run_web.sh` avec `tests/web_client.c` : 50 requêtes
  comparées aux réponses attendues (routes, `int`, `float`, `string`, ordre des routes, modèles avec
  `extends`, `include`, `if`, `elif`, `for`, formulaires, JSON, cookies, `data/`, fichier envoyé,
  405, 418, trois erreurs 500 suivies d'une requête servie, fichiers de `static/`, `HEAD`, fichier
  caché et `..` refusés, requêtes mal faites : 400, 413, 501, 505) ; la mémoire du serveur ne grandit
  pas sur 20 000 requêtes ; PHP s'il est installé ; aussi compilé avec `--dce` ;
- sept programmes refusés (paramètre absent, chemin sans paramètre, mauvais type, mauvaise
  valeur rendue, route en double, `@` sans `import web`, méthode inconnue) ;
- 500 programmes tirés au hasard (2 à 8 routes, parties fixes et `<x>` de types différents,
  méthodes) et 20 000 requêtes (nombres, textes, accents, `%41`, débordements) : code, corps et
  en-tête `Allow` comparés à un modèle des règles écrit en Python ;
- le site entier sous AddressSanitizer et UBSan : aucune erreur ;
- le C des 143 programmes de test et de jeux est identique à la V0.54.1.
