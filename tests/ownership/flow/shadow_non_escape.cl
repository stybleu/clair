def main() -> int {

    let a: list[int] = [10]
    let flag: int = input_int()
    if flag > 0 {
        let a: list[int] = [20]
        let alias: list[int] = a
    }
    let b: list[int] = move(a)
    print(b[0])

    return 0
}
