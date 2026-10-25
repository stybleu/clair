// Doit etre refuse a la compilation : melange bigint / int sans conversion.
def main() -> int {
    let x: bigint = bigint("12345678901234567890")
    print(x + 1)
    return 0
}
