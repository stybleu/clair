// V0.63, V0.64 : un reseau de neurones (comme celui de XOR) qui apprend a prevoir la pluie
// a partir de la meteo heure par heure, ici a Paris, et de celle d'un lieu a l'ouest (Rennes) :
// en France, le mauvais temps arrive surtout de l'Atlantique.
//
// Les donnees : Open-Meteo, la reanalyse ERA5 du centre europeen (ECMWF), gratuite et sans compte,
// de 2010 a 2024, en heure universelle. La premiere fois, le programme les telecharge avec curl
// (pkg install curl) dans meteo_<latitude>_<longitude>.csv (environ 7 Mo par lieu), puis il relit ces fichiers.
// Pour d'autres lieux : changer lieu() et ouest() ; les nouveaux fichiers seront telecharges.
//
// La question : va-t-il pleuvoir (au moins 0.5 mm) dans les 6 prochaines heures ?
// Le reseau apprend sur les premieres annees, puis on le mesure sur la derniere, qu'il n'a jamais vue,
// en le comparant a deux previsions naives : "toujours sec" et "comme les 6 heures d'avant".
// Il annonce la pluie au-dela d'un seuil de probabilite, choisi sur les annees d'apprentissage.
//
// clair --test pluie.cl -o t && ./t : verifie le reseau, la lecture et l'alignement, sans Internet.
import os
import plot
import math

// ---- les lieux : Paris, et Rennes a l'ouest (a changer) ----

def lieu() -> list[string] {
    return ["48.85", "2.35", "Paris"]
}

def ouest() -> list[string] {
    return ["48.11", "-1.68", "Rennes"]
}

def fichier_de(l: list[string]) -> string {
    return "meteo_" + l[0] + "_" + l[1] + ".csv"
}

def adresse(l: list[string]) -> string {
    return "https://archive-api.open-meteo.com/v1/archive?latitude=" + l[0] + "&longitude=" + l[1] + "&start_date=2010-01-01&end_date=2024-12-31&hourly=temperature_2m,relative_humidity_2m,pressure_msl,precipitation,cloud_cover,wind_speed_10m,wind_direction_10m&timezone=GMT&format=csv"
}

// ---- les donnees ----

Class Meteo {
    date: list[string]       // "2019-01-01T00:00", heure universelle
    temp: list[float]        // degres
    hum: list[float]         // humidite, %
    pres: list[float]        // pression au niveau de la mer, hPa
    pluie: list[float]       // mm dans l'heure
    nuages: list[float]      // %
    vent: list[float]        // km/h
    dir: list[float]         // d'ou vient le vent, en degres (0 : du nord, 270 : de l'ouest)
}

def telecharger(l: list[string]) -> bool {
    let fichier: string = fichier_de(l)
    if os.is_file(fichier) {
        return true
    }
    if os.which("curl") == "" {
        print("Il faut curl pour télécharger les données : pkg install curl")
        return false
    }
    print("Téléchargement de 15 ans de météo heure par heure à " + l[2] + " (Open-Meteo, environ 7 Mo)...")
    let r: RunResult = os.run(["curl", "-s", "-S", "-f", "-L", "--max-time", "300", "-o", fichier, adresse(l)])
    if r.code != 0 {
        print("Le téléchargement a échoué : " + strip(r.err))
        os.remove(fichier)
        return false
    }
    return true
}

// le CSV d'Open-Meteo : quelques lignes sur le lieu, une ligne vide, puis
// "time,temperature_2m (°C),relative_humidity_2m (%),..." et une ligne par heure
def lire(fichier: string) -> Meteo {
    let m: Meteo = Meteo([], [], [], [], [], [], [], [])
    let noms: list[string] = ["temperature_2m", "relative_humidity_2m", "pressure_msl", "precipitation", "cloud_cover", "wind_speed_10m", "wind_direction_10m"]
    let col: list[int] = [-1, -1, -1, -1, -1, -1, -1]
    let en_tete: bool = false
    for ligne in read_lines(fichier) {
        let parts: list[string] = split(strip(ligne), ",")
        if not en_tete {
            if len(parts) > 1 and parts[0] == "time" {
                for c in range(1, len(parts)) {
                    let nom: string = split(parts[c], " ")[0]
                    for k in range(0, 7) {
                        if nom == noms[k] {
                            col[k] = c
                        }
                    }
                }
                en_tete = true
            }
        } else {
            // une heure complete seulement (une valeur manquante : l'heure est laissee)
            let complete: bool = len(parts) > 7
            for k in range(0, 7) {
                if complete and (col[k] < 0 or col[k] >= len(parts)) {
                    complete = false
                }
                if complete and parts[col[k]] == "" {
                    complete = false
                }
            }
            if complete and len(parts[0]) >= 13 {
                m.date.append(parts[0])
                m.temp.append(float(parts[col[0]]))
                m.hum.append(float(parts[col[1]]))
                m.pres.append(float(parts[col[2]]))
                m.pluie.append(float(parts[col[3]]))
                m.nuages.append(float(parts[col[4]]))
                m.vent.append(float(parts[col[5]]))
                m.dir.append(float(parts[col[6]]))
            }
        }
    }
    return m
}

// pour chaque heure de m, la meme heure dans a (-1 si a ne l'a pas)
def aligner(m: Meteo, a: Meteo) -> list[int] {
    let ou: dict[string, int] = {}
    for k in range(len(a.date)) {
        ou[a.date[k]] = k
    }
    let r: list[int] = []
    for d in m.date {
        if d in ou {
            r.append(ou[d])
        } else {
            r.append(-1)
        }
    }
    return r
}

// la pluie des 6 heures jusqu'a t compris
def pluie_6h(m: Meteo, t: int) -> float {
    let s: float = 0.0
    for k in range(t - 5, t + 1) {
        s += m.pluie[k]
    }
    return s
}

// ---- ce que le reseau voit, et ce qu'il doit dire ----

def nb_entrees() -> int {
    return 20
}

// on peut s'en servir : 12 heures d'avant ici, 6 a l'ouest, 6 heures d'apres (pour la reponse)
def utilisable(m: Meteo, ia: list[int], t: int) -> bool {
    return t >= 12 and t + 6 < len(m.date) and ia[t] >= 6 and ia[t - 3] >= 0
}

// a l'heure t : la pression et comment elle change, l'humidite, la temperature, les nuages, le vent
// (sa force et d'ou il vient), la pluie des 6 dernieres heures ; a l'ouest : la pression, comment elle
// change, la pluie, l'humidite, les nuages ; la saison et l'heure. Des nombres proches de -1 a 1.
def entrees(m: Meteo, a: Meteo, ia: list[int], t: int, x: matrix[float], i: int) -> int {
    let mois: float = float(int(m.date[t][5:7]))
    let heure: float = float(int(m.date[t][11:13]))
    let angle: float = m.dir[t] * math.pi / 180.0
    let ta: int = ia[t]
    x[i][0] = (m.pres[t] - 1013.0) / 10.0
    x[i][1] = (m.pres[t] - m.pres[t - 3]) / 2.0
    x[i][2] = (m.pres[t] - m.pres[t - 12]) / 5.0
    x[i][3] = m.hum[t] / 100.0
    x[i][4] = (m.hum[t] - m.hum[t - 3]) / 20.0
    x[i][5] = (m.temp[t] - 12.0) / 8.0
    x[i][6] = (m.temp[t] - m.temp[t - 3]) / 3.0
    x[i][7] = m.nuages[t] / 100.0
    x[i][8] = m.vent[t] * math.sin(angle) / 20.0
    x[i][9] = m.vent[t] * math.cos(angle) / 20.0
    x[i][10] = min(pluie_6h(m, t), 10.0) / 5.0
    x[i][11] = (a.pres[ta] - m.pres[t]) / 5.0
    x[i][12] = (a.pres[ta] - a.pres[ia[t - 3]]) / 2.0
    x[i][13] = min(pluie_6h(a, ta), 10.0) / 5.0
    x[i][14] = a.hum[ta] / 100.0
    x[i][15] = a.nuages[ta] / 100.0
    x[i][16] = math.sin(2.0 * math.pi * mois / 12.0)
    x[i][17] = math.cos(2.0 * math.pi * mois / 12.0)
    x[i][18] = math.sin(2.0 * math.pi * heure / 24.0)
    x[i][19] = math.cos(2.0 * math.pi * heure / 24.0)
    return 0
}

// 1 s'il tombe au moins 0.5 mm dans les 6 heures qui suivent t, sinon 0
def pleut_ensuite(m: Meteo, t: int) -> float {
    if pluie_6h(m, t + 6) >= 0.5 {
        return 1.0
    }
    return 0.0
}

// ---- le reseau : 20 entrees, 24 neurones caches (tanh), une sortie (la probabilite de pluie) ----

def sigmoide(s: float) -> float {
    return 1.0 / (1.0 + exp(-s))
}

// l'entropie croisee : ce que coute une prevision p quand la reponse est y
def perte(p: float, y: float) -> float {
    let q: float = min(max(p, 1e-7), 1.0 - 1e-7)
    return -(y * log(q) + (1.0 - y) * log(1.0 - q))
}

Class Reseau {
    w1: matrix[float]
    b1: list[float]
    w2: list[float]
    b2: float
    h: list[float]

    def prevoir(x: matrix[float], i: int) -> float {
        for k in range(rows(self.w1)) {
            let s: float = self.b1[k]
            for j in range(cols(self.w1)) {
                s += self.w1[k][j] * x[i][j]
            }
            self.h[k] = tanh(s)
        }
        let s: float = self.b2
        for k in range(len(self.w2)) {
            s += self.w2[k] * self.h[k]
        }
        return sigmoide(s)
    }

    // un pas de descente de gradient sur l'exemple i (reponse y) ; rend la perte
    def apprendre(x: matrix[float], i: int, y: float, lr: float) -> float {
        let p: float = self.prevoir(x, i)
        let d: float = p - y
        for k in range(len(self.w2)) {
            let hk: float = self.h[k]
            let dh: float = d * self.w2[k] * (1.0 - hk * hk)
            self.w2[k] = self.w2[k] - lr * d * hk
            self.b1[k] = self.b1[k] - lr * dh
            for j in range(cols(self.w1)) {
                self.w1[k][j] = self.w1[k][j] - lr * dh * x[i][j]
            }
        }
        self.b2 = self.b2 - lr * d
        return perte(p, y)
    }
}

def creer_reseau(entrees: int, cachees: int) -> Reseau {
    let r: Reseau = Reseau(matrix(cachees, entrees), [], [], 0.0, [])
    let a: float = 1.0 / math.sqrt(float(entrees))
    for k in range(cachees) {
        for j in range(entrees) {
            r.w1[k][j] = uniform(-a, a)
        }
        r.b1.append(0.0)
        r.h.append(0.0)
        r.w2.append(uniform(-0.5, 0.5))
    }
    return r
}

// la perte moyenne sur les lignes de x (sans apprendre)
def perte_moyenne(r: Reseau, x: matrix[float], y: list[float]) -> float {
    let s: float = 0.0
    for i in range(len(y)) {
        s += perte(r.prevoir(x, i), y[i])
    }
    return s / float(len(y))
}

// ---- les scores : sur 100 heures, combien de bonnes reponses ; parmi les pluies, combien annoncees ;
//      parmi les annonces de pluie, combien fausses ----

Class Score {
    bon: int
    vrai_pluie: int
    pluie_ratee: int
    fausse_alerte: int
    total: int
}

def compter(s: Score, annonce: bool, pluie: bool) -> int {
    s.total += 1
    if annonce == pluie {
        s.bon += 1
    }
    if annonce and pluie {
        s.vrai_pluie += 1
    }
    if pluie and not annonce {
        s.pluie_ratee += 1
    }
    if annonce and not pluie {
        s.fausse_alerte += 1
    }
    return 0
}

def pourcent(a: int, b: int) -> string {
    if b == 0 {
        return "  -"
    }
    return format(100.0 * float(a) / float(b), ".0f") + " %"
}

def ligne_score(nom: string, s: Score) -> string {
    return nom + " : réussite " + pourcent(s.bon, s.total) + ", pluies annoncées " + pourcent(s.vrai_pluie, s.vrai_pluie + s.pluie_ratee) + ", fausses alertes " + pourcent(s.fausse_alerte, s.vrai_pluie + s.fausse_alerte)
}

// les scores des probabilites p avec le seuil s
def scorer(p: list[float], y: list[float], seuil: float) -> Score {
    let sc: Score = Score(0, 0, 0, 0, 0)
    for i in range(len(p)) {
        compter(sc, p[i] >= seuil, y[i] > 0.5)
    }
    return sc
}

// le seuil qui equilibre les pluies trouvees et les fausses alertes (le meilleur score F1),
// choisi sur les annees d'apprentissage
def choisir_seuil(p: list[float], y: list[float]) -> float {
    let meilleur: float = 0.5
    let f1_max: float = -1.0
    for k in range(1, 20) {
        let s: float = float(k) / 20.0
        let sc: Score = scorer(p, y, s)
        let f1: float = 2.0 * float(sc.vrai_pluie) / float(max(1, 2 * sc.vrai_pluie + sc.fausse_alerte + sc.pluie_ratee))
        if f1 > f1_max {
            f1_max = f1
            meilleur = s
        }
    }
    return meilleur
}

def main() -> int {
    if not telecharger(lieu()) or not telecharger(ouest()) {
        return 1
    }
    let m: Meteo = lire(fichier_de(lieu()))
    let a: Meteo = lire(fichier_de(ouest()))
    let n: int = len(m.date)
    if n < 24 * 60 or len(a.date) < 24 * 60 {
        print("Trop peu de données : efface " + fichier_de(lieu()) + " et " + fichier_de(ouest()) + " pour les télécharger de nouveau.")
        return 1
    }
    let ia: list[int] = aligner(m, a)
    let derniere: int = int(m.date[n - 1][0:4])
    print(str(n) + " heures à " + lieu()[2] + " et " + str(len(a.date)) + " à " + ouest()[2] + ", du " + m.date[0][0:10] + " au " + m.date[n - 1][0:10])

    // les exemples : les annees d'avant pour apprendre, la derniere pour mesurer
    let n_appr: int = 0
    let n_mes: int = 0
    for t in range(n) {
        if utilisable(m, ia, t) {
            if int(m.date[t][0:4]) < derniere {
                n_appr += 1
            } else {
                n_mes += 1
            }
        }
    }
    if n_appr < 1000 or n_mes < 500 {
        print("Il faut au moins deux années de données aux deux lieux.")
        return 1
    }
    let xa: matrix[float] = matrix(n_appr, nb_entrees())
    let xm: matrix[float] = matrix(n_mes, nb_entrees())
    let ya: list[float] = []
    let ym: list[float] = []
    let tm: list[int] = []
    for t in range(n) {
        if utilisable(m, ia, t) {
            if int(m.date[t][0:4]) < derniere {
                entrees(m, a, ia, t, xa, len(ya))
                ya.append(pleut_ensuite(m, t))
            } else {
                entrees(m, a, ia, t, xm, len(ym))
                ym.append(pleut_ensuite(m, t))
                tm.append(t)
            }
        }
    }
    let f: float = 0.0
    for v in ya {
        f += v
    }
    f = f / float(n_appr)
    let fm: float = 0.0
    for v in ym {
        fm += v
    }
    fm = fm / float(n_mes)
    print("Apprentissage sur " + str(n_appr) + " heures (pluie dans les 6 h : " + format(100.0 * f, ".0f") + " %), mesure sur les " + str(n_mes) + " heures de " + str(derniere) + " (" + format(100.0 * fm, ".0f") + " %).")

    // l'apprentissage : chaque passage voit les heures dans un ordre different
    seed(2024)
    let r: Reseau = creer_reseau(nb_entrees(), 24)
    let ordre: list[int] = []
    for i in range(n_appr) {
        ordre.append(i)
    }
    let passages: list[float] = []
    let pertes_appr: list[float] = []
    let pertes_mes: list[float] = []
    for passage in range(1, 9) {
        shuffle(ordre)
        let lr: float = 0.01 / (1.0 + 0.3 * float(passage - 1))
        for i in ordre {
            r.apprendre(xa, i, ya[i], lr)
        }
        passages.append(float(passage))
        pertes_appr.append(perte_moyenne(r, xa, ya))
        pertes_mes.append(perte_moyenne(r, xm, ym))
        print("passage " + str(passage) + " : perte " + format(pertes_appr[passage - 1], ".4f") + " (apprentissage), " + format(pertes_mes[passage - 1], ".4f") + " (" + str(derniere) + ")")
    }

    // le seuil, choisi sur l'apprentissage (pas sur la derniere annee : la mesure reste honnete)
    let pa: list[float] = []
    for i in range(n_appr) {
        pa.append(r.prevoir(xa, i))
    }
    let seuil: float = choisir_seuil(pa, ya)

    // la mesure sur la derniere annee
    let sec: Score = Score(0, 0, 0, 0, 0)
    let avant: Score = Score(0, 0, 0, 0, 0)
    let proba: list[float] = []
    for i in range(n_mes) {
        proba.append(r.prevoir(xm, i))
        let pluie: bool = ym[i] > 0.5
        compter(sec, false, pluie)
        compter(avant, pluie_6h(m, tm[i]) >= 0.5, pluie)
    }
    print("")
    print("Sur " + str(derniere) + ", jamais vu pendant l'apprentissage :")
    print(ligne_score("  toujours sec           ", sec))
    print(ligne_score("  comme les 6 h d'avant  ", avant))
    print(ligne_score("  réseau (seuil 50 %)    ", scorer(proba, ym, 0.5)))
    print(ligne_score("  réseau (seuil " + format(100.0 * seuil, ".0f") + " %)    ", scorer(proba, ym, seuil)))

    // 1. comment l'erreur baisse, et ce que ferait une probabilite toujours la meme (la frequence de la pluie)
    let fixe: float = f * perte(f, 1.0) + (1.0 - f) * perte(f, 0.0)
    plot.title("Apprentissage : l'erreur à chaque passage")
    plot.xlabel("passage sur les données")
    plot.ylabel("perte (entropie croisée)")
    plot.line(passages, pertes_appr)
    plot.name("années d'apprentissage")
    plot.line(passages, pertes_mes)
    plot.name(str(derniere) + " (jamais vue)")
    plot.segment(1.0, fixe, float(len(passages)), fixe)
    plot.color("gris")
    plot.name("probabilité toujours la même (" + format(100.0 * f, ".0f") + " %)")
    plot.show()

    // 2. le choix du seuil : plus il est bas, plus de pluies annoncees, mais plus de fausses alertes
    let sx: list[float] = []
    let s_trouvees: list[float] = []
    let s_fausses: list[float] = []
    for k in range(1, 20) {
        let s: float = float(k) / 20.0
        let sc: Score = scorer(pa, ya, s)
        sx.append(s)
        s_trouvees.append(float(sc.vrai_pluie) / float(max(1, sc.vrai_pluie + sc.pluie_ratee)))
        s_fausses.append(float(sc.fausse_alerte) / float(max(1, sc.vrai_pluie + sc.fausse_alerte)))
    }
    plot.title("Le seuil : pluies annoncées contre fausses alertes")
    plot.xlabel("seuil de probabilité")
    plot.ylabel("part")
    plot.yrange(0.0, 1.0)
    plot.line(sx, s_trouvees)
    plot.name("pluies annoncées")
    plot.line(sx, s_fausses)
    plot.name("fausses alertes")
    plot.segment(seuil, 0.0, seuil, 1.0)
    plot.color("gris")
    plot.name("seuil choisi")
    plot.show()

    // 3. les deux semaines de la derniere annee ou il a le plus plu
    let largeur: int = 14 * 24
    let debut: int = 0
    let meilleur: float = -1.0
    for d in range(0, n_mes - largeur, 24) {
        let s: float = 0.0
        for i in range(d, d + largeur) {
            s += ym[i]
        }
        if s > meilleur {
            meilleur = s
            debut = d
        }
    }
    let hx: list[float] = []
    let hp: list[float] = []
    let px: list[float] = []
    let py: list[float] = []
    for i in range(debut, debut + largeur) {
        let jour: float = float(i - debut) / 24.0
        hx.append(jour)
        hp.append(proba[i])
        if ym[i] > 0.5 {
            px.append(jour)
            py.append(1.05)
        }
    }
    plot.title("Deux semaines de " + str(derniere) + " à partir du " + m.date[tm[debut]][0:10])
    plot.xlabel("jours")
    plot.ylabel("probabilité de pluie dans les 6 h")
    plot.yrange(0.0, 1.15)
    plot.line(hx, hp)
    plot.name("réseau de neurones")
    plot.points(px, py)
    plot.name("il a plu")
    plot.width(1.0)
    plot.segment(0.0, seuil, 14.0, seuil)
    plot.color("gris")
    plot.width(1.0)
    plot.name("seuil choisi")
    plot.show()

    // 4. quand il annonce 70 %, pleut-il 7 fois sur 10 ?
    let annonce: list[float] = []
    let observe: list[float] = []
    for b in range(0, 10) {
        let nb: int = 0
        let pl: float = 0.0
        let somme: float = 0.0
        for i in range(n_mes) {
            if proba[i] >= float(b) / 10.0 and proba[i] < float(b + 1) / 10.0 {
                nb += 1
                pl += ym[i]
                somme += proba[i]
            }
        }
        if nb >= 20 {
            annonce.append(somme / float(nb))
            observe.append(pl / float(nb))
        }
    }
    plot.title("Fiabilité en " + str(derniere) + " : annoncé contre observé")
    plot.xlabel("probabilité annoncée")
    plot.ylabel("fréquence de la pluie")
    plot.xrange(0.0, 1.0)
    plot.yrange(0.0, 1.0)
    plot.segment(0.0, 0.0, 1.0, 1.0)
    plot.color("gris")
    plot.name("parfaitement fiable")
    plot.line(annonce, observe)
    plot.name("réseau de neurones")
    plot.points(annonce, observe)
    plot.color("bleu")
    plot.show()
    return 0
}

// ---- les tests (sans Internet) ----

test "le gradient du reseau est le bon" {
    seed(7)
    let r: Reseau = creer_reseau(3, 4)
    let x: matrix[float] = matrix(1, 3)
    x[0][0] = 0.3
    x[0][1] = -0.8
    x[0][2] = 0.5
    let eps: float = 1e-6
    let w: float = r.w1[2][1]
    r.w1[2][1] = w + eps
    let haut: float = perte(r.prevoir(x, 0), 1.0)
    r.w1[2][1] = w - eps
    let bas: float = perte(r.prevoir(x, 0), 1.0)
    r.w1[2][1] = w
    let numerique: float = (haut - bas) / (2.0 * eps)
    let lr: float = 0.001
    r.apprendre(x, 0, 1.0, lr)
    let calcule: float = (w - r.w1[2][1]) / lr
    assert(abs(numerique - calcule) < 1e-6 * max(1.0, abs(numerique)), "numerique " + str(numerique) + ", calcule " + str(calcule))
}

test "le reseau apprend une regle simple" {
    // il pleut quand la pression baisse et que l'air est humide
    seed(11)
    let n: int = 3000
    let x: matrix[float] = matrix(n, 3)
    let y: list[float] = []
    for i in range(n) {
        x[i][0] = uniform(-1.0, 1.0)
        x[i][1] = uniform(0.0, 1.0)
        x[i][2] = uniform(-1.0, 1.0)
        if x[i][0] < -0.2 and x[i][1] > 0.6 {
            y.append(1.0)
        } else {
            y.append(0.0)
        }
    }
    let r: Reseau = creer_reseau(3, 8)
    let avant: float = perte_moyenne(r, x, y)
    for passage in range(20) {
        for i in range(n) {
            r.apprendre(x, i, y[i], 0.1)
        }
    }
    let bons: int = 0
    for i in range(n) {
        if (r.prevoir(x, i) > 0.5) == (y[i] > 0.5) {
            bons += 1
        }
    }
    assert(perte_moyenne(r, x, y) < avant / 3.0, "la perte devrait baisser")
    assert(bons > n * 95 / 100, str(bons) + " bonnes reponses sur " + str(n))
}

test "lecture d'un fichier d'Open-Meteo" {
    write_file("essai_meteo.csv", "latitude,longitude,elevation,utc_offset_seconds,timezone,timezone_abbreviation\n48.84,2.36,43.0,0,GMT,GMT\n\ntime,temperature_2m (°C),relative_humidity_2m (%),pressure_msl (hPa),precipitation (mm),cloud_cover (%),wind_speed_10m (km/h),wind_direction_10m (°)\n2024-01-01T00:00,8.4,91,1012.3,0.20,100,14.8,250\n2024-01-01T01:00,8.1,93,,0.00,100,13.1,255\n2024-01-01T02:00,7.9,94,1011.6,1.40,98,12.9,262\n")
    let m: Meteo = lire("essai_meteo.csv")
    os.remove("essai_meteo.csv")
    assert_equal(len(m.date), 2)
    assert_equal(m.date[1], "2024-01-01T02:00")
    assert_equal(m.pres[1], 1011.6)
    assert_equal(m.pluie[1], 1.4)
    assert_equal(m.vent[0], 14.8)
    assert_equal(m.dir[1], 262.0)
}

test "les colonnes peuvent venir dans un autre ordre" {
    write_file("essai_meteo2.csv", "time,wind_direction_10m (°),precipitation (mm),wind_speed_10m (km/h),temperature_2m (°C),cloud_cover (%),pressure_msl (hPa),relative_humidity_2m (%)\n2024-05-01T12:00,90,0.0,10.0,21.5,20,1020.0,40\n")
    let m: Meteo = lire("essai_meteo2.csv")
    os.remove("essai_meteo2.csv")
    assert_equal(len(m.date), 1)
    assert_equal(m.temp[0], 21.5)
    assert_equal(m.hum[0], 40.0)
    assert_equal(m.dir[0], 90.0)
}

test "les heures des deux lieux sont alignees" {
    let m: Meteo = Meteo(["2024-01-01T00:00", "2024-01-01T01:00", "2024-01-01T02:00"], [], [], [], [], [], [], [])
    let a: Meteo = Meteo(["2024-01-01T01:00", "2024-01-01T02:00", "2024-01-01T03:00"], [], [], [], [], [], [], [])
    let ia: list[int] = aligner(m, a)
    assert_equal(ia[0], -1)
    assert_equal(ia[1], 0)
    assert_equal(ia[2], 1)
}

test "le seuil choisi equilibre pluies trouvees et fausses alertes" {
    // des probabilites bien rangees : le meilleur seuil separe les pluies des heures seches
    let p: list[float] = [0.1, 0.2, 0.25, 0.3, 0.6, 0.7, 0.8, 0.9]
    let y: list[float] = [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0]
    let s: float = choisir_seuil(p, y)
    assert(s > 0.3 and s <= 0.6, "seuil " + str(s))
    let sc: Score = scorer(p, y, s)
    assert_equal(sc.bon, 8)
}
