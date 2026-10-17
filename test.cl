def sum_list(a: int, b: int, c: int) -> int {
    let values: list[int] = [a, b, c]

    return values[0] + values[1] + values[2]
}

def main() -> int {
    let a: int = input_int()
    let b: int = input_int()
    let c: int = input_int()

    print(sum_list(a, b, c))
    return 0
}
