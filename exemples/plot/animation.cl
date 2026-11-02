// V0.61 : une onde qui avance : une image par pas de temps, puis plot.animate.
// Dans Clair Studio : lecture, pause et un curseur sous la figure.
import plot
import math

def main() -> int {
    let xs: list[float] = []
    for i in range(0, 201) {
        xs.append(float(i) / 20.0)
    }
    plot.title("Une onde qui avance")
    plot.yrange(-1.5, 1.5)
    for t in range(0, 40) {
        let ys: list[float] = []
        for i in range(0, 201) {
            ys.append(math.sin(2.0 * xs[i] - float(t) * 0.25) * math.exp(-xs[i] / 8.0))
        }
        plot.line(xs, ys)
        plot.frame()
    }
    plot.animate(12.0)
    return 0
}
