def max(a: int, b: int) -> int {
    if a > b {
        return a
    } else {
        return b
    }
}

def classify(x: int) -> int {
    if x == 10 {
        return 1
    }
    if x != 10 {
        return 2
    }
    return 0
}

def main() -> int {
    print(max(12, 7))
    print(max(3, 9))
    print(classify(10))
    print(classify(11))

    if 3 < 4 {
        print(100)
    }
    if 4 <= 4 {
        print(101)
    }
    if 5 >= 5 {
        print(102)
    }

    return 0
}
