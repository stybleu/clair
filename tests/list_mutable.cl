def mutate(values: list[int]) -> int {
    values[0] = 100
    values.append(50)
    return len(values)
}

def main() -> int {
    let values: list[int] = [10, 20, 30]
    values[1] = 25
    values.append(40)
    print(values[1])
    print(values[3])
    print(len(values))

    print(mutate(values))
    print(values[0])
    print(values[4])

    let empty: list[int] = []
    empty.append(7)
    empty[0] = 8
    print(len(empty))
    print(empty[0])

    return 0
}
