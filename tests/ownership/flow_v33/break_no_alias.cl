def main() -> int {

    let a: list[int] = [10]
    let flag: int = input_int()
    while flag > 0 {
        break
    }
    let b: list[int] = move(a)
    print(b[0])

    return 0
}
