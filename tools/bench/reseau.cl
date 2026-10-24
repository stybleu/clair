// XOR : reseau 2-16-1 entraine 200000 fois sur les 4 exemples (descente de gradient).
// noyau: Mlp__train

def sigmoid(x: float) -> float {
    return 1.0 / (1.0 + exp(-x))
}

// generateur pseudo-aleatoire lineaire congruentiel, valeur dans [-1, 1]
def rnd(state: list[int]) -> float {
    state[0] = (state[0] * 1103515245 + 12345) % 2147483648
    return float(state[0]) / 2147483648.0 * 2.0 - 1.0
}

Class Mlp {
    w1: matrix[float]
    b1: list[float]
    w2: list[float]
    b2: float
    h: list[float]

    def predict(x0: float, x1: float) -> float {
        for i in range(rows(self.w1)) {
            self.h[i] = sigmoid(self.b1[i] + self.w1[i][0] * x0 + self.w1[i][1] * x1)
        }
        let s: float = self.b2
        for i in range(len(self.w2)) {
            s = s + self.w2[i] * self.h[i]
        }
        return sigmoid(s)
    }

    def train(x0: float, x1: float, target: float, lr: float) -> float {
        let o: float = self.predict(x0, x1)
        let err: float = o - target
        let d_out: float = err * o * (1.0 - o)
        for i in range(len(self.w2)) {
            let hi: float = self.h[i]
            let dh: float = d_out * self.w2[i] * hi * (1.0 - hi)
            self.w2[i] = self.w2[i] - lr * d_out * hi
            self.b1[i] = self.b1[i] - lr * dh
            self.w1[i][0] = self.w1[i][0] - lr * dh * x0
            self.w1[i][1] = self.w1[i][1] - lr * dh * x1
        }
        self.b2 = self.b2 - lr * d_out
        return err * err
    }
}

def make_mlp(hidden: int, state: list[int]) -> Mlp {
    let net: Mlp = Mlp(matrix(hidden, 2), [], [], 0.0, [])
    for i in range(hidden) {
        net.w1[i][0] = rnd(state)
        net.w1[i][1] = rnd(state)
        net.b1.append(0.0)
        net.h.append(0.0)
    }
    for i in range(hidden) {
        net.w2.append(rnd(state))
    }
    return net
}

def main() -> int {
    let state: list[int] = [42]
    let net: Mlp = make_mlp(16, state)
    let x0: list[float] = [0.0, 0.0, 1.0, 1.0]
    let x1: list[float] = [0.0, 1.0, 0.0, 1.0]
    let y: list[float] = [0.0, 1.0, 1.0, 0.0]

    let loss: float = 0.0
    for epoch in range(200000) {
        loss = 0.0
        for n in range(4) {
            loss = loss + net.train(x0[n], x1[n], y[n], 0.5)
        }
    }

    for n in range(4) {
        let p: float = net.predict(x0[n], x1[n])
        print(int(p * 100.0 + 0.5))
    }
    print(loss / 4.0 < 0.002)
    return 0
}
