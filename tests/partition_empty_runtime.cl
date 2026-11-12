// V0.76 : partition avec un separateur vide arrete le programme, comme Python (ValueError)
def main() {
    print("a=b".partition("=")[2])
    let vide = ""
    print("abc".partition(vide)[0])
}
