def main() -> int {

    let a: list[int] = [10, 20]
    let flag: int = input_int()
    if flag > 0 {
        let b: list[int] = move(a)
        print(b[0])
    }

    return 0
}
