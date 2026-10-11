def main() -> int {

    let total: int = 0
    for i in range(5) {
        if i == 2 {
            continue
        }
        total = total + 1
    }
    print(total)

    return 0
}
