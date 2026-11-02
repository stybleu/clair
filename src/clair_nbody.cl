// V0.66: the module nbody (import nbody), written in Clair and built into the compiler.
// N bodies pulled by Newton's gravity: planets, stars, a galaxy. The forces and the steps are
// computed in C (clair_nbody_rt.c, through the __nbody_... functions, which only this file can
// call), and s.show() shows the bodies moving in real time, in a window of the module gui.
//
//     let s: Simulation = nbody.simulation(1.0)                // G = 1, in your own units
//     let soleil: int = s.add("Soleil", 1000.0, 0.0, 0.0, 0.0, 0.0)
//     s.orbit("Terre", 1.0, soleil, 100.0, 0.0)               // a round orbit, 100 from the Sun
//     s.show()
//
// The bodies are in the lists of the simulation: s.x[i], s.y[i], s.vx[i], s.m[i]... (a body
// absorbed by a collision has a mass of 0 and a radius of -1). In SI units: nbody.simulation(nbody.G()).

import gui

Class Simulation {
    name: list[string]
    m: list[float]
    x: list[float]
    y: list[float]
    z: list[float]
    vx: list[float]
    vy: list[float]
    vz: list[float]
    r: list[float]
    colors: list[int]
    G: float
    t: float
    dt: float
    soft: float
    order: int
    merge: bool
    speed: float
    trail: int
    time_unit: string
    time_scale: float
    length_unit: string
    length_scale: float

    // the C part works on the lists of this simulation
    def bind() -> int {
        return __nbody_use(self.x, self.y, self.z, self.vx, self.vy, self.vz, self.m, self.r)
    }

    // a body in the plane: its name, its mass, where it is, how fast it goes; gives its number
    def add(name: string, m: float, x: float, y: float, vx: float, vy: float) -> int {
        return self.add3(name, m, x, y, 0.0, vx, vy, 0.0)
    }

    // a body in space (x, y, z)
    def add3(name: string, m: float, x: float, y: float, z: float, vx: float, vy: float, vz: float) -> int {
        if m < 0.0 {
            __nbody_fail("a mass is not negative (" + name + ")")
        }
        self.name.append(name)
        self.m.append(m)
        self.x.append(x)
        self.y.append(y)
        self.z.append(z)
        self.vx.append(vx)
        self.vy.append(vy)
        self.vz.append(vz)
        self.r.append(0.0)
        self.colors.append(-1)
        return len(self.m) - 1
    }

    // a body on a round orbit around the body around, at this distance, at this angle (degrees),
    // turning the other way from a clock
    def orbit(name: string, m: float, around: int, distance: float, angle: float) -> int {
        if around < 0 or around >= len(self.m) or distance <= 0.0 {
            __nbody_fail("orbit() needs a body to turn around and a distance more than 0 (" + name + ")")
        }
        let a: float = angle * 3.141592653589793 / 180.0
        let v: float = sqrt(self.G * (self.m[around] + m) / distance)
        return self.add3(name, m, self.x[around] + distance * cos(a), self.y[around] + distance * sin(a), self.z[around], self.vx[around] - v * sin(a), self.vy[around] + v * cos(a), self.vz[around])
    }

    // the size of a body (for collisions; in the window, a body is never drawn smaller than a dot)
    def radius(i: int, r: float) -> int {
        self.r[i] = r
        return 0
    }

    // the color of a body in the window: "jaune", "blue", "#ffcc00"...
    def color(i: int, name: string) -> int {
        let c: Color = gui.rgb(0, 0, 0)
        if len(name) > 0 and name[0:1] == "#" {
            c = gui.hex(name)
        } else {
            c = gui.color(name)
        }
        self.colors[i] = c.r * 65536 + c.g * 256 + c.b
        return 0
    }

    // the number of a body by its name (-1: none)
    def find(name: string) -> int {
        for i in range(len(self.name)) {
            if self.name[i] == name {
                return i
            }
        }
        return -1
    }

    // the time step (when dt is 0: a five hundredth of the time a pair of bodies takes to fall together)
    def time_step() -> float {
        if self.dt <= 0.0 {
            self.bind()
            let tau: float = __nbody_tau(self.G, self.soft)
            if tau <= 0.0 {
                return 0.001
            }
            self.dt = tau / 500.0
        }
        return self.dt
    }

    def flags() -> int {
        let f: int = 0
        if self.merge {
            f += 1
        }
        if self.order == 4 {
            f += 2
        }
        return f
    }

    // one step of time: gives the number of collisions
    def step() -> int {
        let h: float = self.time_step()
        self.bind()
        let n: int = __nbody_step(h, 1, self.G, self.soft, self.flags())
        self.t += h
        return n
    }

    // goes on for this time (in steps of dt): gives the number of collisions
    def run(duration: float) -> int {
        let h: float = self.time_step()
        let steps: int = int(duration / h + 0.5)
        if steps <= 0 {
            return 0
        }
        self.bind()
        let n: int = __nbody_step(h, steps, self.G, self.soft, self.flags())
        self.t += h * float(steps)
        return n
    }

    def kinetic() -> float {
        self.bind()
        return __nbody_energy(self.G, self.soft, 0)
    }

    def potential() -> float {
        self.bind()
        return __nbody_energy(self.G, self.soft, 1)
    }

    // kinetic + potential: the same all along, when the steps are small enough
    def energy() -> float {
        self.bind()
        return __nbody_energy(self.G, self.soft, 0) + __nbody_energy(self.G, self.soft, 1)
    }

    // the momentum (mass x speed) of everything along an axis: 0 x, 1 y, 2 z
    def momentum(axis: int) -> float {
        self.bind()
        return __nbody_sum(axis)
    }

    // the angular momentum of everything (its size)
    def angular_momentum() -> float {
        self.bind()
        let lx: float = __nbody_sum(3)
        let ly: float = __nbody_sum(4)
        let lz: float = __nbody_sum(5)
        return sqrt(lx * lx + ly * ly + lz * lz)
    }

    // the center of mass along an axis: 0 x, 1 y, 2 z
    def center_of_mass(axis: int) -> float {
        self.bind()
        return __nbody_sum(6 + axis)
    }

    // moves everything so that the center of mass stays at 0, without moving
    def center() -> int {
        self.bind()
        return __nbody_center()
    }

    // the bodies still there (not absorbed by a collision)
    def count() -> int {
        let n: int = 0
        for i in range(len(self.r)) {
            if self.r[i] >= 0.0 {
                n += 1
            }
        }
        return n
    }

    def distance(i: int, j: int) -> float {
        let dx: float = self.x[i] - self.x[j]
        let dy: float = self.y[i] - self.y[j]
        let dz: float = self.z[i] - self.z[j]
        return sqrt(dx * dx + dy * dy + dz * dz)
    }

    def velocity(i: int) -> float {
        return sqrt(self.vx[i] * self.vx[i] + self.vy[i] * self.vy[i] + self.vz[i] * self.vz[i])
    }

    // the bodies moving in real time, in a window (the whole screen), until "Quitter":
    // drag to move, the wheel or + and - to zoom, touch a body to follow it.
    // s.speed: the time of the simulation in one second (0: one quick orbit in 4 seconds)
    def show() -> int {
        let h: float = self.time_step()
        self.bind()
        __nbody_look(self.name, self.colors)
        __nbody_param(0, h)
        __nbody_param(1, self.speed)
        __nbody_param(2, float(self.trail))
        __nbody_param(3, self.time_scale)
        __nbody_param(4, self.length_scale)
        __nbody_param(5, self.G)
        __nbody_param(6, self.soft)
        __nbody_param(7, float(self.flags()))
        __nbody_param(8, self.t)
        __nbody_unit(0, self.time_unit)
        __nbody_unit(1, self.length_unit)
        self.t = __nbody_show()
        return 0
    }
}

// the constant of gravitation in SI units (m^3 / (kg s^2)), CODATA 2022
def G() -> float {
    return 6.6743e-11
}

// an empty simulation; g: the constant of gravitation in the units chosen (1.0, or nbody.G() in SI)
def simulation(g: float) -> Simulation {
    return Simulation([], [], [], [], [], [], [], [], [], [], g, 0.0, 0.0, 0.0, 2, false, 0.0, 400, "", 1.0, "", 1.0)
}
