def main() -> int {

    let a: list[int] = [10]
    for i in range(2) {
        let a: list[int] = [20]
        if i == 0 {
            break
        }
    }
    let b: list[int] = move(a)
    print(b[0])

    return 0
}
