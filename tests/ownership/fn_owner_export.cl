
def identity(values: list[int]) -> list[int] {
    return values
}

def main() -> int {

    let a: list[int] = [10, 20]
    let b: list[int] = move(a)
    let retained: list[int] = identity(b)
    print(retained[0])

    return 0
}
