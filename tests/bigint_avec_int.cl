// V0.44 : un bigint avec un int, comme en Python (sans bigint(k))
def main() -> int {
    let a: bigint = bigint("123456789012345678901234567890")
    let k: int = 0 - 7
    print(a + 1)
    print(a - k)
    print(1 - a)
    print(a * k)
    print(a * 0)
    print(a / k)
    print(a % k)
    print(1000000 / a)
    print(1000000 % bigint(7))
    print(a + a * 9)
    print(a * 3 + a)
    print(a + a * k)
    print(a > 5)
    print(5 > a)
    print(a == 0)
    print(bigint(0) == 0)
    let f: bigint = bigint(1)
    for i in range(1, 31) {
        f = f * i
    }
    print(f)
    let u: u32 = u32(4000000000)
    print(f / u)
    return 0
}
