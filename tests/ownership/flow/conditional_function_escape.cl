
def identity(values: list[int]) -> list[int] {
    return values
}
def main() -> int {

    let a: list[int] = [10]
    let sink: list[int] = [0]
    let flag: int = input_int()
    if flag > 0 {
        sink = identity(a)
    }
    let b: list[int] = move(a)
    print(b[0])

    return 0
}
