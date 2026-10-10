Class Person {
    age: int
}

def main() -> int {
    let a: Person = Person(1)
    let b: Person = Person(1)
    if a == b {
        print(1)
    }
    return 0
}
