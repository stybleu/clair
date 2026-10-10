def identity(value: string) -> string {
    return value
}

def same(a: string, b: string) -> bool {
    return a == b
}

def main() -> int {
    let greeting: string = "Hello"
    print(greeting)
    print(identity("Clair"))

    if same(greeting, "Hello") {
        print(1)
    }

    if greeting != "World" {
        print(2)
    }

    print(len(greeting))

    let copy: string = greeting
    print(copy)

    copy = "Changed"
    print(copy)

    return 0
}
