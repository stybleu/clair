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
//
// V0.70: s.gravity = "tree": the tree of Barnes and Hut (N log N), on every core (s.threads: 0 = all);
// s.add_plummer(...): a cluster of stars in equilibrium.
// V0.69: s.integrator = "whfast": Wisdom-Holman (as in REBOUND), for planets over millions of years;
// s.save(path) and nbody.load(path).
// V0.68: s.integrator = "ias15": the integrator of order 15 with its own time step (as in REBOUND),
// precise to the last digits even when bodies pass close; s.add_orbit(...) and s.elements(i, j):
// the orbital elements; nbody.solar_system(): the Sun and the eight planets (JPL), in AU and years.

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
    integrator: string
    epsilon: float
    gravity: string
    theta: float
    threads: int

    // the C part works on the lists of this simulation (V0.70: and with its gravity: the tree or not)
    def bind() -> int {
        if self.gravity == "tree" {
            if self.theta <= 0.0 or self.theta >= 1.5 {
                __nbody_fail("s.theta: between 0 and 1.5 (0.5: the forces to about 1e-3)")
            }
            __nbody_param(10, self.theta)
        } else {
            if self.gravity != "direct" {
                __nbody_fail("s.gravity: \"direct\" or \"tree\", not \"" + self.gravity + "\"")
            }
            __nbody_param(10, 0.0)
        }
        __nbody_param(11, float(self.threads))
        __nbody_param(12, self.theta)
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

    // the time step (when dt is 0: a five hundredth of the time a pair of bodies takes to fall together;
    // WHFast: a fortieth of the shortest orbit)
    def time_step() -> float {
        if self.dt <= 0.0 and self.gravity == "tree" {
            // V0.70: many bodies: a two hundredth of the time to cross the cluster (sqrt(R^3 / G M))
            self.bind()
            let M: float = __nbody_sum(12)
            let cx: float = __nbody_sum(6)
            let cy: float = __nbody_sum(7)
            let cz: float = __nbody_sum(8)
            let s2: float = 0.0
            for i in range(len(self.m)) {
                if self.r[i] >= 0.0 {
                    s2 += self.m[i] * ((self.x[i] - cx) * (self.x[i] - cx) + (self.y[i] - cy) * (self.y[i] - cy) + (self.z[i] - cz) * (self.z[i] - cz))
                }
            }
            if M <= 0.0 or s2 <= 0.0 or self.G <= 0.0 {
                return 0.001
            }
            let R: float = sqrt(s2 / M)
            self.dt = sqrt(R * R * R / (self.G * M)) / 200.0
        }
        if self.dt <= 0.0 {
            self.bind()
            let tau: float = __nbody_tau(self.G, self.soft)
            if tau <= 0.0 {
                return 0.001
            }
            if self.integrator == "whfast" {
                self.dt = tau * 2.0 * 3.141592653589793 / 40.0
            } else {
                self.dt = tau / 500.0
            }
        }
        return self.dt
    }

    def flags() -> int {
        let f: int = 0
        if self.merge {
            f += 1
        }
        if self.gravity == "tree" and self.integrator != "leapfrog" and self.integrator != "yoshida" {
            __nbody_fail("s.gravity = \"tree\" goes with s.integrator = \"leapfrog\" or \"yoshida\" (the forces of the tree are approximate)")
        }
        if self.integrator == "ias15" {
            return f + 4
        }
        if self.integrator == "whfast" {
            if self.merge {
                __nbody_fail("s.merge: WHFast does not merge bodies (s.integrator = \"ias15\" does)")
            }
            return f + 8
        }
        if self.integrator == "yoshida" or (self.integrator == "leapfrog" and self.order == 4) {
            return f + 2
        }
        if self.integrator != "leapfrog" {
            __nbody_fail("s.integrator: \"leapfrog\", \"yoshida\", \"ias15\" or \"whfast\", not \"" + self.integrator + "\"")
        }
        return f
    }

    def ias15() -> bool {
        let f: int = self.flags()
        return f >= 4 and f < 8
    }

    // one step of time: gives the number of collisions (IAS15: a step of the length it chooses)
    def step() -> int {
        if self.ias15() {
            self.bind()
            let k: int = __nbody_ias15(-1.0, self.G, self.soft, self.flags(), self.epsilon, self.dt)
            self.dt = __nbody_out(0)
            self.t += __nbody_out(1)
            return k
        }
        let h: float = self.time_step()
        self.bind()
        let n: int = __nbody_step(h, 1, self.G, self.soft, self.flags())
        self.t += h
        return n
    }

    // goes on for this time (in steps of dt; IAS15: exactly this time): gives the number of collisions
    def run(duration: float) -> int {
        if self.ias15() {
            if duration <= 0.0 {
                return 0
            }
            self.bind()
            let k: int = __nbody_ias15(duration, self.G, self.soft, self.flags(), self.epsilon, self.dt)
            self.dt = __nbody_out(0)
            self.t += duration
            return k
        }
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
        __nbody_param(9, self.epsilon)
        __nbody_unit(0, self.time_unit)
        __nbody_unit(1, self.length_unit)
        self.t = __nbody_show()
        if self.ias15() {
            self.dt = __nbody_out(0)
        }
        return 0
    }

    // V0.69: the whole simulation in a text file (nbody.load(path) gives it back, to the last digit)
    def save(path: string) -> int {
        self.bind()
        __nbody_look(self.name, self.colors)
        __nbody_param(0, self.dt)
        __nbody_param(1, self.speed)
        __nbody_param(2, float(self.trail))
        __nbody_param(3, self.time_scale)
        __nbody_param(4, self.length_scale)
        __nbody_param(5, self.G)
        __nbody_param(6, self.soft)
        __nbody_param(7, float(self.flags()))
        __nbody_param(8, self.t)
        __nbody_param(9, self.epsilon)
        return __nbody_save(path, self.integrator, self.time_unit, self.length_unit, self.gravity)
    }

    // V0.70: a cluster of n stars in equilibrium (a Plummer sphere) of this mass and Plummer radius, centered
    // at x, y, z and moving at vx, vy, vz; color: "" or a color ("blanc", "#ffcc00"); seed: the same seed, the
    // same cluster.  Gives the number of its first star.
    def add_plummer(n: int, mass: float, radius: float, seed: int, x: float, y: float, z: float, vx: float, vy: float, vz: float, color: string) -> int {
        let first: int = len(self.m)
        __nbody_plummer(n, self.G, mass, radius, seed)
        for k in range(n) {
            self.add3("", mass / float(n), x + __nbody_gen(k, 0), y + __nbody_gen(k, 1), z + __nbody_gen(k, 2), vx + __nbody_gen(k, 3), vy + __nbody_gen(k, 4), vz + __nbody_gen(k, 5))
        }
        if len(color) > 0 {
            self.color(first, color)
            let c: int = self.colors[first]
            for k in range(n) {
                self.colors[first + k] = c
            }
        }
        return first
    }

    // V0.70: the virial ratio 2 K / |W| (1: in equilibrium; the potential energy W by direct sums)
    def virial() -> float {
        let w: float = self.potential()
        if w == 0.0 {
            return 0.0
        }
        return 2.0 * self.kinetic() / abs(w)
    }

    // V0.70: the radius around the center of mass that holds this fraction of the mass (0.5: half the mass)
    def lagrange_radius(fraction: float) -> float {
        self.bind()
        let cx: float = __nbody_sum(6)
        let cy: float = __nbody_sum(7)
        let cz: float = __nbody_sum(8)
        let total: float = __nbody_sum(12)
        let d: list[float] = []
        let w: list[float] = []
        for i in range(len(self.m)) {
            if self.r[i] >= 0.0 {
                d.append(sqrt((self.x[i] - cx) * (self.x[i] - cx) + (self.y[i] - cy) * (self.y[i] - cy) + (self.z[i] - cz) * (self.z[i] - cz)))
                w.append(self.m[i])
            }
        }
        return __nbody_quantile(d, w, fraction * total)
    }

    // V0.68: the IAS15 steps done (0), done again because they were too long (1)
    def steps_done(what: int) -> int {
        return int(__nbody_stat(what))
    }

    // a body on an orbit around the body around (-1: around the center of mass of all the bodies already
    // there, as REBOUND does): a the semi-major axis (< 0 for a hyperbola), e the eccentricity, and in
    // degrees: inc the inclination, node the longitude of the ascending node, peri the argument of the
    // pericenter, f the true anomaly (from the mean anomaly: nbody.true_anomaly(M, e))
    def add_orbit(name: string, m: float, around: int, a: float, e: float, inc: float, node: float, peri: float, f: float) -> int {
        let n: int = len(self.m)
        if around >= n or around < -1 {
            __nbody_fail("add_orbit(): no body " + str(around) + " to turn around (" + name + ")")
        }
        let mc: float = 0.0
        let c: list[float] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        if around >= 0 {
            mc = self.m[around]
            c = [self.x[around], self.y[around], self.z[around], self.vx[around], self.vy[around], self.vz[around]]
        } else {
            for k in range(n) {
                if self.r[k] >= 0.0 {
                    mc += self.m[k]
                    c[0] = c[0] + self.m[k] * self.x[k]
                    c[1] = c[1] + self.m[k] * self.y[k]
                    c[2] = c[2] + self.m[k] * self.z[k]
                    c[3] = c[3] + self.m[k] * self.vx[k]
                    c[4] = c[4] + self.m[k] * self.vy[k]
                    c[5] = c[5] + self.m[k] * self.vz[k]
                }
            }
            if mc <= 0.0 {
                __nbody_fail("add_orbit(): no mass to turn around (" + name + ")")
            }
            for k in range(6) {
                c[k] = c[k] / mc
            }
        }
        __nbody_orbit(self.G * (mc + m), a, e, inc, node, peri, f)
        return self.add3(name, m, c[0] + __nbody_out(0), c[1] + __nbody_out(1), c[2] + __nbody_out(2), c[3] + __nbody_out(3), c[4] + __nbody_out(4), c[5] + __nbody_out(5))
    }

    // the orbit of the body i around the body j (-1: the center of mass of the bodies before i):
    // [a, e, inclination, node, pericenter, true anomaly, mean anomaly, period] (angles in degrees)
    def elements(i: int, j: int) -> list[float] {
        self.bind()
        __nbody_elements(i, j, self.G)
        return [__nbody_out(0), __nbody_out(1), __nbody_out(2), __nbody_out(3), __nbody_out(4), __nbody_out(5), __nbody_out(6), __nbody_out(7)]
    }

    def semi_major_axis(i: int, j: int) -> float {
        self.bind()
        __nbody_elements(i, j, self.G)
        return __nbody_out(0)
    }

    def eccentricity(i: int, j: int) -> float {
        self.bind()
        __nbody_elements(i, j, self.G)
        return __nbody_out(1)
    }

    def inclination(i: int, j: int) -> float {
        self.bind()
        __nbody_elements(i, j, self.G)
        return __nbody_out(2)
    }

    // the time of one turn (0 when it does not come back: e >= 1)
    def period(i: int, j: int) -> float {
        self.bind()
        __nbody_elements(i, j, self.G)
        return __nbody_out(7)
    }
}

// the constant of gravitation in SI units (m^3 / (kg s^2)), CODATA 2022
def G() -> float {
    return 6.6743e-11
}

// an empty simulation; g: the constant of gravitation in the units chosen (1.0, or nbody.G() in SI)
def simulation(g: float) -> Simulation {
    return Simulation([], [], [], [], [], [], [], [], [], [], g, 0.0, 0.0, 0.0, 2, false, 0.0, 400, "", 1.0, "", 1.0, "leapfrog", 1e-9, "direct", 0.5, 0)
}

// V0.68: the constant of gravitation in astronomical units, solar masses and years of 365.25 days:
// GM of the Sun 1.32712440041e20 m^3/s^2 (IAU 2009, TDB), 1 au = 149597870700 m (IAU 2012)
def G_solar() -> float {
    return 39.476926421093573
}

// an empty simulation in au, solar masses and years, with IAS15
def simulation_solar() -> Simulation {
    let s: Simulation = simulation(G_solar())
    s.time_unit = "an"
    s.length_unit = "UA"
    s.integrator = "ias15"
    return s
}

// V0.69: a simulation saved by s.save(path)
def load(path: string) -> Simulation {
    let n: int = __nbody_load(path)
    let s: Simulation = simulation(__nbody_loaded(-1, 0))
    s.t = __nbody_loaded(-1, 1)
    s.dt = __nbody_loaded(-1, 2)
    s.soft = __nbody_loaded(-1, 3)
    s.epsilon = __nbody_loaded(-1, 4)
    s.merge = __nbody_loaded(-1, 5) != 0.0
    s.order = int(__nbody_loaded(-1, 6))
    s.speed = __nbody_loaded(-1, 7)
    s.trail = int(__nbody_loaded(-1, 8))
    s.time_scale = __nbody_loaded(-1, 9)
    s.length_scale = __nbody_loaded(-1, 10)
    s.integrator = __nbody_loaded_text(-1)
    s.time_unit = __nbody_loaded_text(-2)
    s.length_unit = __nbody_loaded_text(-3)
    s.gravity = __nbody_loaded_text(-4)
    s.theta = __nbody_loaded(-1, 11)
    s.threads = int(__nbody_loaded(-1, 12))
    for i in range(n) {
        s.add3(__nbody_loaded_text(i), __nbody_loaded(i, 0), __nbody_loaded(i, 1), __nbody_loaded(i, 2), __nbody_loaded(i, 3), __nbody_loaded(i, 4), __nbody_loaded(i, 5), __nbody_loaded(i, 6))
        s.r[i] = __nbody_loaded(i, 7)
        s.colors[i] = int(__nbody_loaded(i, 8))
    }
    s.flags()
    return s
}

// the true anomaly (degrees) for the mean anomaly M (degrees): Kepler's equation
def true_anomaly(M: float, e: float) -> float {
    return __nbody_anomaly(M, e)
}

// the Sun and the eight planets on January 1st, 2000 at 12:00 (J2000), around the center of mass, in
// the plane of the ecliptic: the elements of JPL valid from 1800 to 2050 (E. M. Standish, "Keplerian
// elements for approximate positions of the major planets"), the masses of the IAU (2009); the Earth
// is the Earth and the Moon together.  Approximate places (to some minutes of arc), for real dynamics.
def solar_system() -> Simulation {
    let s: Simulation = simulation_solar()
    let soleil: int = s.add("Soleil", 1.0, 0.0, 0.0, 0.0, 0.0)
    s.radius(soleil, 0.00465047)
    s.color(soleil, "#fdd835")
    let noms: list[string] = ["Mercure", "Vénus", "Terre", "Mars", "Jupiter", "Saturne", "Uranus", "Neptune"]
    let rapports: list[float] = [6023600.0, 408523.71, 328900.5614, 3098708.0, 1047.3486, 3497.898, 22902.98, 19412.24]
    let a: list[float] = [0.38709927, 0.72333566, 1.00000261, 1.52371034, 5.20288700, 9.53667594, 19.18916464, 30.06992276]
    let e: list[float] = [0.20563593, 0.00677672, 0.01671123, 0.09339410, 0.04838624, 0.05386179, 0.04725744, 0.00859048]
    let inc: list[float] = [7.00497902, 3.39467605, -0.00001531, 1.84969142, 1.30439695, 2.48599187, 0.77263783, 1.77004347]
    let L: list[float] = [252.25032350, 181.97909950, 100.46457166, -4.55343205, 34.39644051, 49.95424423, 313.23810451, -55.12002969]
    let varpi: list[float] = [77.45779628, 131.60246718, 102.93768193, -23.94362959, 14.72847983, 92.59887831, 170.95427630, 44.96476227]
    let node: list[float] = [48.33076593, 76.67984255, 0.0, 49.55953891, 100.47390909, 113.66242448, 74.01692503, 131.78422574]
    let couleurs: list[string] = ["#b0bec5", "#ffe082", "#4fc3f7", "#ff7043", "#ffb74d", "#ffe0b2", "#80deea", "#5c6bc0"]
    for k in range(8) {
        let f: float = true_anomaly(L[k] - varpi[k], e[k])
        let i: int = s.add_orbit(noms[k], 1.0 / rapports[k], soleil, a[k], e[k], inc[k], node[k], varpi[k] - node[k], f)
        s.color(i, couleurs[k])
    }
    s.center()
    return s
}
