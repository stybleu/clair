Class Team {
    leader: Person
    label: string
}

Class Person {
    name: string
    age: int
    score: float
    active: bool
    tags: list[string]
}


def birthday(p: Person) -> Person {
    p.age = p.age + 1
    return p
}

def main() -> int {
    let p: Person = Person("Alice", 30, 9, true, ["dev"])
    print(p.name)
    print(p.age)
    print(p.score)
    print(p.active)
    print(p.tags[0])

    p.name = "Bob"
    p.score = 9.5
    p.tags = ["dev", "clair"]

    let alias: Person = p
    alias.age = 40
    print(p.name)
    print(p.age)
    print(p.score)
    print(len(p.tags))

    let after: Person = birthday(p)
    print(after.age)

    let team: Team = Team(p, "Core")
    print(team.label)
    print(team.leader.name)
    print(team.leader.age)


    return 0
}
