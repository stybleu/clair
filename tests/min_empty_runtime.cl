// V0.75 : min() d une liste vide arrete le programme, comme Python (ValueError)
def main() {
    let xs: list[int] = [4]
    print(min(xs))
    let vide: list[int] = []
    print(min(vide))
    print(0)
}
