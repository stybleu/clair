def is_valid(x: int) -> bool {
    return x > 0 and x < 100
}

def bool_echo(value: bool) -> bool {
    return value
}

def marker() -> bool {
    print(999)
    return true
}

def main() -> int {
    let active: bool = true
    let inactive: bool = false

    if active and not inactive {
        print(1)
    } else {
        print(0)
    }

    if is_valid(50) and not is_valid(100) {
        print(2)
    }

    if false or true and false {
        print(99)
    } else {
        print(3)
    }

    if (false or true) and not false {
        print(4)
    }

    print(true)
    print(false)

    if bool_echo(true) {
        print(5)
    }

    // and doit etre prioritaire sur or : true or (false and false)
    if true or false and false {
        print(6)
    } else {
        print(66)
    }

    // marker() ne doit jamais etre appelee grace au court-circuit.
    if false and marker() {
        print(88)
    }

    if true or marker() {
        print(7)
    }

    return 0
}
