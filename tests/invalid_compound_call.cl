def suivant(k: int) -> int {
    return k + 1
}

def main() -> int {
    let xs: list[int] = [1, 2, 3]
    xs[suivant(0)] += 1
    print(xs[1])
    return 0
}
