Class Counter {
    count: int
    step: int

    def increment() -> int {
        self.count = self.count + self.step
        return self.count
    }

    def add(n: int) -> int {
        self.count = self.count + n
        return self.count
    }

    def twice() -> int {
        self.increment()
        return self.increment()
    }

    def is_zero() -> bool {
        return self.count == 0
    }
}

Class Person {
    name: string

    def greeting() -> string {
        return "Hello " + self.name
    }

    age: int

    def birthday() -> int {
        self.age = self.age + 1
        return self.age
    }

    def older_than(other: Person) -> bool {
        return self.age > other.age
    }
}

Class Team {
    leader: Person
    size: int

    def leader_birthday() -> int {
        return self.leader.birthday()
    }
}

Class Bag {
    items: list[int]

    def total() -> int {
        let sum: int = 0
        for i in range(len(self.items)) {
            sum = sum + self.items[i]
        }
        return sum
    }
}

def bump(c: Counter) -> int {
    return c.add(1)
}

def main() -> int {
    let c: Counter = Counter(0, 5)
    print(c.is_zero())
    print(c.increment())
    print(c.add(10))
    print(c.twice())
    print(c.is_zero())
    c.increment()
    c.add(70)
    print(c.count)

    let alias: Counter = c
    alias.add(1)
    print(c.count)
    bump(c)
    print(c.count)

    let p: Person = Person("Alice", 30)
    let q: Person = Person("Bob", 25)
    print(p.greeting())
    print(p.older_than(q))
    print(q.older_than(p))
    p.birthday()
    print(p.age)

    let team: Team = Team(q, 1)
    print(team.leader_birthday())
    team.leader.birthday()
    print(q.age)
    team.leader.age = 40
    print(q.age)
    print(team.leader.greeting())

    print(Person("Zed", 9).birthday())

    let bag: Bag = Bag([1, 2, 3])
    print(bag.total())

    return 0
}
