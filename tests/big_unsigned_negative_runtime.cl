// Un uint ne devient jamais negatif : 3 - 5 arrete le programme
def main() -> int {
    let a: uint128 = 3
    let b: uint128 = 5
    print(b - a)
    print(a - b)
    print(1)
    return 0
}
