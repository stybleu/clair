def main() -> int {

    let a: list[int] = [10, 20]
    let b: list[int] = move(a)

    a = [30, 40]
    print(a[0])

    return 0
}
