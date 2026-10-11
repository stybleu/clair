def main() -> int {

    let a: list[int] = [10, 20]
    let b: list[int] = move(a)

    let flag: int = input_int()
    while flag > 0 {
        a = [30, 40]
        flag = 0
    }
    print(a[0])

    return 0
}
