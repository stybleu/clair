// V0.70 (V0.71 : MERCURIUS) : la vitesse de nbody sur cet appareil (pour le journal : tools/journal.sh). Quelques secondes.
import nbody
import os

def amas(n: int, coeurs: int) -> Simulation {
    let s: Simulation = nbody.simulation(1.0)
    s.add_plummer(n, 1.0, 1.0, 1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, "")
    s.soft = 0.01
    s.gravity = "tree"
    s.dt = 0.001
    s.threads = coeurs
    return s
}

def main() -> int {
    for n in [1000, 10000] {
        let s: Simulation = amas(n, 0)
        s.run(0.001)
        let t0: float = os.clock()
        s.run(0.004)
        let t1: float = os.clock()
        s.threads = 1
        s.run(0.002)
        let t2: float = os.clock()
        print("arbre, " + str(n) + " etoiles : " + format((t1 - t0) / 4.0 * 1000.0, ".1f") + " ms par pas (tous les coeurs), " + format((t2 - t1) / 2.0 * 1000.0, ".1f") + " ms (un coeur)")
    }
    let d: Simulation = amas(2000, 0)
    d.gravity = "direct"
    d.run(0.001)
    let t3: float = os.clock()
    d.run(0.004)
    let t4: float = os.clock()
    print("calcul direct, 2000 etoiles : " + format((t4 - t3) / 4.0 * 1000.0, ".1f") + " ms par pas")
    let i: Simulation = nbody.solar_system()
    let t5: float = os.clock()
    i.run(100.0)
    let t6: float = os.clock()
    print("IAS15, systeme solaire, 100 ans : " + format(t6 - t5, ".2f") + " s")
    let w: Simulation = nbody.solar_system()
    w.integrator = "whfast"
    w.dt = 0.01
    let t7: float = os.clock()
    w.run(1000.0)
    let t8: float = os.clock()
    print("WHFast, systeme solaire, 1 000 ans : " + format(t8 - t7, ".2f") + " s")
    // V0.71 : MERCURIUS, sans rencontre (le systeme solaire) puis avec (trois planetes serrees)
    let mc: Simulation = nbody.solar_system()
    mc.integrator = "mercurius"
    mc.dt = 0.005
    let t9: float = os.clock()
    mc.run(1000.0)
    let t10: float = os.clock()
    print("MERCURIUS, systeme solaire, 1 000 ans : " + format(t10 - t9, ".2f") + " s")
    let p: Simulation = nbody.simulation(1.0)
    p.integrator = "mercurius"
    p.add("etoile", 1.0, 0.0, 0.0, 0.0, 0.0)
    for k in range(3) {
        let a: float = 1.0 + 0.1 * float(k)
        p.add("p", 0.002, a, 0.0, 0.0, sqrt(1.0 / a))
    }
    p.center()
    p.dt = 0.0012 * 2.0 * 3.141592653589793
    let t11: float = os.clock()
    p.run(1326.0 * p.dt)
    let t12: float = os.clock()
    print("MERCURIUS, trois planetes serrees, 1 326 pas : " + format(t12 - t11, ".2f") + " s, " + str(p.steps_done(3)) + " pas avec rencontre, " + str(p.steps_done(4)) + " pas d'IAS15")
    return 0
}
