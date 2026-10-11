
Class Holder {
    items: list[int]
}

def main() -> int {

    let a: list[int] = [10, 20]
    let holder: Holder = Holder([0])
    holder.items = a
    let b: list[int] = move(a)
    print(b[0])

    return 0
}
