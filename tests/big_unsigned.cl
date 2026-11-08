// Entiers sans signe uint128 .. uint1024, et abs, min, max, pow sur les grands entiers

Class Graine {
    nom: string
    etat: uint128
}

// generateur pseudo-aleatoire 128 bits (multiplication, puis reste modulo 2^127 - 1)
def suivant(x: uint128) -> uint128 {
    let premier: uint256 = 170141183460469231731687303715884105727
    let p: uint256 = x
    return uint128((p * 6364136223846793005 + 1442695040888963407) % premier)
}

def factorielle(n: int) -> uint1024 {
    let r: uint1024 = 1
    for i in range(2, n + 1) {
        r = r * i
    }
    return r
}

// combinaisons n parmi k, exactes
def combinaisons(n: int, k: int) -> uint512 {
    let r: uint512 = 1
    for i in range(k) {
        r = r * (n - i) / (i + 1)
    }
    return r
}

def main() -> int {
    let m: uint128 = 340282366920938463463374607431768211455
    print(m)
    print(m / 3)
    print(m % 1000)
    let a: uint256 = m
    print(a * a)
    print(a + 1)
    print(m > 5)
    print(m == a)

    print(factorielle(160))
    print(combinaisons(200, 100))

    let g: Graine = Graine("x", 42)
    for i in range(5) {
        g.etat = suivant(g.etat)
    }
    print(g.etat)

    let n: int = 12
    let b: uint128 = n
    print(b - 2)
    print(int(b) * 3)
    print(float(m))
    print(uint128(int256(m) - 1))
    print(int256(m) + 1)

    // abs, min, max, pow
    let h: int256 = -170141183460469231731687303715884105728
    print(abs(h))
    print(min(h, 3))
    print(max(h, 3))
    print(min(a, 7))
    print(max(a, m))
    print(pow(b, 30))
    print(pow(int128(-3), 77))
    print(pow(h, 0))
    let deux: int1024 = pow(2, 1000)
    print(deux)
    print((pow(uint1024(2), 1023) - 1) * 2 + 1)
    return 0
}
