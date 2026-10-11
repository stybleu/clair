
Class Holder {
    items: list[int]
}

def main() -> int {

    let a: list[int] = [10, 20]
    let holder: Holder = Holder(a)
    let b: list[int] = move(a)
    let retained: list[int] = holder.items
    print(retained[0])

    return 0
}
