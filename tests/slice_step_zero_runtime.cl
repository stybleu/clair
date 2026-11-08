// Comme en Python, un pas de 0 arrete le programme
def main() -> int {
    let xs: list[int] = [1, 2, 3, 4]
    let pas: int = 2
    print(len(xs[::pas]))
    pas -= 2
    print(len(xs[::pas]))
    return 0
}
