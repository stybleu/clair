// Nombres aleatoires : memes tirages que le module random de Python pour la meme graine

def des(n: int) -> list[int] {
    let compte: list[int] = [0, 0, 0, 0, 0, 0]
    for k in range(n) {
        let face: int = randint(1, 6)
        compte[face - 1] += 1
    }
    return compte
}

def main() -> int {
    seed(2026)
    print(random())
    print(random())
    print(randint(1, 6))
    print(randint(-50, 50))
    print(randint(7, 7))
    print(randint(0, 4294967296))
    print(randint(-9223372036854775807 - 1, 9223372036854775807))
    print(uniform(-1.0, 1.0))
    print(uniform(10, 20))
    for k in range(5) {
        print(gauss(0.0, 1.0))
    }
    for v in des(6000) {
        print(v)
    }

    // melanger des donnees, tirer au hasard
    let xs: list[int] = []
    for i in range(20) {
        xs.append(i * i)
    }
    shuffle(xs)
    print(xs[0])
    print(xs[19])
    let mots: list[string] = ["chat", "chien", "loup", "renard", "ours"]
    shuffle(mots)
    print(mots[0])
    print(choice(mots))
    let octets: list[u8] = [1, 2, 3, 4, 5, 6, 7, 8]
    shuffle(octets)
    print(octets[0] + octets[7])

    // initialiser les poids d'une couche : loi normale, en float32
    let n: int = 64
    let w: matrix[float32] = matrix(n, n)
    for i in range(n) {
        for j in range(n) {
            w[i][j] = gauss(0.0, 0.125)
        }
    }
    let total: float32 = 0.0
    for i in range(n) {
        for j in range(n) {
            total += w[i][j] * w[i][j]
        }
    }
    print(total)
    print(w[3][5])

    // la meme graine redonne les memes tirages
    seed(99)
    let a: float = random()
    let g: float = gauss(1.0, 0.5)
    seed(99)
    print(random() == a)
    print(gauss(1.0, 0.5) == g)
    seed(-99)
    print(random() == a)
    return 0
}
