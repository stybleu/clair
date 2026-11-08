def consume(values: list[int]) -> int {
    return values[0]
}

def main() -> int {
    let fixed: list[int] = [10, 20, 30]
    fixed[1] = 25
    print(fixed[1])
    print(len(fixed))

    let grows: list[int] = [1, 2]
    grows.append(3)
    print(len(grows))

    let passed: list[int] = [7, 8]
    print(consume(passed))

    let aliased: list[int] = [4, 5]
    let alias: list[int] = aliased
    alias[0] = 9
    print(aliased[0])

    return 0
}
