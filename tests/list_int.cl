def first(values: list[int]) -> int {
    return values[0]
}

def identity(values: list[int]) -> list[int] {
    return values
}

def make_values() -> list[int] {
    return [4, 5, 6]
}

def main() -> int {
    let values: list[int] = [10, 20, 30]
    print(values[0])
    print(values[1 + 1])
    print(len(values))
    print(first(values))

    let copy: list[int] = identity(values)
    print(copy[1])

    let made: list[int] = make_values()
    print(made[2])
    print([100, 200][1])

    let empty: list[int] = []
    print(len(empty))

    values = [7, 8]
    print(values[0])
    print(len(values))

    return 0
}
