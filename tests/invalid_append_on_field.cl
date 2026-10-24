Class Team {
    names: list[string]
}

def main() -> int {
    let t: Team = Team(["a"])
    t.names.append("b")
    return 0
}
