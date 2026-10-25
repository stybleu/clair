// Syntaxe Python : x += v (et -= *= /= %=), for i, v in enumerate(xs), tranches xs[a:b:c]

Class Stats {
    total: float
    vus: list[int]
}

// produit de matrices ecrit avec += : reconnu comme c[i][j] = c[i][j] + ...
def produit(a: matrix[float], b: matrix[float], c: matrix[float]) -> int {
    for i in range(rows(a)) {
        for k in range(cols(a)) {
            let aik: float = a[i][k]
            for j in range(cols(b)) {
                c[i][j] += aik * b[k][j]
            }
        }
    }
    return 0
}

// couche dense ecrite avec += : reconnue, sur tous les coeurs
def couche(w: matrix[float], x: list[float], y: list[float]) -> int {
    for i in range(rows(w)) {
        let s: float = 0.0
        for j in range(cols(w)) {
            s += w[i][j] * x[j]
        }
        y[i] = max(s, 0.0)
    }
    return 0
}

def montre(v: list[int]) -> int {
    let texte: string = ""
    for i, x in enumerate(v) {
        if i > 0 {
            texte += ","
        }
        if x < 0 {
            texte += "-"
        }
        texte += "#"
    }
    print(texte)
    return len(v)
}

def main() -> int {
    // += -= *= /= %= sur des variables
    let n: int = 7
    n += 5
    n -= 2
    n *= 3
    n /= 4
    n %= 5
    print(n)
    let x: float = 2.0
    x += 0.5
    x *= 3.0
    x /= 2.0
    x -= 0.25
    print(x)
    let mot: string = "py"
    mot += "thon"
    print(mot)
    let grand: int256 = 1
    for i in range(1, 40) {
        grand *= i
    }
    print(grand)

    // sur des cases de liste, de matrice, des champs
    let xs: list[int] = [5, 3, 8, 1, 9, 2]
    xs[0] += 10
    xs[len(xs) - 1] *= 7
    print(xs[0])
    print(xs[5])
    let st: Stats = Stats(0.0, [0, 0, 0])
    for v in xs {
        st.total += float(v)
        st.vus[v % 3] += 1
    }
    print(st.total)
    print(st.vus[0])
    print(st.vus[1])
    print(st.vus[2])

    // enumerate
    let somme: int = 0
    for i, v in enumerate(xs) {
        somme += i * v
    }
    print(somme)
    for rang, v in enumerate(xs, 1) {
        if rang == 2 {
            continue
        }
        if v > 50 {
            break
        }
        print(rang)
    }
    let pousse: list[int] = [1, 2]
    for i, v in enumerate(pousse) {
        if i < 3 {
            pousse.append(v * 10)
        }
        i = 100
    }
    print(len(pousse))
    print(pousse[4])

    // tranches
    print(montre(xs[1:4]))
    print(montre(xs[::-2]))
    print(montre(xs[-2:]))
    print(montre(xs[10:]))
    print(montre(xs[:-10]))
    let fl: list[float] = [0.5, 1.5, 2.5, 3.5]
    let milieu: list[float] = fl[1:3]
    print(milieu[0] + milieu[1])
    let noms: list[string] = ["ada", "bob", "cy", "dan"]
    for i, nom in enumerate(noms[1:], 2) {
        print(nom[0:1] + nom[1:])
        print(i)
    }
    let s: string = "bonjour"
    print(s[3:])
    print(s[:3])
    print(s[::-1])
    print(s[1:-1:2])

    // les boucles ecrites avec += sont reconnues comme les autres
    let a: matrix[float] = matrix(3, 2)
    let b: matrix[float] = matrix(2, 3)
    let c: matrix[float] = matrix(3, 3)
    for i in range(3) {
        for j in range(2) {
            a[i][j] = float(i + j)
            b[j][i] = float(i * j + 1)
        }
    }
    produit(a, b, c)
    print(c[2][2])
    let y: list[float] = [0.0, 0.0, 0.0]
    couche(c, [1.0, -1.0, 0.5], y)
    print(y[0] + y[1] + y[2])
    return 0
}
