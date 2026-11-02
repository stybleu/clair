// V0.66 : la fenetre de nbody (s.show), sans ecran : run_tests.sh touche "Pause", "Lecture" puis "Quitter"
import nbody
import gui

def main() -> int {
    if not gui.available() {
        print("sans SDL3")
        return 0
    }
    let w: Window = gui.open("nbody", 800, 600)
    let s: Simulation = nbody.simulation(1.0)
    let a: int = s.add("Etoile", 1.0, 0.0, 0.0, 0.0, 0.0)
    s.orbit("Planete", 0.001, a, 1.0, 0.0)
    s.orbit("Lune", 0.00001, 1, 0.05, 0.0)
    s.center()
    s.speed = 1.0
    s.dt = 0.001
    let e0: float = s.energy()
    s.show()
    print("fenetre fermee par Quitter, t = " + format(s.t, ".3f"))
    print("energie gardee : " + str(abs((s.energy() - e0) / e0) < 0.00001))
    print("la fenetre du programme est encore la : " + str(w.running()))
    s.show()
    print("deuxieme fois : t = " + format(s.t, ".3f"))
    w.close()
    return 0
}
