
def identity(values: list[int]) -> list[int] {
    return values
}

def main() -> int {

    let a: list[int] = [10, 20]
    let retained: list[int] = identity(a)
    let b: list[int] = move(a)
    print(retained[0])

    return 0
}
