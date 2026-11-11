// V0.74 : [[x] for x in xs] est une liste de listes ; une liste de bigint n existe pas
def main() -> int {
    let xs: list[int] = [1, 2]
    let ys = [bigint(x) for x in xs]
    return 0
}
