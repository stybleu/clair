// V0.68 : le systeme solaire dans la fenetre de nbody, avec IAS15 (sans ecran : tests/run_tests.sh)
import nbody

def main() -> int {
    let s: Simulation = nbody.solar_system()
    s.speed = 2.0
    let e0: float = s.energy()
    s.show()
    print("t = " + format(s.t, ".3f") + " an")
    print("energie gardee : " + str(abs((s.energy() - e0) / e0) < 1e-14))
    print("pas choisi par IAS15 : " + str(s.dt > 0.001 and s.dt < 0.01))
    return 0
}
