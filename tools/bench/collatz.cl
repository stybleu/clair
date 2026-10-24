def steps(n: int) -> int {
    let count: int = 0
    let x: int = n
    while x != 1 {
        if x / 2 * 2 == x {
            x = x / 2
        } else {
            x = 3 * x + 1
        }
        count = count + 1
    }
    return count
}

def main() -> int {
    let best: int = 0
    for i in range(1, 1200000) {
        let s: int = steps(i)
        if s > best {
            best = s
        }
    }
    print(best)
    return 0
}
