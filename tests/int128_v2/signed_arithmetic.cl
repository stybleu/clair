def main() -> int {

    let a: int128 = 170141183460469231731687303715884105726
    let one: int128 = 1
    let minimum: int128 = -170141183460469231731687303715884105728
    let hundred: int128 = 100
    let three: int128 = 3

    print(a + one)
    print(a - one)
    print(hundred * three)
    print(hundred / three)
    print(minimum / one)
    print(a > one)

    return 0
}
