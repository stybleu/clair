// Le bouton ▶ d'un IDE, ecrit en Clair : compile un programme Clair, montre ses
// erreurs, sinon le lance et montre ce qu'il ecrit pendant qu'il tourne.
//   ~/clair/build/clair lancer.cl -o lancer && ./lancer ../../tests/hello.cl
import os

def main() -> int {
    let a: list[string] = os.args()
    if len(a) == 0 {
        print("usage : ./lancer programme.cl")
        return 2
    }
    let source: string = a[0]
    if not os.is_file(source) {
        print("pas de fichier " + source)
        return 2
    }
    // le compilateur : $CLAIR, ou ~/clair/build/clair
    let clair: string = os.env("CLAIR", os.join(os.home(), "clair/build/clair"))
    let nom: string = os.basename(source)
    let binaire: string = os.join(os.env("TMPDIR", "/tmp"), "lancer_" + nom + ".bin")

    let t0: float = os.clock()
    let c: RunResult = os.run([clair, source, "-o", binaire])
    if c.code != 0 {
        print("--- " + nom + " ne compile pas :")
        write(c.err)
        return 1
    }
    print("--- " + nom + " compile en " + format(os.clock() - t0, ".2f") + " s")

    let t1: float = os.clock()
    let p: Process = os.start_merged([binaire])
    while p.running() {
        write(p.read())
        os.sleep(0.02)
    }
    write(p.read())
    let code: int = p.wait()
    print("--- termine, code " + str(code) + ", en " + format(os.clock() - t1, ".2f") + " s")
    os.remove(binaire)
    os.remove(binaire + ".generated.c")
    return 0
}
