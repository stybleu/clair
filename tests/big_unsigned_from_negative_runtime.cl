// Un int negatif ne peut pas devenir un uint
def main() -> int {
    let n: int = 7
    let u: uint256 = n
    print(u)
    n = -7
    let v: uint256 = n
    print(v)
    return 0
}
