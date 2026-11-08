// V0.61 : une depression : la pression en couleurs, les isobares, et le vent qui tourne autour
// (dans l'hemisphere nord, le vent tourne dans le sens inverse des aiguilles d'une montre).
import plot
import math

def main() -> int {
    let nx: int = 80
    let ny: int = 50
    let pression: list[float] = []
    let u: list[float] = []
    let v: list[float] = []
    for j in range(0, ny) {
        for i in range(0, nx) {
            let x: float = -8.0 + 16.0 * (float(i) + 0.5) / float(nx)
            let y: float = -5.0 + 10.0 * (float(j) + 0.5) / float(ny)
            let creux: float = math.exp(-(x * x + y * y) / 8.0)
            pression.append(1015.0 - 30.0 * creux)
            // le vent suit les isobares : perpendiculaire a la pente de la pression
            u.append(-y * creux)
            v.append(x * creux)
        }
    }
    plot.size(640, 420)
    plot.title("Depression : pression (hPa) et vent")
    plot.extent(-8.0, 8.0, -5.0, 5.0)
    plot.equal()
    plot.colormap("bleu-rouge")
    plot.heatmap(pression, nx, ny)
    plot.contour(pression, nx, ny, 10)
    plot.arrows(u, v, nx, ny)
    plot.show()
    return 0
}
