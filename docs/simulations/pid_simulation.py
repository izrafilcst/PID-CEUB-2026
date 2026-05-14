"""
Simulação PID para LFR — validação matemática pré-C++
Rodando 3 cenários: P, PD, PID
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

class PID:
    def __init__(self, kp, ki, kd, dt=0.01, out_min=-255, out_max=255):
        self.kp, self.ki, self.kd = kp, ki, kd
        self.dt = dt
        self.out_min, self.out_max = out_min, out_max
        self.integral = 0.0
        self.prev_measurement = 0.0

    def compute(self, setpoint, measurement):
        error = setpoint - measurement
        self.integral = np.clip(self.integral + error * self.dt, self.out_min/self.ki if self.ki > 0 else -1e9, self.out_max/self.ki if self.ki > 0 else 1e9)
        derivative = -(measurement - self.prev_measurement) / self.dt  # derivada no processo
        self.prev_measurement = measurement
        output = self.kp * error + self.ki * self.integral + self.kd * derivative
        return float(np.clip(output, self.out_min, self.out_max)), error, self.kp*error, self.ki*self.integral, self.kd*derivative

def simulate(kp, ki, kd, steps=500, dt=0.01):
    pid = PID(kp, ki, kd, dt)
    t = np.arange(steps) * dt
    track = 30 * np.sin(2 * np.pi * t / 3.0)  # pista senoidal
    robot = 0.0
    alpha = 0.3  # inércia

    positions, errors, p_terms, i_terms, d_terms = [], [], [], [], []

    for i, sp in enumerate(track):
        out, err, p, ig, d = pid.compute(sp, robot)
        robot = alpha * (robot + out * 0.05) + (1 - alpha) * robot
        positions.append(robot)
        errors.append(err)
        p_terms.append(p)
        i_terms.append(ig)
        d_terms.append(d)

    return t, track, np.array(positions), np.array(errors), np.array(p_terms), np.array(i_terms), np.array(d_terms)

scenarios = [
    ("Apenas P (Kp=2)", 2.0, 0.0, 0.0, 'red'),
    ("PD (Kp=2, Kd=8)", 2.0, 0.0, 8.0, 'blue'),
    ("PID (Kp=2, Ki=0.01, Kd=8)", 2.0, 0.01, 8.0, 'green'),
]

fig, axes = plt.subplots(4, 1, figsize=(12, 14))
fig.suptitle('Simulação PID — LFR ESP32', fontsize=14, fontweight='bold')

for name, kp, ki, kd, color in scenarios:
    t, track, pos, err, p_t, i_t, d_t = simulate(kp, ki, kd)

    mae = np.mean(np.abs(err))
    max_err = np.max(np.abs(err))
    print(f"{name}: MAE={mae:.2f}mm  MaxErr={max_err:.2f}mm  Overshoot={'SIM' if np.any(pos * np.sign(track) > np.abs(track) * 1.05) else 'NÃO'}")

    axes[0].plot(t, pos, color=color, label=name, linewidth=1.5)
    axes[1].plot(t, err, color=color, linewidth=1.0)
    axes[2].plot(t, p_t, color=color, linewidth=1.0)
    axes[3].plot(t, (pos + 128) / 255 * 255, color=color, label=f'L {name}', linewidth=1.0)

axes[0].plot(t, track, 'k--', label='Pista', linewidth=2)
axes[0].set_ylabel('Posição (mm)'); axes[0].legend(fontsize=8); axes[0].grid(True)
axes[1].set_ylabel('Erro (mm)'); axes[1].grid(True); axes[1].axhline(0, color='k', lw=0.5)
axes[2].set_ylabel('Termo P'); axes[2].grid(True)
axes[3].set_ylabel('PWM Motor L'); axes[3].set_xlabel('Tempo (s)'); axes[3].grid(True)

plt.tight_layout()
plt.savefig('/home/linux/dev/carro-pid/docs/simulations/pid_response.png', dpi=120, bbox_inches='tight')
print("Salvo: docs/simulations/pid_response.png")
