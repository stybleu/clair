// V0.56 : le module os (programmes, fichiers, dossiers, chemins, temps)
import os

def main() -> int {
    for a in os.args() {
        print("arg " + a)
    }
    let r: RunResult = os.run(["echo", "bonjour", "le monde"])
    print(r.code)
    print(r.out)
    let s: RunResult = os.shell("echo sortie; echo erreur 1>&2; exit 3")
    print(str(s.code) + " [" + s.out + "] [" + s.err + "]")
    let t: RunResult = os.run_with(["cat"], "ligne 1\nligne 2\n", 5.0)
    print(t.out)
    let u: RunResult = os.run(["programme_qui_nexiste_pas"])
    print(str(u.code) + " " + u.err)
    print(os.mkdir("dossier/sous"))
    print(os.exists("dossier/sous"))
    print(os.is_dir("dossier"))
    write_file("dossier/a.txt", "abc")
    print(os.size("dossier/a.txt"))
    print(os.copy_file("dossier/a.txt", "dossier/b.txt"))
    for n in os.listdir("dossier") {
        print("dans dossier : " + n)
    }
    print(os.rename("dossier/b.txt", "dossier/c.txt"))
    print(os.remove("dossier/sous"))
    print(os.remove("nexiste"))
    print(os.error())
    print(os.basename("a/b/c.cl") + " " + os.dirname("a/b/c.cl") + " " + os.extension("a/b/c.cl") + " " + os.join("a", "b.cl"))
    print(os.which("sh") != "")
    print(os.env("CLAIR_TEST", "absent"))
    os.set_env("CLAIR_TEST", "present")
    print(os.shell("echo $CLAIR_TEST").out)
    let t0: float = os.clock()
    os.sleep(0.05)
    print(os.clock() - t0 >= 0.05)
    let p: Process = os.start(["sh", "-c", "for i in 1 2 3; do echo $i; sleep 0.1; done"])
    let lu: string = ""
    while p.running() {
        lu = lu + p.read()
        os.sleep(0.02)
    }
    lu = lu + p.read()
    print(lu)
    print(p.wait())
    let q: Process = os.start(["sleep", "10"])
    print(q.wait_for(0.1))
    print(q.stop())
    print(q.wait())
    print(os.remove("dossier/a.txt") and os.remove("dossier/c.txt") and os.remove("dossier"))
    print(os.exists("dossier"))
    os.exit(4)
    return 0
}
