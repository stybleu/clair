
Class Holder {
    items: list[int]
}

def main() -> int {

    let a: list[int] = [10, 20]
    let b: list[int] = move(a)
    let holder: Holder = Holder(b)
    let retained: list[int] = holder.items
    print(retained[0])

    return 0
}
