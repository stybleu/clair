// V0.70.1 : la fluidite de la fenetre de nbody sur cet appareil (sans ecran, pour le journal).
// A lancer avec CLAIR_NBODY_THREAD=1 CLAIR_GUI_FRAMES=120 SDL_VIDEO_DRIVER=offscreen.
import nbody
import os

def main() -> int {
    for n in [500, 4000] {
        let s: Simulation = nbody.simulation(1.0)
        s.add_plummer(n / 2, 1.0, 0.4, 1, -2.0, 0.0, 0.0, 0.3, 0.08, 0.0, "#4fc3f7")
        s.add_plummer(n / 2, 1.0, 0.4, 2, 2.0, 0.0, 0.0, -0.3, -0.08, 0.0, "#ff7043")
        s.gravity = "tree"
        s.soft = 0.02
        s.dt = 0.02
        s.speed = 1.0
        let t0: float = os.clock()
        s.show()
        let t1: float = os.clock()
        print("fenetre, " + str(n) + " etoiles : " + format(120.0 / (t1 - t0), ".0f") + " images par seconde (60 au plus), " + format(s.t / 0.02 / (t1 - t0), ".0f") + " pas de calcul par seconde")
    }
    return 0
}
