// Plusieurs coeurs : grands produits de matrices et grandes couches denses repartis
// sur tous les coeurs. Le resultat est identique au bit pres au calcul simple,
// quel que soit le nombre de coeurs (CLAIR_THREADS=1, 2, 3, 7...).

def remplir(m: matrix[float], seed: int) -> int {
    for i in range(rows(m)) {
        for j in range(cols(m)) {
            m[i][j] = float((i * 7 + j * 3 + seed) % 11) / 7.0 - 0.6
        }
    }
    return 0
}

def liste(n: int, seed: int) -> list[float] {
    let out: list[float] = []
    for i in range(n) {
        out.append(float((i * 5 + seed) % 13) / 9.0 - 0.7)
    }
    return out
}

// reconnu : noyau en blocs, sur tous les coeurs
def produit(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float = a[i][k]
            for j in range(cols(b)) {
                c[i][j] = c[i][j] + aik * b[k][j]
            }
        }
    }
    return 0
}

// meme calcul, non reconnu (une instruction de plus)
def produit_simple(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    let tours: int = 0
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float = a[i][k]
            for j in range(cols(b)) {
                c[i][j] = c[i][j] + aik * b[k][j]
                tours = tours + 1
            }
        }
    }
    return tours
}

def ecarts(x: matrix[float], y: matrix[float]) -> int {
    let d: int = 0
    for i in range(rows(x)) {
        for j in range(cols(x)) {
            if x[i][j] != y[i][j] {
                d = d + 1
            }
        }
    }
    return d
}

def total(x: matrix[float]) -> float {
    let t: float = 0.0
    for i in range(rows(x)) {
        for j in range(cols(x)) {
            t = t + x[i][j]
        }
    }
    return t
}

def essai_produit(m: int, k: int, n: int) -> int {
    let a: matrix[float] = matrix(m, k)
    let b: matrix[float] = matrix(k, n)
    let c1: matrix[float] = matrix(m, n)
    let c2: matrix[float] = matrix(m, n)
    remplir(a, 1)
    remplir(b, 2)
    remplir(c1, 3)
    remplir(c2, 3)
    produit(a, b, c1)
    produit_simple(a, b, c2)
    print(total(c1))
    return ecarts(c1, c2)
}

// reconnue : les sommes sur tous les coeurs, puis la suite ligne par ligne, dans l'ordre
def couche(w: matrix[float], b: list[float], x: list[float], y: list[float], ordre: list[float]) -> int {
    let rang: int = 0
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = max(s, 0.0) + s * 0.25
        ordre[i] = float(rang)
        rang = rang + 1
    }
    return rang
}

def couche_simple(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    let tours: int = 0
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
            tours = tours + 1
        }
        y[i] = max(s, 0.0) + s * 0.25
    }
    return tours
}

// reconnue, avec un return au milieu des lignes
def premiere_au_dessus(w: matrix[float], x: list[float], seuil: float) -> int {
    for i in range(rows(w)) {
        let s: float = 0.0
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        if s > seuil {
            return i
        }
    }
    return -1
}

def premiere_simple(w: matrix[float], x: list[float], seuil: float) -> int {
    let tours: int = 0
    for i in range(rows(w)) {
        let s: float = 0.0
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
            tours = tours + 1
        }
        if s > seuil {
            return i
        }
    }
    return -1
}

def differences(a: list[float], b: list[float]) -> int {
    let d: int = 0
    for i in range(len(a)) {
        if a[i] != b[i] {
            d = d + 1
        }
    }
    return d
}

def essai_couche(m: int, n: int) -> int {
    let w: matrix[float] = matrix(m, n)
    remplir(w, 4)
    let b: list[float] = liste(m, 1)
    let x: list[float] = liste(n, 2)
    let y1: list[float] = liste(m, 0)
    let y2: list[float] = liste(m, 0)
    let ordre: list[float] = liste(m, 0)
    let rang: int = couche(w, b, x, y1, ordre)
    couche_simple(w, b, x, y2)
    let desordre: int = 0
    for i in range(m) {
        if ordre[i] != float(i) {
            desordre = desordre + 1
        }
    }
    let t: float = 0.0
    for v in y1 {
        t = t + v
    }
    print(t)
    print(rang - m + desordre)
    return differences(y1, y2)
}

def main() -> int {
    print(essai_produit(160, 150, 170))
    print(essai_produit(97, 33, 500))
    print(essai_produit(3, 2, 5))
    print(essai_couche(300, 257))
    print(essai_couche(1000, 40))
    print(essai_couche(9, 5))

    // somme de la ligne i : environ 0.3 * i
    let w: matrix[float] = matrix(800, 300)
    let x: list[float] = []
    for j in range(300) {
        x.append(1.0)
    }
    for i in range(800) {
        for j in range(300) {
            w[i][j] = float(i) * 0.001 + float(j % 7) * 0.0001
        }
    }
    print(premiere_au_dessus(w, x, 150.0))
    print(premiere_simple(w, x, 150.0))
    print(premiere_au_dessus(w, x, 1000.0))
    return 0
}
