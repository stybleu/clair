def main() -> int {

    let a: list[int] = [10, 20]
    let b: list[int] = move(a)

    let flag: int = input_int()
    if flag > 0 {
        a = [30, 40]
    }
    print(a[0])

    return 0
}
