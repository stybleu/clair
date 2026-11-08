// V0.42 : grands nombres dans des fonctions, recursion, boucles, break, continue,
// variables masquees, alias, champs et methodes ; resultats identiques a Python
Class Compte {
    nom: string
    solde: bigint
    taux: bigfloat

    def depose(v: bigint) -> int {
        self.solde = self.solde + v
        return 0
    }

    def double() -> bigint {
        let s: bigint = self.solde
        return s + s
    }
}

def fib(n: int) -> bigint {
    if n < 2 {
        return bigint("1")
    }
    return fib(n - 1) + fib(n - 2)
}

def fact(n: int) -> bigint {
    if n <= 1 {
        return bigint("1")
    }
    let r: bigint = fact(n - 1)
    let k: bigint = bigint("1")
    for i in range(n - 1) {
        k = k + bigint("1")
    }
    return r * k
}

def cherche(limite: bigint) -> int {
    let x: bigint = bigint("1")
    let deux: bigint = bigint("2")
    let i: int = 0
    while true {
        x = x * deux
        i += 1
        if x > limite {
            return i
        }
    }
    return 0 - 1
}

def saute(n: int) -> bigint {
    let s: bigint = bigint("0")
    let un: bigint = bigint("1")
    for i in range(n) {
        s = s + un
        if i % 3 == 0 {
            continue
        }
        s = s + un
        if i > 100000 {
            break
        }
    }
    return s
}

def ombre(a: bigint) -> bigint {
    let x: bigint = a * a
    if a > bigint("0") {
        let x: bigint = a + a
        let y: bigint = x * x
        return y - x
    }
    return x
}

def alias(a: bigint) -> bigint {
    let b: bigint = a
    let c: bigint = b
    a = a + bigint("1")
    return a * b * c
}

def flottant(n: int) -> bigfloat {
    let x: bigfloat = bigfloat("1", 50)
    let trois: bigfloat = bigfloat("3", 50)
    for i in range(n) {
        x = x / trois + bigfloat("1", 50)
    }
    return x
}

def main() -> int {
    print(fib(25))
    print(fact(30))
    print(cherche(bigint("1000000000000000000000000000000")))
    print(saute(200000))
    print(ombre(bigint("12345678901234567890")))
    print(alias(bigint("99999999999999999999")))
    let c: Compte = Compte("Alice", bigint("100"), bigfloat("0.5", 20))
    for i in range(1000) {
        c.depose(bigint("12345678901234567890"))
    }
    print(c.solde)
    print(c.double() + fib(10) * c.double())
    print(flottant(2000))
    let total: bigint = bigint("0")
    for i in range(20000) {
        total = total + fib(5) * alias(bigint("7"))
    }
    print(total)
    return 0
}
