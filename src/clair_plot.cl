// V0.61: the module plot (import plot), written in Clair and built into the compiler.
// Graphs and drawings, a little like matplotlib: plot.line(xs, ys), then plot.show().
// In Clair Studio the figure appears in the console; elsewhere plot.show() writes
// plot_1.svg, plot_2.svg... (an SVG: open it in a browser).
// The work is done in C (clair_plot_rt.c) through the __plot_... functions, which only this file can call.

// ---- the figure ----

// its size in pixels (640 x 400 at first)
def size(width: int, height: int) -> int {
    return __plot_size(width, height)
}

def title(text: string) -> int {
    return __plot_set(0, text)
}

// the name of the horizontal axis
def xlabel(text: string) -> int {
    return __plot_set(1, text)
}

// the name of the vertical axis
def ylabel(text: string) -> int {
    return __plot_set(2, text)
}

// what the horizontal axis shows (otherwise: all the data)
def xrange(low: float, high: float) -> int {
    return __plot_range(0, low, high)
}

// what the vertical axis shows (otherwise: all the data)
def yrange(low: float, high: float) -> int {
    return __plot_range(1, low, high)
}

// the values that the colors of plot.heatmap go from and to (otherwise: the smallest and largest of the field);
// the same scale for every frame of an animation
def crange(low: float, high: float) -> int {
    return __plot_range(2, low, high)
}

// the same scale on both axes: a circle stays round, a square square
def equal() -> int {
    return __plot_flag(0, 1)
}

// no axes, no grid: a drawing
def hide_axes() -> int {
    return __plot_flag(1, 1)
}

// the colors of plot.heatmap: "viridis" (at first), "chaleur" or "bleu-rouge"
def colormap(name: string) -> int {
    return __plot_set(6, name)
}

// ---- data ----

// a curve through the points (xs[i], ys[i])
def line(xs: list[float], ys: list[float]) -> int {
    return __plot_series(0, xs, ys)
}

// a point at each (xs[i], ys[i])
def points(xs: list[float], ys: list[float]) -> int {
    return __plot_series(1, xs, ys)
}

// a closed shape through the points (plot.fill("jaune") fills it)
def polygon(xs: list[float], ys: list[float]) -> int {
    return __plot_series(2, xs, ys)
}

// a bar for each value, its name below
def bars(names: list[string], values: list[float]) -> int {
    return __plot_bars(names, values)
}

// how many values fall in each of the bins between the smallest and the largest
def histogram(values: list[float], bins: int) -> int {
    return __plot_histogram(values, bins)
}

// a straight line from (x1, y1) to (x2, y2)
def segment(x1: float, y1: float, x2: float, y2: float) -> int {
    return __plot_shape(0, x1, y1, x2, y2)
}

// an arrow from (x, y), going dx to the right and dy up
def arrow(x: float, y: float, dx: float, dy: float) -> int {
    return __plot_shape(1, x, y, x + dx, y + dy)
}

def circle(x: float, y: float, radius: float) -> int {
    return __plot_shape(2, x, y, radius, 0.0)
}

// a text centered on (x, y)
def text(x: float, y: float, t: string) -> int {
    return __plot_text(x, y, t)
}

// ---- a field on a grid of nx columns and ny rows: values[j * nx + i], row 0 at the bottom ----

// where the next grids lie (otherwise: from 0 to nx and from 0 to ny)
def extent(x0: float, x1: float, y0: float, y1: float) -> int {
    return __plot_extent(x0, x1, y0, y1)
}

// the field in colors, with its scale on the right
def heatmap(values: list[float], nx: int, ny: int) -> int {
    return __plot_field(0, values, nx, ny, 0)
}

// the lines where the field has the same value (isobars...), about levels of them
def contour(values: list[float], nx: int, ny: int, levels: int) -> int {
    return __plot_field(1, values, nx, ny, levels)
}

// an arrow for (us, vs) on the grid (the wind...), the longest as long as a cell
def arrows(us: list[float], vs: list[float], nx: int, ny: int) -> int {
    return __plot_arrows(us, vs, nx, ny)
}

// ---- the last thing drawn ----

// its name in the legend
def name(text: string) -> int {
    return __plot_set(3, text)
}

// its color: "rouge", "bleu", "vert"... or "#1d4ed8"
def color(c: string) -> int {
    return __plot_set(4, c)
}

// the color inside a polygon or a circle
def fill(c: string) -> int {
    return __plot_set(5, c)
}

// the width of its line, in pixels
def width(w: float) -> int {
    return __plot_width(w)
}

// ---- showing ----

// shows the figure (the console of Clair Studio, or plot_1.svg...), then starts a new one
def show() -> int {
    return __plot_out(0, "", 0.0)
}

// writes the figure into an SVG file (and keeps it)
def save(path: string) -> int {
    return __plot_out(1, path, 0.0)
}

// keeps the figure as the next frame of an animation, then clears its data (not its title, axes, size)
def frame() -> int {
    return __plot_out(2, "", 0.0)
}

// shows the frames as an animation, fps frames per second
def animate(fps: float) -> int {
    return __plot_out(3, "", fps)
}

// shows an image of the folder in the console of Clair Studio (png, jpg, svg...)
def image(path: string) -> int {
    return __plot_image(path)
}
