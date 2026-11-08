// V0.68 : le systeme solaire dans la fenetre de nbody, avec IAS15, puis WHFast (V0.69), puis un amas avec l'arbre (V0.70) (sans ecran : tests/run_tests.sh)
import nbody

def main() -> int {
    let s: Simulation = nbody.solar_system()
    s.speed = 2.0
    let e0: float = s.energy()
    s.show()
    print("t = " + format(s.t, ".3f") + " an")
    print("energie gardee : " + str(abs((s.energy() - e0) / e0) < 1e-14))
    print("pas choisi par IAS15 : " + str(s.dt > 0.001 and s.dt < 0.01))
    // V0.69 : la meme chose avec WHFast
    let w: Simulation = nbody.solar_system()
    w.integrator = "whfast"
    w.dt = 0.005
    w.speed = 2.0
    let w0: float = w.energy()
    w.show()
    print("WHFast : t = " + format(w.t, ".3f") + " an, energie gardee : " + str(abs((w.energy() - w0) / w0) < 1e-9))
    // V0.70 : un amas de 2 000 etoiles avec l'arbre
    let a: Simulation = nbody.simulation(1.0)
    a.add_plummer(2000, 1.0, 1.0, 4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "blanc")
    a.gravity = "tree"
    a.soft = 0.02
    a.dt = 0.01
    a.show()
    print("amas avec l'arbre : t = " + format(a.t, ".2f") + ", " + str(len(a.m)) + " etoiles")
    return 0
}
