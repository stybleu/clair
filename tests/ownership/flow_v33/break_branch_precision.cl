def main() -> int {

    let a: list[int] = [10]
    let b: list[int] = move(a)

    let flag: int = input_int()
    while flag > 0 {
        if flag == 1 {
            break
        } else {
            a = [20]
        }
        print(a[0])
        break
    }

    return 0
}
