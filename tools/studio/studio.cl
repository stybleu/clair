// Clair Studio : un IDE pour Clair, ecrit en Clair (V0.58).
// Le serveur est ce programme (modules web et os) ; l'editeur s'ouvre dans le navigateur.
//   sh ~/clair/tools/studio/studio.sh [dossier] [port]
import web
import os

// ---- ce que les routes partagent (Clair n'a pas de variable globale : data/) ----

def dossier() -> string {
    return web.load("dossier", "")
}

def compilateur() -> string {
    return os.env("CLAIR", os.join(os.dirname(os.dirname(os.cwd())), "build/clair"))
}

def binaire(nom: string) -> string {
    return os.join(os.join(os.env("TMPDIR", "/tmp"), "clair_studio"), nom + ".bin")
}

// seule la personne qui a ouvert l'adresse avec le jeton (affichee dans Termux) entre
def autorise(req: Request) -> bool {
    return req.session_value("studio", "") == web.load("jeton", "?")
}

def refus() -> Response {
    return web.error(403, "Ouvre l'adresse affichée dans Termux.")
}

// un fichier du dossier : pas de nom vide, pas de fichier cache ni de ..
def valide(nom: string) -> bool {
    if nom == "" or len(nom) > 120 {
        return false
    }
    return nom[0:1] != "."
}

def oui(b: bool) -> string {
    if b {
        return "true"
    }
    return "false"
}

// ---- les pages ----

@web.route("/")
def accueil(req: Request) -> Response {
    let t: string = req.arg("t", "")
    if t != "" and t == web.load("jeton", "?") {
        let s: dict[string, string] = req.session
        s["studio"] = t
        return web.with_session(web.redirect("/"), s)
    }
    if not autorise(req) {
        return refus()
    }
    return web.html(web.page("index.html", {"dossier": dossier()}))
}

@web.route("/api/fichiers")
def fichiers(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    let d: string = dossier()
    let json: string = "["
    let premier: bool = true
    for n in os.listdir(d) {
        let chemin: string = os.join(d, n)
        if valide(n) and os.is_file(chemin) {
            if not premier {
                json = json + ","
            }
            json = json + "{\"nom\":" + web.json_string(n) + ",\"taille\":" + str(os.size(chemin)) + ",\"date\":" + str(os.modified(chemin)) + "}"
            premier = false
        }
    }
    return web.json(json + "]")
}

@web.route("/api/lire/<nom>")
def lire(req: Request, nom: string) -> Response {
    if not autorise(req) or not valide(nom) {
        return refus()
    }
    let chemin: string = os.join(dossier(), nom)
    if not os.is_file(chemin) {
        return web.error(404, "pas de fichier " + nom)
    }
    return web.text(read_text(chemin))
}

@web.route("/api/ecrire/<nom>", "POST")
def ecrire(req: Request, nom: string) -> Response {
    if not autorise(req) or not valide(nom) {
        return refus()
    }
    write_file(os.join(dossier(), nom), req.body)
    return web.json("{\"ok\":true}")
}

@web.route("/api/supprimer/<nom>", "POST")
def supprimer(req: Request, nom: string) -> Response {
    if not autorise(req) or not valide(nom) {
        return refus()
    }
    let ok: bool = os.remove(os.join(dossier(), nom))
    return web.json("{\"ok\":" + oui(ok) + ",\"erreur\":" + web.json_string(os.error()) + "}")
}

@web.route("/api/renommer/<nom>/<nouveau>", "POST")
def renommer(req: Request, nom: string, nouveau: string) -> Response {
    if not autorise(req) or not valide(nom) or not valide(nouveau) {
        return refus()
    }
    let vers: string = os.join(dossier(), nouveau)
    if os.exists(vers) {
        return web.json("{\"ok\":false,\"erreur\":" + web.json_string(nouveau + " existe déjà") + "}")
    }
    let ok: bool = os.rename(os.join(dossier(), nom), vers)
    return web.json("{\"ok\":" + oui(ok) + ",\"erreur\":" + web.json_string(os.error()) + "}")
}

// compile, puis lance le programme (dans le dossier des fichiers) sans l'attendre
@web.route("/api/lancer/<nom>", "POST")
def lancer(req: Request, nom: string) -> Response {
    if not autorise(req) or not valide(nom) {
        return refus()
    }
    let source: string = os.join(dossier(), nom)
    let sortie: string = binaire(nom)
    os.mkdir(os.dirname(sortie))
    let t0: float = os.clock()
    let c: RunResult = os.run([compilateur(), source, "-o", sortie])
    let duree: string = format(os.clock() - t0, ".2f")
    if c.code != 0 {
        return web.json("{\"ok\":false,\"erreurs\":" + web.json_string(c.err + c.out) + ",\"duree\":" + web.json_string(duree) + "}")
    }
    let p: Process = os.start_merged(["sh", "-c", "cd \"$1\" && exec \"$2\"", "sh", dossier(), sortie])
    if req.body != "" {
        p.write(req.body)
    }
    return web.json("{\"ok\":true,\"id\":" + str(p.id) + ",\"duree\":" + web.json_string(duree) + "}")
}

// ce que le programme a ecrit depuis la derniere fois, et s'il a fini
@web.route("/api/suivre/<id>")
def suivre(req: Request, id: int) -> Response {
    if not autorise(req) {
        return refus()
    }
    let p: Process = os.Process(id)
    let texte: string = p.read()
    let fini: bool = not p.running()
    let code: int = -1
    if fini {
        code = p.wait()
        texte = texte + p.read()
    }
    return web.json("{\"sortie\":" + web.json_string(texte) + ",\"fini\":" + oui(fini) + ",\"code\":" + str(code) + "}")
}

// une ligne tapee pendant que le programme tourne (input())
@web.route("/api/entree/<id>", "POST")
def entree(req: Request, id: int) -> Response {
    if not autorise(req) {
        return refus()
    }
    let p: Process = os.Process(id)
    return web.json("{\"ok\":" + oui(p.write(req.body)) + "}")
}

@web.route("/api/arreter/<id>", "POST")
def arreter(req: Request, id: int) -> Response {
    if not autorise(req) {
        return refus()
    }
    let p: Process = os.Process(id)
    return web.json("{\"ok\":" + oui(p.stop()) + "}")
}

// le C que Clair a ecrit pour ce programme (apres le dernier lancement)
@web.route("/api/c/<nom>")
def code_c(req: Request, nom: string) -> Response {
    if not autorise(req) or not valide(nom) {
        return refus()
    }
    let chemin: string = binaire(nom) + ".generated.c"
    if not os.is_file(chemin) {
        return web.text("// Lance d'abord le programme : le C apparait apres la compilation.\n")
    }
    return web.text(read_text(chemin))
}

def main() -> int {
    let a: list[string] = os.args()
    let d: string = os.join(os.home(), "clair_projets")
    if len(a) > 0 {
        d = a[0]
    }
    if d[0:1] != "/" {
        d = os.join(os.cwd(), d)
    }
    if not os.mkdir(d) {
        print("Clair Studio : " + os.error())
        return 1
    }
    web.save("dossier", d)
    if len(os.listdir(d)) == 0 {
        write_file(os.join(d, "bonjour.cl"), "def main() -> int {\n    let nom: string = \"Clair\"\n    print(\"Bonjour depuis \" + nom)\n    for i in range(1, 6) {\n        print(str(i) + \" au carre = \" + str(i * i))\n    }\n    return 0\n}\n")
    }
    let jeton: string = web.load("jeton", "")
    if jeton == "" {
        jeton = web.random_token(16)
        web.save("jeton", jeton)
    }
    let port: int = 8090
    if len(a) > 1 {
        port = int(a[1])
    }
    let adresse: string = "http://127.0.0.1:" + str(port) + "/?t=" + jeton
    print("Clair Studio : " + adresse)
    print("dossier : " + d)
    if os.which("termux-open-url") != "" and os.env("CLAIR_STUDIO_OUVRIR", "1") == "1" {
        os.start(["sh", "-c", "sleep 1; termux-open-url \"$1\"", "sh", adresse])
    }
    web.run(port)
    return 0
}
