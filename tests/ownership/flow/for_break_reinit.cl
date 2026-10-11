def main() -> int {

    let a: list[int] = [10]
    let b: list[int] = move(a)

    for i in range(3) {
        if i == 1 {
            break
        }
        a = [20]
    }
    print(a[0])

    return 0
}
