// Un petit carnet de notes sur le telephone, avec le module web (V0.55).
//   ~/clair/build/clair app.cl -o app && ./app
// puis, dans le navigateur : http://127.0.0.1:8080/
import web

// la page d'accueil : les notes et un formulaire
@web.route("/")
def accueil(req: Request) -> Response {
    let notes: string = ""
    for n in web.lines("notes") {
        notes = notes + n + "\n"
    }
    let visites: int = int(req.cookie("visites", "0")) + 1
    let page: string = web.page("accueil.html", {"notes": notes, "visites": str(visites)})
    return web.with_cookie(web.html(page), "visites", str(visites))
}

// le formulaire envoie ici (POST), puis on revient a l'accueil
@web.route("/ajouter", "POST")
def ajouter(req: Request) -> Response {
    let texte: string = strip(req.field("texte", ""))
    if texte != "" {
        web.add_line("notes", texte)
    }
    return web.redirect("/")
}

// /carres/12 : un calcul fait par Clair, montre avec un modele
@web.route("/carres/<n>")
def carres(n: int) -> Response {
    if n < 1 or n > 1000 {
        return web.error(400, "choisis un nombre de 1 a 1000")
    }
    let lignes: string = ""
    for i in range(1, n + 1) {
        lignes = lignes + str(i) + " x " + str(i) + " = " + str(i * i) + "\n"
    }
    return web.html(web.page("carres.html", {"n": str(n), "lignes": lignes}))
}

// les notes en JSON, pour un autre programme
@web.route("/api/notes")
def api_notes() -> Response {
    let json: string = "["
    let premier: bool = true
    for n in web.lines("notes") {
        if not premier {
            json = json + ", "
        }
        json = json + web.json_string(n)
        premier = false
    }
    return web.json(json + "]")
}

def main() -> int {
    web.run(8080)
    return 0
}
