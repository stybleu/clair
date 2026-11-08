// pow() d'un grand entier : exposant 0 ou plus
def main() -> int {
    let x: int512 = 3
    let e: int = 2
    print(pow(x, e))
    e = e - 3
    print(pow(x, e))
    return 0
}
