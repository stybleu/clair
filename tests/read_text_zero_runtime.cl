def main() -> int {
    write_file("build/zero.bin", [65, 0, 66] * 2)
    print(len(read_bytes("build/zero.bin")))
    print(read_text("build/zero.bin"))
    return 0
}
