Class Counter {
    count: int

    def add(n: int) -> int {
        self.count = self.count + n
        return self.count
    }
}

def main() -> int {
    let c: Counter = Counter(0)
    print(c.add(1.5))
    return 0
}
