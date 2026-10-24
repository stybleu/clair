Class Counter {
    count: int
}

def main() -> int {
    let c: Counter = Counter(0)
    print(c.count())
    return 0
}
