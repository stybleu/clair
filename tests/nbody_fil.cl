// V0.70.1 : la fenetre de nbody avec le calcul dans un fil a part (CLAIR_NBODY_THREAD=1, sans ecran :
// tests/run_tests.sh). Le temps avance au rythme de l'horloge : on verifie qu'il avance, pas sa valeur.
import nbody

def main() -> int {
    let s: Simulation = nbody.solar_system()
    s.speed = 2.0
    let e0: float = s.energy()
    s.show()
    print("IAS15 : le temps avance " + str(s.t > 0.1) + ", energie gardee " + str(abs((s.energy() - e0) / e0) < 1e-13) + ", pas normal " + str(s.dt > 0.0005 and s.dt < 0.05))
    let w: Simulation = nbody.solar_system()
    w.integrator = "whfast"
    w.dt = 0.005
    w.speed = 2.0
    let w0: float = w.energy()
    w.show()
    print("WHFast : le temps avance " + str(w.t > 0.1) + ", energie gardee " + str(abs((w.energy() - w0) / w0) < 1e-9))
    let a: Simulation = nbody.simulation(1.0)
    a.add_plummer(2000, 1.0, 1.0, 4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "")
    a.gravity = "tree"
    a.soft = 0.02
    a.dt = 0.01
    a.merge = true
    for i in range(len(a.m)) {
        a.radius(i, 0.001)
    }
    let p0: float = a.momentum(0)
    a.show()
    print("amas avec l'arbre : le temps avance " + str(a.t > 0.0) + ", un multiple du pas " + str(abs(a.t / 0.01 - float(int(a.t / 0.01 + 0.5))) < 1e-6) + ", quantite de mouvement " + str(abs(a.momentum(0) - p0) < 1e-4))
    return 0
}
