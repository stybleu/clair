def main() -> int {

    let a: list[int] = [10]
    let b: list[int] = move(a)

    let flag: int = input_int()
    if flag > 0 {
        a = [20]
    }
    if flag == 0 {
        a = [30]
    }
    print(a[0])

    return 0
}
