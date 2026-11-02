// ==== Clair Studio : écrit par le designer à partir de Calcul.ui. Ne pas changer jusqu'à la ligne de fin.
import web
import forms

Class Calcul {
    fenetre: Control
    saisie1: Control
    saisie2: Control
    bouton1: Control
    etiquette1: Control
    liste1: Control
    minuteur1: Control
}

// la fenêtre telle que la page l'envoie
def calcul_lire(req: Request) -> Calcul {
    return Calcul(forms.read(req, "fenetre"), forms.read(req, "saisie1"), forms.read(req, "saisie2"), forms.read(req, "bouton1"), forms.read(req, "etiquette1"), forms.read(req, "liste1"), forms.read(req, "minuteur1"))
}

// la fenêtre pour la page
def calcul_json(f: Calcul) -> string {
    return "{" + forms.json(f.fenetre) + "," + forms.json(f.saisie1) + "," + forms.json(f.saisie2) + "," + forms.json(f.bouton1) + "," + forms.json(f.etiquette1) + "," + forms.json(f.liste1) + "," + forms.json(f.minuteur1) + "}"
}

@web.route("/")
def calcul_page() -> Response {
    return forms.page("Calcul.html")
}

@web.route("/evenement", "POST")
def calcul_evenement(req: Request) -> Response {
    let f: Calcul = calcul_lire(req)
    let e: string = req.field("_evenement", "")
    if e == "bouton1.click" {
        bouton1_click(f)
    }
    if e == "fenetre.load" {
        fenetre_load(f)
    }
    if e == "minuteur1.tick" {
        minuteur1_tick(f)
    }
    return web.json(calcul_json(f))
}

def main() -> int {
    return forms.run(8100)
}
// ==== fin de ce qu'écrit le designer ====

// Ton code : les fonctions qui répondent aux événements.
// Dans le dessin, touche deux fois un contrôle pour écrire la sienne.

// quand on touche bouton1
def bouton1_click(f: Calcul) -> int {
    if not f.saisie1.is_number() or not f.saisie2.is_number() {
        f.fenetre.message = "Il faut deux nombres"
        return 0
    }
    let p: float = f.saisie1.number() * f.saisie2.number()
    f.etiquette1.text = "Produit : " + forms.text_of(p)
    f.liste1.add(f.saisie1.text + " x " + f.saisie2.text)
    f.liste1.selected = len(f.liste1.items) - 1
    return 0
}

// quand la fenêtre s'ouvre
def fenetre_load(f: Calcul) -> int {
    f.etiquette1.text = "Prêt"
    return 0
}

// à chaque tic de minuteur1
def minuteur1_tick(f: Calcul) -> int {
    f.minuteur1.text = str(int(f.minuteur1.number()) + 1)
    return 0
}
