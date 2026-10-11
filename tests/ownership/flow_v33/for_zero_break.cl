def main() -> int {

    let a: list[int] = [10]
    let b: list[int] = move(a)

    for i in range(0) {
        a = [20]
        break
    }
    print(a[0])

    return 0
}
