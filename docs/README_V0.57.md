# Clair V0.57

## Le module web protégé comme Flask, et un peu plus

```text
import web

@web.route("/connexion", "POST")
def connexion(req: Request) -> Response {
    if not web.check_password(web.load("mot_de_passe", ""), req.field("mot", "")) {
        return web.error(401, "mauvais mot de passe")
    }
    let s: dict[string, string] = req.session
    s["nom"] = req.field("nom", "")
    return web.with_session(web.redirect("/"), s)
}

@web.route("/")
def accueil(req: Request) -> string {
    return "Bonjour " + web.escape(req.session_value("nom", "inconnu"))
}

@web.route("/deconnexion", "POST")
def deconnexion() -> Response {
    return web.end_session(web.redirect("/"))
}
```

### Les sessions signées

Comme la `session` de Flask : les valeurs sont gardées par le navigateur dans un cookie, avec une
signature (HMAC-SHA256). Le navigateur peut lire le cookie, mais pas le changer : une valeur
changée, une date changée, une signature fausse, et la session est vide, comme si personne n'était
connecté.

- `req.session` : les valeurs reçues (`dict[string, string]`), `req.session_value("nom", "")` : une
  valeur.
- `web.with_session(r, valeurs)` : la réponse garde ces valeurs dans la session, pour 31 jours.
- `web.end_session(r)` : la session est effacée (déconnexion).
- Le cookie est `HttpOnly` (le JavaScript de la page ne peut pas le lire) et `SameSite=Lax` (un
  autre site ne peut pas l'envoyer avec un formulaire). Une session tient en 3 Ko : on y met un nom,
  et le reste dans `data/`.
- La clé secrète est faite au hasard la première fois et gardée dans `data/.secret_key`, lisible par
  toi seul : les sessions restent valables quand le serveur redémarre. `CLAIR_SECRET_KEY` la donne
  à la place (par exemple pour plusieurs copies d'un serveur).

### Les mots de passe

- `web.hash_password(mot)` : le mot de passe salé puis haché 600 000 fois (PBKDF2-SHA256), écrit
  comme Werkzeug (`pbkdf2:sha256:600000$sel$...`). C'est ça qu'on garde, jamais le mot de passe.
- `web.check_password(hache, mot)` : `true` si c'est le bon. Il lit aussi les mots de passe hachés
  par Flask/Werkzeug en `pbkdf2:sha256`. La comparaison prend toujours le même temps.
- Une vérification prend environ une demi-seconde : c'est voulu, pour qu'un voleur de la liste des
  mots de passe hachés ne puisse pas en essayer des milliards.
- `web.random_token(16)` : 16 octets au hasard, en hexadécimal (un nom que personne ne peut deviner).

### Les protections automatiques

Rien à écrire, elles sont toujours là :

| | |
|---|---|
| **CSRF** | un formulaire ou un script d'un autre site ne peut pas envoyer de `POST`, `PUT`, `DELETE` ou `PATCH` : le navigateur dit d'où vient la requête (`Sec-Fetch-Site`, `Origin`), et le serveur répond 403. Flask demande Flask-WTF et un jeton caché dans chaque formulaire ; ici, aucun jeton. Les programmes qui ne sont pas des navigateurs (`curl`, un autre programme Clair) passent. |
| **Nom du serveur** | `web.run` (seulement sur le téléphone) ne répond qu'aux adresses `127.0.0.1` et `localhost` : une page piégée ne peut pas faire passer son propre nom pour celui du serveur (*DNS rebinding*). `web.run_public` répond à toutes. |
| **En-têtes** | `X-Content-Type-Options: nosniff` (le navigateur ne devine pas le type d'un fichier) et `X-Frame-Options: SAMEORIGIN` (un autre site ne peut pas montrer tes pages dans un cadre) ; une route peut les changer avec `web.with_header`. |
| **Déjà là (V0.55)** | les valeurs protégées dans les modèles (`{{ x }}`), rien hors de `static/` et `templates/`, tailles et durées limitées, pas de détail d'erreur avec `run_public`. |

### Les autres ajouts

- `web.forget_cookie(r, "nom")` : efface un cookie.

### Ce que Flask a et pas encore Clair

- HTTPS : il faut le mettre devant (un relais comme Caddy ou nginx), comme pour Flask en production.
- Plusieurs requêtes à la fois : une seule à la fois ; en mode public, un visiteur lent ralentit les
  autres (au plus 10 s).
- Les mots de passe `scrypt` de Werkzeug (son format par défaut depuis la version 3) : seul
  `pbkdf2:sha256` est lu.

## Tests

- `tests/web/site.cl` (dans `tests/run_web.sh`) : une connexion avec un mot de passe haché par
  Python au format de Werkzeug, un mauvais mot de passe (401), la session lue avec d'autres cookies,
  une session au contenu changé, à la date changée, à la signature coupée, fabriquée : toutes
  refusées ; la déconnexion ; `POST` venu d'un autre site (`Sec-Fetch-Site: cross-site`,
  `same-site`, `Origin` d'un autre site) refusé, du même site accepté ; un `GET` d'un autre site
  accepté ; un autre nom de serveur refusé, `localhost` accepté ; les en-têtes de sécurité, et une
  route qui en change un ;
- les réponses des tests de la V0.55 ne changent pas ; la mémoire du serveur ne grandit pas ;
  AddressSanitizer et UBSan sans erreur ;
- à part, avec Python : `web.hash_password` donne ce que `hashlib.pbkdf2_hmac` donne,
  `web.check_password` accepte un mot de passe haché par Python ; une session fabriquée avec la clé
  est acceptée, périmée ou signée avec une autre clé elle est refusée ; la clé `data/.secret_key`
  est en `-rw-------` et les sessions survivent au redémarrage.
