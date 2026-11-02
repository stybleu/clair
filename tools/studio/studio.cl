// Clair Studio : un IDE pour Clair, ecrit en Clair (V0.58 a V0.62).
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

// le dossier de Clair (build/clair est dedans) : les sources des modules sont dans src/
def racine() -> string {
    return os.dirname(os.dirname(compilateur()))
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

// V0.62 : ce que contient un dossier (?d=sous/dossier, sinon le dossier des programmes) : dossiers puis fichiers
@web.route("/api/fichiers")
def fichiers(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    let sous: string = req.arg("d", "")
    if sous != "" and not chemin_valide(sous) {
        return refus()
    }
    let d: string = dossier()
    if sous != "" {
        d = os.join(d, sous)
    }
    if not os.is_dir(d) {
        return web.error(404, "pas de dossier " + sous)
    }
    let json: string = "["
    let premier: bool = true
    for genre in range(0, 2) {
        for n in os.listdir(d) {
            let chemin: string = os.join(d, n)
            let est_dossier: bool = os.is_dir(chemin)
            if valide(n) and ((genre == 0 and est_dossier) or (genre == 1 and os.is_file(chemin))) {
                if not premier {
                    json = json + ","
                }
                json = json + "{\"nom\":" + web.json_string(n) + ",\"dossier\":" + oui(est_dossier) + ",\"taille\":" + str(os.size(chemin)) + ",\"date\":" + str(os.modified(chemin)) + "}"
                premier = false
            }
        }
    }
    return web.json(json + "]")
}

// V0.62 : un nouveau dossier
@web.route("/api/dossier/<path:nom>", "POST")
def nouveau_dossier(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let chemin: string = os.join(dossier(), nom)
    if os.exists(chemin) {
        return web.json("{\"ok\":false,\"erreur\":" + web.json_string(nom + " existe déjà") + "}")
    }
    let ok: bool = os.mkdir(chemin)
    return web.json("{\"ok\":" + oui(ok) + ",\"erreur\":" + web.json_string(os.error()) + "}")
}

// V0.62 : les reglages de l'editeur (theme, taille du texte), gardes dans data/
@web.route("/api/reglages", "GET POST")
def reglages(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    if req.method == "POST" {
        if len(req.body) > 2000 or req.body[0:1] != "{" {
            return web.error(400, "des reglages en JSON")
        }
        web.save("reglages", req.body)
    }
    return web.json(web.load("reglages", "{}"))
}

@web.route("/api/lire/<path:nom>")
def lire(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let chemin: string = os.join(dossier(), nom)
    if not os.is_file(chemin) {
        return web.error(404, "pas de fichier " + nom)
    }
    return web.text(read_text(chemin))
}

@web.route("/api/ecrire/<path:nom>", "POST")
def ecrire(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    write_file(os.join(dossier(), nom), req.body)
    return web.json("{\"ok\":true}")
}

// un fichier, ou un dossier vide
@web.route("/api/supprimer/<path:nom>", "POST")
def supprimer(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let ok: bool = os.remove(os.join(dossier(), nom))
    return web.json("{\"ok\":" + oui(ok) + ",\"erreur\":" + web.json_string(os.error()) + "}")
}

// V0.62 : le nouveau chemin dans le corps de la requete (il peut changer de dossier)
@web.route("/api/renommer/<path:nom>", "POST")
def renommer(req: Request, nom: string) -> Response {
    let nouveau: string = strip(req.body)
    if not autorise(req) or not chemin_valide(nom) or not chemin_valide(nouveau) {
        return refus()
    }
    let vers: string = os.join(dossier(), nouveau)
    if os.exists(vers) {
        return web.json("{\"ok\":false,\"erreur\":" + web.json_string(nouveau + " existe déjà") + "}")
    }
    let ok: bool = os.rename(os.join(dossier(), nom), vers)
    return web.json("{\"ok\":" + oui(ok) + ",\"erreur\":" + web.json_string(os.error()) + "}")
}

// ---- compiler, lancer, tester ----

Class Compilation {
    ok: bool
    erreurs: string
    duree: string
    cache: bool
}

// V0.59 : ce qui decide du programme : le compilateur, puis chaque .cl du dossier
// (les modules importes en font partie). La meme cle : pas besoin de recompiler.
def cle_sources(nom: string, mode: string) -> string {
    let d: string = os.dirname(os.join(dossier(), nom))
    let c: string = compilateur()
    let cle: string = mode + "|" + str(os.modified(c)) + "|" + str(os.size(c)) + "\n"
    for n in os.listdir(d) {
        let chemin: string = os.join(d, n)
        if valide(n) and os.extension(n) == ".cl" and os.is_file(chemin) {
            let texte: string = read_text(chemin)
            cle = cle + n + "|" + str(len(texte)) + "\n" + texte
        }
    }
    return cle
}

// compile nom vers sortie (mode "" ou "--test"), sauf si rien n'a change depuis la derniere fois
def compiler(nom: string, sortie: string, mode: string) -> Compilation {
    os.mkdir(os.dirname(sortie))
    let cle: string = cle_sources(nom, mode)
    let fichier_cle: string = sortie + ".cle"
    if os.is_file(sortie) and os.is_file(fichier_cle) {
        if read_text(fichier_cle) == cle {
            return Compilation(true, "", "0.00", true)
        }
    }
    os.remove(fichier_cle)
    let commande: list[string] = [compilateur(), os.join(dossier(), nom), "-o", sortie]
    if mode != "" {
        commande.append(mode)
    }
    let t0: float = os.clock()
    let c: RunResult = os.run(commande)
    let duree: string = format(os.clock() - t0, ".2f")
    if c.code != 0 {
        return Compilation(false, c.err + c.out, duree, false)
    }
    write_file(fichier_cle, cle)
    return Compilation(true, "", duree, false)
}

// lance sortie dans le dossier du fichier, sans l'attendre
def demarrer(nom: string, sortie: string, c: Compilation, entree: string) -> Response {
    if not c.ok {
        return web.json("{\"ok\":false,\"erreurs\":" + web.json_string(c.erreurs) + ",\"duree\":" + web.json_string(c.duree) + "}")
    }
    let p: Process = os.start_merged(["sh", "-c", "cd \"$1\" && exec \"$2\"", "sh", os.dirname(os.join(dossier(), nom)), sortie])
    if entree != "" {
        p.write(entree)
    }
    return web.json("{\"ok\":true,\"id\":" + str(p.id) + ",\"duree\":" + web.json_string(c.duree) + ",\"cache\":" + oui(c.cache) + "}")
}

// compile (si besoin), puis lance le programme
@web.route("/api/lancer/<path:nom>", "POST")
def lancer(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let sortie: string = binaire(nom)
    return demarrer(nom, sortie, compiler(nom, sortie, ""), req.body)
}

// V0.59 : les blocs test "nom" { ... } du fichier, chacun a part (clair --test)
@web.route("/api/tester/<path:nom>", "POST")
def tester(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let sortie: string = binaire(nom) + ".tests"
    return demarrer(nom, sortie, compiler(nom, sortie, "--test"), "")
}

// V0.59 : les erreurs du fichier, sans le compiler (clair --check : quelques millisecondes)
@web.route("/api/verifier/<path:nom>", "POST")
def verifier(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let source: string = os.join(dossier(), nom)
    if not os.is_file(source) {
        return web.error(404, "pas de fichier " + nom)
    }
    let t0: float = os.clock()
    let c: RunResult = os.run([compilateur(), source, "--check"])
    let duree: string = format((os.clock() - t0) * 1000.0, ".0f")
    return web.json("{\"ok\":" + oui(c.code == 0) + ",\"erreurs\":" + web.json_string(c.err + c.out) + ",\"ms\":" + duree + "}")
}

// V0.59 : la source des modules os et web, pour l'autocompletion de l'editeur
@web.route("/api/module/<nom>")
def module_source(req: Request, nom: string) -> Response {
    if not autorise(req) {
        return refus()
    }
    let chemin: string = os.join(racine(), "src/clair_" + nom + ".cl")
    if (nom != "os" and nom != "web" and nom != "plot") or not os.is_file(chemin) {
        return web.error(404, "pas de module " + nom)
    }
    return web.text(read_text(chemin))
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
@web.route("/api/c/<path:nom>")
def code_c(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    let chemin: string = binaire(nom) + ".generated.c"
    if not os.is_file(chemin) {
        return web.text("// Lance d'abord le programme : le C apparait apres la compilation.\n")
    }
    return web.text(read_text(chemin))
}

// ---- V0.61 : les figures de plot.show() ----
// Les programmes lances par le Studio ecrivent leurs figures dans ce dossier (CLAIR_PLOT_DIR)
// et une ligne ESC[clair:plot NOM] que l'editeur remplace par l'image.

def dossier_figures() -> string {
    return os.join(os.join(os.env("TMPDIR", "/tmp"), "clair_studio"), "figures")
}

// 123_4_56.svg, 123_a1_56.svg : rien d'autre
def figure_valide(nom: string) -> bool {
    if len(nom) < 5 or len(nom) > 60 or nom[len(nom) - 4:len(nom)] != ".svg" {
        return false
    }
    for i in range(0, len(nom) - 4) {
        let c: int = ord(nom[i:i + 1])
        if not ((c >= 48 and c <= 57) or c == 95 or c == 97) {
            return false
        }
    }
    return true
}

@web.route("/api/figure/<nom>")
def figure(req: Request, nom: string) -> Response {
    if not autorise(req) or not figure_valide(nom) {
        return refus()
    }
    let chemin: string = os.join(dossier_figures(), nom)
    if not os.is_file(chemin) {
        return web.error(404, "pas de figure " + nom)
    }
    return web.text(read_text(chemin))
}

// plot.image("photo.png") : une image du dossier des programmes
@web.route("/api/image/<path:chemin>")
def image(req: Request, chemin: string) -> Response {
    if not autorise(req) or not chemin_valide(chemin) {
        return refus()
    }
    let ext: string = os.extension(chemin)
    let ok: bool = ext == ".png" or ext == ".jpg" or ext == ".jpeg" or ext == ".gif" or ext == ".svg" or ext == ".webp"
    let fichier: string = os.join(dossier(), chemin)
    if not ok or not os.is_file(fichier) {
        return web.error(404, "pas d'image " + chemin)
    }
    return web.file(fichier)
}

// ---- V0.60 : Git (les modifications et les versions du dossier) ----
// git -C dossier ..., toujours avec -- avant les fichiers ; l'editeur lit ce que git ecrit.

def git(args: list[string]) -> RunResult {
    let commande: list[string] = ["git", "-C", dossier(), "-c", "core.quotePath=false", "-c", "color.ui=false"]
    for a in args {
        commande.append(a)
    }
    return os.run(commande)
}

def git_reponse(r: RunResult) -> Response {
    return web.json("{\"ok\":" + oui(r.code == 0) + ",\"code\":" + str(r.code) + ",\"sortie\":" + web.json_string(r.out + r.err) + "}")
}

// un fichier du dossier ou d'un sous-dossier : pas de .., pas de nom cache, rien qui ressemble a une option
def chemin_valide(c: string) -> bool {
    if c == "" or len(c) > 300 or c[0:1] == "/" {
        return false
    }
    for morceau in split(c, "/") {
        if morceau == "" or morceau[0:1] == "." or morceau[0:1] == "-" {
            return false
        }
    }
    return true
}

// un numero de version : 4 a 40 chiffres hexadecimaux
def version_valide(h: string) -> bool {
    if len(h) < 4 or len(h) > 40 {
        return false
    }
    for i in range(0, len(h)) {
        let c: int = ord(h[i:i + 1])
        if not ((c >= 48 and c <= 57) or (c >= 97 and c <= 102)) {
            return false
        }
    }
    return true
}

def a_des_versions() -> bool {
    return git(["rev-parse", "--verify", "-q", "HEAD"]).code == 0
}

@web.route("/api/git/etat")
def git_etat(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    if os.which("git") == "" {
        return web.json("{\"git\":false}")
    }
    let prefixe: RunResult = git(["rev-parse", "--show-prefix"])
    if prefixe.code != 0 {
        return web.json("{\"git\":true,\"depot\":false}")
    }
    let statut: RunResult = git(["status", "--porcelain=v1", "--branch", "--untracked-files=all", "--", "."])
    let distant: RunResult = git(["remote", "get-url", "origin"])
    let nom: string = strip(git(["config", "user.name"]).out)
    let email: string = strip(git(["config", "user.email"]).out)
    return web.json("{\"git\":true,\"depot\":true,\"prefixe\":" + web.json_string(strip(prefixe.out)) + ",\"statut\":" + web.json_string(statut.out) + ",\"distant\":" + web.json_string(strip(distant.out)) + ",\"identite\":" + oui(nom != "" and email != "") + "}")
}

@web.route("/api/git/creer", "POST")
def git_creer(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    return git_reponse(git(["init", "-q", "-b", "main"]))
}

// le nom et l'adresse mail qui signent les versions de ce dossier
@web.route("/api/git/identite", "POST")
def git_identite(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    let parties: list[string] = split(req.body, "\n")
    if len(parties) < 2 or strip(parties[0]) == "" or strip(parties[1]) == "" {
        return web.json("{\"ok\":false,\"code\":1,\"sortie\":\"un nom et une adresse mail\"}")
    }
    let r: RunResult = git(["config", "user.name", strip(parties[0])])
    if r.code != 0 {
        return git_reponse(r)
    }
    return git_reponse(git(["config", "user.email", strip(parties[1])]))
}

// ce qui a change dans un fichier depuis la derniere version (?nouveau=1 : un fichier que git ne suit pas)
@web.route("/api/git/diff/<path:chemin>")
def git_diff(req: Request, chemin: string) -> Response {
    if not autorise(req) or not chemin_valide(chemin) {
        return refus()
    }
    if req.arg("nouveau", "") == "1" or not a_des_versions() {
        let r: RunResult = git(["diff", "--no-index", "--", "/dev/null", chemin])
        return web.json("{\"ok\":true,\"code\":0,\"sortie\":" + web.json_string(r.out) + "}")
    }
    return git_reponse(git(["diff", "HEAD", "--", chemin]))
}

// les lignes changees du fichier ouvert, pour la marge de l'editeur
@web.route("/api/git/lignes/<path:nom>")
def git_lignes(req: Request, nom: string) -> Response {
    if not autorise(req) or not chemin_valide(nom) {
        return refus()
    }
    return git_reponse(git(["diff", "-U0", "HEAD", "--", nom]))
}

// enregistre une version de tout le dossier
@web.route("/api/git/version", "POST")
def git_version(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    if strip(req.body) == "" {
        return web.json("{\"ok\":false,\"code\":1,\"sortie\":\"il faut un message\"}")
    }
    let ajout: RunResult = git(["add", "-A", "--", "."])
    if ajout.code != 0 {
        return git_reponse(ajout)
    }
    return git_reponse(git(["commit", "-q", "-m", req.body, "--", "."]))
}

@web.route("/api/git/historique")
def git_historique(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    if not a_des_versions() {
        return web.json("{\"ok\":true,\"code\":0,\"sortie\":\"\"}")
    }
    return git_reponse(git(["log", "-n", "200", "--format=%h%x1f%an%x1f%ct%x1f%s", "--", "."]))
}

@web.route("/api/git/voir/<version>")
def git_voir(req: Request, version: string) -> Response {
    if not autorise(req) or not version_valide(version) {
        return refus()
    }
    return git_reponse(git(["show", "--stat", "--patch", "--date=format:%d/%m/%Y %H:%M", "--format=%h, %an, %ad%n%n%B", version, "--", "."]))
}

// remet un fichier comme dans la derniere version (ses changements sont perdus)
@web.route("/api/git/annuler/<path:chemin>", "POST")
def git_annuler(req: Request, chemin: string) -> Response {
    if not autorise(req) or not chemin_valide(chemin) {
        return refus()
    }
    return git_reponse(git(["checkout", "-q", "HEAD", "--", chemin]))
}

// remet un fichier comme dans une version plus ancienne
@web.route("/api/git/restaurer/<version>/<path:chemin>", "POST")
def git_restaurer(req: Request, version: string, chemin: string) -> Response {
    if not autorise(req) or not version_valide(version) or not chemin_valide(chemin) {
        return refus()
    }
    return git_reponse(git(["checkout", "-q", version, "--", chemin]))
}

// relie le dossier a un depot distant (GitHub) : https://..., git@..., ssh://..., ou un chemin
@web.route("/api/git/distant", "POST")
def git_distant(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    let url: string = strip(req.body)
    let ok: bool = url[0:8] == "https://" or url[0:6] == "ssh://" or url[0:4] == "git@" or url[0:1] == "/"
    if not ok or len(url) > 500 {
        return web.json("{\"ok\":false,\"code\":1,\"sortie\":\"une adresse https://..., git@... ou ssh://...\"}")
    }
    if git(["remote", "get-url", "origin"]).code == 0 {
        return git_reponse(git(["remote", "set-url", "origin", url]))
    }
    return git_reponse(git(["remote", "add", "origin", url]))
}

// envoyer et recuperer : jamais de question dans le terminal (GIT_TERMINAL_PROMPT=0), 90 s au plus
@web.route("/api/git/envoyer", "POST")
def git_envoyer(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    return git_reponse(os.run_with(["git", "-C", dossier(), "push", "-u", "origin", "HEAD"], "", 90.0))
}

@web.route("/api/git/recuperer", "POST")
def git_recuperer(req: Request) -> Response {
    if not autorise(req) {
        return refus()
    }
    return git_reponse(os.run_with(["git", "-C", dossier(), "pull", "--ff-only"], "", 90.0))
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
    os.set_env("GIT_TERMINAL_PROMPT", "0")       // V0.60 : git ne pose jamais de question au terminal
    // V0.61 : les figures des programmes, effacees a chaque demarrage du Studio
    let figures: string = dossier_figures()
    os.mkdir(figures)
    for f in os.listdir(figures) {
        os.remove(os.join(figures, f))
    }
    os.set_env("CLAIR_PLOT_DIR", figures)
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
