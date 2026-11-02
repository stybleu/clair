// V0.66: the module gui (import gui), written in Clair and built into the compiler.
// A window to draw in, in real time, like pygame: shapes, text, images, the keyboard, the mouse
// (or a finger). The work is done in C (clair_gui_rt.c) through the __gui_... functions, which only
// this file can call. SDL3 is loaded when the window opens: a program that never opens one does
// not need it. In Termux: pkg install x11-repo && pkg install sdl3 termux-x11-nightly, and the app
// Termux:X11, where the window appears.
//
//     let w: Window = gui.open("Balle", 800, 600)
//     while w.running() {
//         w.clear(gui.color("noir"))
//         w.circle(w.mouse_x(), w.mouse_y(), 20.0, gui.color("jaune"))
//         w.present()
//     }

// A color: red, green, blue (0 to 255), and alpha (255: opaque, 0: invisible).
Class Color {
    r: int
    g: int
    b: int
    a: int
}

// What happened: kind is "quit", "key_down", "key_up", "text", "mouse_down", "mouse_up",
// "mouse_move", "wheel" or "resize".
Class Event {
    kind: string
    key: string
    text: string
    x: float
    y: float
    button: int
}

Class Image {
    id: int
    width: int
    height: int
}

Class Window {
    id: int

    // reads what happened since the last frame; false once the window is closed
    def running() -> bool {
        return __gui_frame()
    }

    // what happened since the last frame, in order:
    //     for i in range(w.event_count()) { let e: Event = w.event(i) ... }
    def event_count() -> int {
        return __gui_event_count()
    }

    def event(i: int) -> Event {
        return Event(__gui_event_text(i, 0), __gui_event_text(i, 1), __gui_event_text(i, 2), __gui_event_num(i, 0), __gui_event_num(i, 1), int(__gui_event_num(i, 2)))
    }

    // a key held down: "left", "a", "space", "return", "escape"... or gauche, droite, haut, bas, espace, entree, echap
    def key_down(name: string) -> bool {
        return __gui_key(name, 0)
    }

    // pressed at this frame (once per press)
    def key_pressed(name: string) -> bool {
        return __gui_key(name, 1)
    }

    def key_released(name: string) -> bool {
        return __gui_key(name, 2)
    }

    def mouse_x() -> float {
        return __gui_mouse(0)
    }

    def mouse_y() -> float {
        return __gui_mouse(1)
    }

    // the turns of the wheel at this frame (up: more than 0)
    def wheel() -> float {
        return __gui_mouse(2)
    }

    // button 1: left (or a finger), 2: middle, 3: right
    def mouse_down(button: int) -> bool {
        return __gui_mouse_button(button, 0)
    }

    def mouse_pressed(button: int) -> bool {
        return __gui_mouse_button(button, 1)
    }

    // the left button (or a finger) let go at this frame
    def clicked() -> bool {
        return __gui_mouse_button(1, 2)
    }

    def width() -> int {
        return __gui_size(0)
    }

    def height() -> int {
        return __gui_size(1)
    }

    def clear(c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_clear()
    }

    // a filled rectangle
    def rect(x: float, y: float, w: float, h: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_rect(x, y, w, h, true)
    }

    // the outline of a rectangle
    def frame(x: float, y: float, w: float, h: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_rect(x, y, w, h, false)
    }

    def line(x1: float, y1: float, x2: float, y2: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_line(x1, y1, x2, y2)
    }

    // a line through the points (x[0], y[0]), (x[1], y[1])... : a curve, a trail
    def lines(xs: list[float], ys: list[float], c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_polygon(xs, ys, 2)
    }

    def point(x: float, y: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_point(x, y)
    }

    // a filled disc
    def circle(x: float, y: float, radius: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_circle(x, y, radius, true)
    }

    // the outline of a circle
    def ring(x: float, y: float, radius: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_circle(x, y, radius, false)
    }

    def triangle(x1: float, y1: float, x2: float, y2: float, x3: float, y3: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_triangle(x1, y1, x2, y2, x3, y3)
    }

    // a filled convex polygon: its corners, in order
    def polygon(xs: list[float], ys: list[float], c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_polygon(xs, ys, 1)
    }

    // the outline of a polygon
    def outline(xs: list[float], ys: list[float], c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_polygon(xs, ys, 0)
    }

    // text (accents included); size: the height of a letter in pixels (8, 16, 24...)
    def text(s: string, x: float, y: float, size: float, c: Color) -> int {
        __gui_set_color(c.r, c.g, c.b, c.a)
        return __gui_text(s, x, y, size)
    }

    def text_width(s: string, size: float) -> float {
        return __gui_text_width(s, size)
    }

    // an image at x, y, at its size
    def draw(img: Image, x: float, y: float) -> int {
        return __gui_image_draw(img.id, x, y, float(img.width), float(img.height), 0.0)
    }

    def draw_scaled(img: Image, x: float, y: float, w: float, h: float) -> int {
        return __gui_image_draw(img.id, x, y, w, h, 0.0)
    }

    // turned by angle degrees around its center
    def draw_rotated(img: Image, x: float, y: float, w: float, h: float, angle: float) -> int {
        return __gui_image_draw(img.id, x, y, w, h, angle)
    }

    // a button: drawn, and true at the frame where it is clicked (or touched)
    def button(label: string, x: float, y: float, w: float, h: float) -> bool {
        return __gui_button(label, x, y, w, h)
    }

    // shows what was drawn, then waits to keep 60 frames per second
    def present() -> int {
        return __gui_present()
    }

    // the frames per second wanted (0: as fast as possible)
    def set_fps(n: int) -> int {
        return __gui_set_fps(n)
    }

    // the frames per second measured
    def fps() -> float {
        return __gui_time(2)
    }

    // the seconds the last frame took: move by speed * w.dt() to go at the same speed everywhere
    def dt() -> float {
        return __gui_time(0)
    }

    // the seconds since the window opened
    def time() -> float {
        return __gui_time(1)
    }

    def title(t: string) -> int {
        return __gui_title(t)
    }

    // what is drawn so far, in a PNG file (or BMP)
    def screenshot(path: string) -> bool {
        return __gui_screenshot(path)
    }

    // the color of a pixel drawn so far
    def pixel(x: int, y: int) -> Color {
        return Color(__gui_pixel(x, y, 0), __gui_pixel(x, y, 1), __gui_pixel(x, y, 2), __gui_pixel(x, y, 3))
    }

    // the window closes: running() gives false
    def close() -> int {
        return __gui_close()
    }
}

// opens the window; width and height 0: the whole screen (in Termux, the app Termux:X11 shows it)
def open(title: string, width: int, height: int) -> Window {
    return Window(__gui_open(title, width, height))
}

// true when SDL3 is there: a window can open
def available() -> bool {
    return __gui_available()
}

def rgb(r: int, g: int, b: int) -> Color {
    return Color(r, g, b, 255)
}

def rgba(r: int, g: int, b: int, a: int) -> Color {
    return Color(r, g, b, a)
}

// "red", "rouge", "noir", "white", "ciel"...
def color(name: string) -> Color {
    return Color(__gui_color_name(name, 0), __gui_color_name(name, 1), __gui_color_name(name, 2), 255)
}

// "#ffcc00" or "ffcc00"
def hex(text: string) -> Color {
    return Color(__gui_hex(text, 0), __gui_hex(text, 1), __gui_hex(text, 2), 255)
}

// a PNG or BMP image (after gui.open)
def image(path: string) -> Image {
    let id: int = __gui_image_load(path)
    return Image(id, __gui_image_size(id, 0), __gui_image_size(id, 1))
}
