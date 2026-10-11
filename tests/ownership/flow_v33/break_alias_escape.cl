def main() -> int {

    let a: list[int] = [10]
    let sink: list[int] = [0]
    let flag: int = input_int()
    while flag > 0 {
        sink = a
        break
    }
    let b: list[int] = move(a)
    print(b[0])

    return 0
}
