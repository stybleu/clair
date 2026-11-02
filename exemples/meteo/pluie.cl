// V0.63 : un reseau de neurones (comme celui de XOR) qui apprend a prevoir la pluie
// a partir de la meteo d'un lieu, heure par heure, sur plusieurs annees.
//
// Les donnees : Open-Meteo, la reanalyse ERA5 du centre europeen (ECMWF), gratuite et sans compte.
// La premiere fois, le programme les telecharge avec curl (pkg install curl) dans historique.csv ;
// ensuite il relit ce fichier. Pour un autre lieu : changer latitude() et longitude(), effacer historique.csv.
//
// La question : va-t-il pleuvoir (au moins 0.5 mm) dans les 6 prochaines heures ?
// Le reseau apprend sur les premieres annees, puis on le mesure sur la derniere, qu'il n'a jamais vue,
// en le comparant a deux previsions naives : "toujours sec" et "comme les 6 heures d'avant".
//
// clair --test pluie.cl -o t && ./t : verifie le reseau et la lecture du fichier, sans Internet.
import os
import plot
import math

// ---- le lieu : Paris (a changer) ----

def latitude() -> string {
    return "48.85"
}

def longitude() -> string {
    return "2.35"
}

def adresse() -> string {
    return "https://archive-api.open-meteo.com/v1/archive?latitude=" + latitude() + "&longitude=" + longitude() + "&start_date=2019-01-01&end_date=2024-12-31&hourly=temperature_2m,relative_humidity_2m,pressure_msl,precipitation,cloud_cover,wind_speed_10m&timezone=auto&format=csv"
}

// ---- les donnees ----

Class Meteo {
    date: list[string]       // "2019-01-01T00:00"
    temp: list[float]        // degres
    hum: list[float]         // humidite, %
    pres: list[float]        // pression au niveau de la mer, hPa
    pluie: list[float]       // mm dans l'heure
    nuages: list[float]      // %
    vent: list[float]        // km/h
}

def telecharger(fichier: string) -> bool {
    if os.is_file(fichier) {
        return true
    }
    if os.which("curl") == "" {
        print("Il faut curl pour télécharger les données : pkg install curl")
        return false
    }
    print("Téléchargement de 6 ans de météo heure par heure (Open-Meteo, environ 2 Mo)...")
    let r: RunResult = os.run(["curl", "-s", "-S", "-f", "-L", "--max-time", "180", "-o", fichier, adresse()])
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
    let m: Meteo = Meteo([], [], [], [], [], [], [])
    let noms: list[string] = ["temperature_2m", "relative_humidity_2m", "pressure_msl", "precipitation", "cloud_cover", "wind_speed_10m"]
    let col: list[int] = [-1, -1, -1, -1, -1, -1]
    let en_tete: bool = false
    for ligne in read_lines(fichier) {
        let parts: list[string] = split(strip(ligne), ",")
        if not en_tete {
            if len(parts) > 1 and parts[0] == "time" {
                for c in range(1, len(parts)) {
                    let nom: string = split(parts[c], " ")[0]
                    for k in range(0, 6) {
                        if nom == noms[k] {
                            col[k] = c
                        }
                    }
                }
                en_tete = true
            }
        } else {
            // une heure complete seulement (une valeur manquante : l'heure est laissee)
            let complete: bool = len(parts) > 6
            for k in range(0, 6) {
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
            }
        }
    }
    return m
}

// ---- ce que le reseau voit, et ce qu'il doit dire ----

def nb_entrees() -> int {
    return 14
}

// a l'heure t : la pression et comment elle change, l'humidite, la temperature, les nuages, le vent,
// la pluie des 6 dernieres heures, la saison et l'heure du jour ; ramenes a des nombres proches de -1 a 1
def entrees(m: Meteo, t: int, x: matrix[float], i: int) -> int {
    let tombee: float = 0.0
    for k in range(t - 5, t + 1) {
        tombee += m.pluie[k]
    }
    let mois: float = float(int(m.date[t][5:7]))
    let heure: float = float(int(m.date[t][11:13]))
    x[i][0] = (m.pres[t] - 1013.0) / 10.0
    x[i][1] = (m.pres[t] - m.pres[t - 3]) / 2.0
    x[i][2] = (m.pres[t] - m.pres[t - 12]) / 5.0
    x[i][3] = m.hum[t] / 100.0
    x[i][4] = (m.hum[t] - m.hum[t - 3]) / 20.0
    x[i][5] = (m.temp[t] - 12.0) / 8.0
    x[i][6] = (m.temp[t] - m.temp[t - 3]) / 3.0
    x[i][7] = m.nuages[t] / 100.0
    x[i][8] = m.vent[t] / 20.0
    x[i][9] = min(tombee, 10.0) / 5.0
    x[i][10] = math.sin(2.0 * math.pi * mois / 12.0)
    x[i][11] = math.cos(2.0 * math.pi * mois / 12.0)
    x[i][12] = math.sin(2.0 * math.pi * heure / 24.0)
    x[i][13] = math.cos(2.0 * math.pi * heure / 24.0)
    return 0
}

// 1 s'il tombe au moins 0.5 mm dans les 6 heures qui suivent t, sinon 0
def pleut_ensuite(m: Meteo, t: int) -> float {
    let s: float = 0.0
    for k in range(t + 1, t + 7) {
        s += m.pluie[k]
    }
    if s >= 0.5 {
        return 1.0
    }
    return 0.0
}

// la prevision naive : il pleuvra si le seuil a ete atteint dans les 6 heures d'avant
def pleuvait_avant(m: Meteo, t: int) -> float {
    let s: float = 0.0
    for k in range(t - 5, t + 1) {
        s += m.pluie[k]
    }
    if s >= 0.5 {
        return 1.0
    }
    return 0.0
}

// ---- le reseau : 14 entrees, 16 neurones caches (tanh), une sortie (la probabilite de pluie) ----

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

def main() -> int {
    let fichier: string = "historique.csv"
    if not telecharger(fichier) {
        return 1
    }
    let m: Meteo = lire(fichier)
    let n: int = len(m.date)
    if n < 24 * 60 {
        print("Trop peu de données dans " + fichier + " (" + str(n) + " heures) : efface-le pour le télécharger de nouveau.")
        return 1
    }
    let derniere: int = int(m.date[n - 1][0:4])
    print(str(n) + " heures, du " + m.date[0][0:10] + " au " + m.date[n - 1][0:10] + " (" + latitude() + ", " + longitude() + ")")

    // les exemples : les annees d'avant pour apprendre, la derniere pour mesurer
    let n_appr: int = 0
    let n_mes: int = 0
    for t in range(12, n - 6) {
        if int(m.date[t][0:4]) < derniere {
            n_appr += 1
        } else {
            n_mes += 1
        }
    }
    if n_appr < 1000 or n_mes < 500 {
        print("Il faut au moins deux années de données.")
        return 1
    }
    let xa: matrix[float] = matrix(n_appr, nb_entrees())
    let xm: matrix[float] = matrix(n_mes, nb_entrees())
    let ya: list[float] = []
    let ym: list[float] = []
    let tm: list[int] = []
    for t in range(12, n - 6) {
        if int(m.date[t][0:4]) < derniere {
            entrees(m, t, xa, len(ya))
            ya.append(pleut_ensuite(m, t))
        } else {
            entrees(m, t, xm, len(ym))
            ym.append(pleut_ensuite(m, t))
            tm.append(t)
        }
    }
    print("Apprentissage sur " + str(n_appr) + " heures, mesure sur les " + str(n_mes) + " heures de " + str(derniere) + ".")

    // l'apprentissage : chaque passage voit les heures dans un ordre different
    seed(2024)
    let r: Reseau = creer_reseau(nb_entrees(), 16)
    let ordre: list[int] = []
    for i in range(n_appr) {
        ordre.append(i)
    }
    let passages: list[float] = []
    let pertes_appr: list[float] = []
    let pertes_mes: list[float] = []
    for passage in range(1, 11) {
        shuffle(ordre)
        let lr: float = 0.01 / (1.0 + 0.25 * float(passage - 1))
        for i in ordre {
            r.apprendre(xa, i, ya[i], lr)
        }
        passages.append(float(passage))
        pertes_appr.append(perte_moyenne(r, xa, ya))
        pertes_mes.append(perte_moyenne(r, xm, ym))
        print("passage " + str(passage) + " : perte " + format(pertes_appr[passage - 1], ".4f") + " (apprentissage), " + format(pertes_mes[passage - 1], ".4f") + " (" + str(derniere) + ")")
    }

    // la mesure sur la derniere annee
    let sec: Score = Score(0, 0, 0, 0, 0)
    let avant: Score = Score(0, 0, 0, 0, 0)
    let reseau: Score = Score(0, 0, 0, 0, 0)
    let proba: list[float] = []
    for i in range(n_mes) {
        let p: float = r.prevoir(xm, i)
        proba.append(p)
        let pluie: bool = ym[i] > 0.5
        compter(sec, false, pluie)
        compter(avant, pleuvait_avant(m, tm[i]) > 0.5, pluie)
        compter(reseau, p > 0.5, pluie)
    }
    print("")
    print("Sur " + str(derniere) + ", jamais vu pendant l'apprentissage :")
    print(ligne_score("  toujours sec          ", sec))
    print(ligne_score("  comme les 6 h d'avant ", avant))
    print(ligne_score("  réseau de neurones    ", reseau))

    // 1. comment l'erreur baisse, et ce que ferait une probabilite toujours la meme (la frequence de la pluie)
    let f: float = 0.0
    for v in ya {
        f += v
    }
    f = f / float(n_appr)
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

    // 2. les deux semaines de la derniere annee ou il a le plus plu
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
    plot.segment(0.0, 0.5, 14.0, 0.5)
    plot.color("gris")
    plot.width(1.0)
    plot.show()

    // 3. quand il annonce 70 %, pleut-il 7 fois sur 10 ?
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
    plot.title("Fiabilité : annoncé contre observé")
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
    write_file("essai_meteo.csv", "latitude,longitude,elevation,utc_offset_seconds,timezone,timezone_abbreviation\n48.84,2.36,43.0,3600,Europe/Paris,GMT+1\n\ntime,temperature_2m (°C),relative_humidity_2m (%),pressure_msl (hPa),precipitation (mm),cloud_cover (%),wind_speed_10m (km/h)\n2024-01-01T00:00,8.4,91,1012.3,0.20,100,14.8\n2024-01-01T01:00,8.1,93,,0.00,100,13.1\n2024-01-01T02:00,7.9,94,1011.6,1.40,98,12.9\n")
    let m: Meteo = lire("essai_meteo.csv")
    os.remove("essai_meteo.csv")
    assert_equal(len(m.date), 2)
    assert_equal(m.date[1], "2024-01-01T02:00")
    assert_equal(m.pres[1], 1011.6)
    assert_equal(m.pluie[1], 1.4)
    assert_equal(m.vent[0], 14.8)
}

test "les colonnes peuvent venir dans un autre ordre" {
    write_file("essai_meteo2.csv", "time,precipitation (mm),wind_speed_10m (km/h),temperature_2m (°C),cloud_cover (%),pressure_msl (hPa),relative_humidity_2m (%)\n2024-05-01T12:00,0.0,10.0,21.5,20,1020.0,40\n")
    let m: Meteo = lire("essai_meteo2.csv")
    os.remove("essai_meteo2.csv")
    assert_equal(len(m.date), 1)
    assert_equal(m.temp[0], 21.5)
    assert_equal(m.hum[0], 40.0)
}
