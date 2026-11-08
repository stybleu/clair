// Matrice x vecteur (couche dense) : 4 lignes a la fois, resultat identique au bit pres

Class Couche {
    w: matrix[float]
    b: list[float]

    def avant(x: list[float], y: list[float]) -> int {
        for i in range(rows(self.w)) {
            let s: float = self.b[i]
            for j in range(cols(self.w)) {
                s = s + self.w[i][j] * x[j]
            }
            y[i] = max(s, 0.0)
        }
        return 0
    }
}

def fill(n: int, seed: int) -> list[float] {
    let out: list[float] = []
    for i in range(n) {
        out.append(float((i * 7 + seed) % 11) / 3.0 - 1.5)
    }
    return out
}

def matrice(m: int, n: int) -> matrix[float] {
    let w: matrix[float] = matrix(m, n)
    for i in range(m) {
        for j in range(n) {
            w[i][j] = float((i * 5 + j * 3) % 13) / 7.0 - 0.9
        }
    }
    return w
}

// reconnue
def couche(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = 1.0 / (1.0 + exp(-s))
    }
    return 0
}

// meme calcul, non reconnu (une instruction de plus dans la boucle)
def couche_simple(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    let tours: int = 0
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
            tours = tours + 1
        }
        y[i] = 1.0 / (1.0 + exp(-s))
    }
    return tours
}

// liste a plat, depart 0.0, numero de ligne affiche ligne par ligne
def plat(w: list[float], x: list[float], y: list[float], m: int, n: int) -> float {
    let total: float = 0.0
    for i in range(m) {
        let s: float = 0.0
        for j in range(n) {
            s = x[j] * w[i * n + j] + s
        }
        total = total + s
        y[i] = s
        print(i)
    }
    return total
}

def plat_simple(w: list[float], x: list[float], y: list[float], m: int, n: int) -> float {
    let total: float = 0.0
    let tours: int = 0
    for i in range(m) {
        let s: float = 0.0
        for j in range(n) {
            s = x[j] * w[i * n + j] + s
            tours = tours + 1
        }
        total = total + s
        y[i] = s
    }
    return total
}

def couche_relu(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    let tours: int = 0
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
            tours = tours + 1
        }
        y[i] = max(s, 0.0)
    }
    return tours
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

def essai(m: int, n: int) -> int {
    let w: matrix[float] = matrice(m, n)
    let b: list[float] = fill(m, 1)
    let x: list[float] = fill(n, 2)
    let y1: list[float] = fill(m, 0)
    let y2: list[float] = fill(m, 0)
    couche(w, b, x, y1)
    couche_simple(w, b, x, y2)
    return differences(y1, y2)
}

def main() -> int {
    print(essai(1, 1))
    print(essai(4, 3))
    print(essai(7, 300))
    print(essai(130, 17))

    let c: Couche = Couche(matrice(6, 5), fill(6, 3))
    let x: list[float] = fill(5, 4)
    let y: list[float] = fill(6, 0)
    let y2: list[float] = fill(6, 0)
    c.avant(x, y)
    couche_relu(matrice(6, 5), fill(6, 3), x, y2)
    print(differences(y, y2))

    let p1: list[float] = fill(5, 0)
    let p2: list[float] = fill(5, 0)
    let t1: float = plat(fill(15, 5), fill(3, 6), p1, 5, 3)
    let t2: float = plat_simple(fill(15, 5), fill(3, 6), p2, 5, 3)
    print(differences(p1, p2))
    print(t1 == t2)

    // y est aussi x : les 4 lignes ne sont pas utilisees, resultat de la boucle simple
    let w: matrix[float] = matrice(5, 5)
    let v: list[float] = fill(5, 7)
    let v2: list[float] = fill(5, 7)
    couche(w, fill(5, 1), v, v)
    couche_simple(w, fill(5, 1), v2, v2)
    print(differences(v, v2))
    return 0
}
