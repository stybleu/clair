def main() -> int {

    let a: list[int] = [10, 20]
    let b: list[int] = move(a)

    for i in range(0) {
        a = [30, 40]
    }
    print(a[0])

    return 0
}
