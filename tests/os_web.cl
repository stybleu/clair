// V0.56 : web et os ensemble (ce dont un IDE dans le navigateur a besoin)
import web
import os

@web.route("/lancer/<mot>")
def lancer(mot: string) -> Response {
    let r: RunResult = os.run(["sh", "-c", "echo \"$1\" | tr a-z A-Z", "sh", mot])
    return web.text(str(r.code) + " " + strip(r.out))
}

@web.route("/fichiers")
def fichiers() -> string {
    let html: string = ""
    for n in os.listdir("static") {
        html = html + n + ";"
    }
    return html
}

def main() -> int {
    web.run(int(input()))
    return 0
}
