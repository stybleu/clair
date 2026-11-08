// Clair V0.40 : tests d'integration des entiers et flottants dynamiques.
def main() -> int {
    let a: bigint = bigint("123456789012345678901234567890123456789")
    let b: bigint = bigint("987654321098765432109876543210987654321")
    let c: bigint = (a * b) / b
    if c != a {
        print("ECHEC bigint division")
        return 1
    }
    if (a + b) - b != a {
        print("ECHEC bigint addition")
        return 2
    }
    print(c)
    let x: bigfloat = bigfloat("1.616255e-35", 80)
    let y: bigfloat = bigfloat("1e-1000000000", 80)
    if y >= x {
        print("ECHEC bigfloat ordre")
        return 3
    }
    let z: bigfloat = (x * x) / x
    if z != x {
        print("ECHEC bigfloat division")
        return 4
    }
    print(y)
    print("CLAIR V0.40 : TESTS OK")
    return 0
}
