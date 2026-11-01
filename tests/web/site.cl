// V0.55 : un site qui essaie tout le module web (tests/run_web.sh l'interroge)
import web

@web.route("/")
def accueil() -> string {
    return web.page("page.html", {"titre": "Accueil", "nom": "<Ada & Grace>", "notes": "12\n15\n9\n", "vide": "", "lien": "<b>gras</b>"})
}

@web.route("/bonjour/<nom>")
def bonjour(nom: string) -> string {
    return "Bonjour " + web.escape(nom)
}

@web.route("/carre/<n>")
def carre(n: int) -> string {
    return str(n * n)
}

@web.route("/carre/special")
def special() -> string {
    return "special"
}

@web.route("/moitie/<x>")
def moitie(x: float) -> string {
    return str(x / 2.0)
}

@web.route("/mot/<m>")
def mot_texte(m: string) -> string {
    return "texte " + m
}

@web.route("/mot/<n>")
def mot_nombre(n: int) -> string {
    return "nombre " + str(n)
}

@web.route("/somme/<a>/<b>")
def somme(b: int, a: int) -> string {
    return str(a - b)
}

@web.route("/formulaire", "GET POST")
def formulaire(req: Request) -> Response {
    if req.method == "POST" {
        web.add_line("livre", req.field("nom", "?") + " : " + req.field("message", ""))
        return web.redirect("/livre")
    }
    return web.html("<form method=\"post\"><input name=\"nom\"><input name=\"message\"></form>")
}

@web.route("/livre")
def livre() -> string {
    let html: string = "<ul>"
    for l in web.lines("livre") {
        html = html + "<li>" + web.escape(l) + "</li>"
    }
    return html + "</ul>"
}

@web.route("/recherche")
def recherche(req: Request) -> string {
    return "q=" + req.arg("q", "(rien)") + " page=" + req.arg("page", "1")
}

@web.route("/api/info", "GET POST")
def info(req: Request) -> Response {
    return web.json("{\"methode\": " + web.json_string(req.method) + ", \"agent\": " + web.json_string(req.header("User-Agent")) + ", \"corps\": " + web.json_string(req.body) + "}")
}

@web.route("/visite")
def visite(req: Request) -> Response {
    let n: int = int(req.cookie("visites", "0")) + 1
    return web.with_cookie(web.text("visite " + str(n)), "visites", str(n))
}

@web.route("/compteur", "POST")
def compteur() -> string {
    let n: int = int(web.load("compteur", "0")) + 1
    web.save("compteur", str(n))
    return str(n)
}

@web.route("/erreur")
def erreur() -> string {
    let l: list[int] = [1, 2]
    return str(l[5])
}

@web.route("/cle")
def cle(req: Request) -> string {
    return req.form["absent"]
}

@web.route("/teapot")
def teapot() -> Response {
    return web.error(418, "Je suis une theiere <vide>")
}

@web.route("/fichier")
def fichier() -> Response {
    return web.file("static/info.txt")
}

@web.route("/envoi", "POST")
def envoi(req: Request) -> string {
    let nom: string = req.field("photo", "")
    let ok: int = web.keep(req.files["photo"], "recu.txt")
    return "recu " + nom + " " + str(ok) + " " + req.field("legende", "") + " [" + read_text("recu.txt") + "]"
}

@web.route("/statut", "PUT")
def statut() -> Response {
    return web.with_status(web.text("cree"), 201)
}

@web.route("/casse")
def casse() -> string {
    return web.page("casse.html", {})
}

@web.route("/boucle/<n>")
def boucle(n: int) -> string {
    let s: string = ""
    for i in range(n) {
        s = s + str(i % 10)
    }
    let mots: list[string] = split("a b c d", " ")
    let d: dict[string, int] = {}
    for m in mots {
        d[m] = len(m) + n
    }
    return s + " " + str(len(mots)) + " " + str(d["a"])
}

def main() -> int {
    web.run(int(input()))
    return 0
}
