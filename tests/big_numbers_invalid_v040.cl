// Doit etre refuse a la compilation : melange bigint / float sans conversion.
// (V0.44 : bigint + int est permis, comme en Python ; bigint + float reste refuse.)
def main() -> int {
    let x: bigint = bigint("12345678901234567890")
    print(x + 1.5)
    return 0
}
