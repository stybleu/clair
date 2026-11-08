def make() -> list[int] {
    return [1]
}

def main() -> int {
    make().append(2)
    return 0
}
