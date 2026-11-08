// Grands entiers : int128, int256, int512, int1024

Class Compte {
    nom: string
    solde: int256
}

def factorielle(n: int) -> int1024 {
    let r: int1024 = 1
    for i in range(2, n + 1) {
        r = r * i
    }
    return r
}

def fibonacci(n: int) -> int256 {
    let a: int256 = 0
    let b: int256 = 1
    for i in range(n) {
        let c: int256 = a + b
        a = b
        b = c
    }
    return a
}

def puissance(base: int, exposant: int) -> int1024 {
    let r: int1024 = 1
    let i: int = 0
    while i < exposant {
        r = r * base
        i = i + 1
    }
    return r
}

def main() -> int {
    let max128: int128 = 170141183460469231731687303715884105727
    let min128: int128 = -170141183460469231731687303715884105728
    print(max128)
    print(min128)
    print(max128 + min128)

    let g: int256 = 123456789012345678901234567890
    print(g * g)
    print(g / 7)
    print(g % 7)
    print(-g / 7)
    print(-g % 7)
    print(g > 1000)
    print(g == g)

    print(factorielle(100))
    print(fibonacci(300))
    print(puissance(2, 1000))
    print(puissance(3, 600) / puissance(3, 599))

    let petit: int = 42
    let mix: int512 = g
    print(mix + petit)
    print(int(g / g) + petit)
    print(float(g))
    print(int128(g / 1000000000000000))

    let c: Compte = Compte("Ada", 1000000000000000000000)
    c.solde = c.solde * 3
    print(c.solde)

    let n: int1024 = 1
    let chiffres: int = 0
    while n < puissance(10, 300) {
        n = n * 10
        chiffres = chiffres + 1
    }
    print(chiffres)
    print(-9223372036854775808)
    return 0
}
