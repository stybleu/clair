// V0.61 : deux courbes, une legende, des noms d'axes.
// Dans Clair Studio, la figure s'affiche dans la console ; sinon plot_1.svg (a ouvrir dans un navigateur).
import plot
import math

def main() -> int {
    let xs: list[float] = []
    let sinus: list[float] = []
    let amorti: list[float] = []
    for i in range(0, 301) {
        let x: float = float(i) / 30.0
        xs.append(x)
        sinus.append(math.sin(x))
        amorti.append(math.exp(-x / 4.0) * math.cos(2.0 * x))
    }
    plot.title("Deux oscillations")
    plot.xlabel("temps (s)")
    plot.ylabel("position")
    plot.line(xs, sinus)
    plot.name("sin(t)")
    plot.line(xs, amorti)
    plot.name("oscillation amortie")
    plot.show()
    return 0
}
