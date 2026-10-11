
def identity(values: list[int]) -> list[int] {
    return values
}
def main() -> int {

    let a: list[int] = [10]
    let b: list[int] = move(a)

    let flag: int = input_int()
    if flag > 0 {
        let leaked: list[int] = identity(b)
    }

    return 0
}
