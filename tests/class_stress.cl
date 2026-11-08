Class Point {
    x: int
    y: int
}

def main() -> int {
    let last: Point = Point(0, 0)
    for i in range(0, 100) {
        last = Point(i, i + 1)
    }
    print(last.x)
    print(last.y)
    return 0
}
