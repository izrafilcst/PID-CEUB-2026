"""
Simulação do array de 8 sensores TCRT5000 — LFR
Modelo gaussiano, validação de linearidade
"""
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def sensor_array(line_pos, spacing_mm, n=8, sigma_mm=9.5, max_val=1023):
    centers = (np.arange(n) - (n-1)/2.0) * spacing_mm
    dists = line_pos - centers
    return max_val * np.exp(-(dists / sigma_mm)**2)

def weighted_position(readings, spacing_mm, n=8):
    centers = (np.arange(n) - (n-1)/2.0) * spacing_mm
    total = np.sum(readings)
    if total < 10:
        return None  # linha perdida
    return np.sum(centers * readings) / total

line_positions = np.linspace(-40, 40, 200)
spacings = [8, 10, 12]
colors = ['red', 'blue', 'green']

fig, axes = plt.subplots(3, 1, figsize=(12, 12))
fig.suptitle('Simulação Sensor Array 8× TCRT5000 — LFR', fontsize=14, fontweight='bold')

# Heatmap com spacing=10mm
readings_matrix = np.array([sensor_array(lp, 10) for lp in line_positions])
im = axes[0].imshow(readings_matrix.T, aspect='auto', origin='lower',
                     extent=[-40, 40, 0, 8], cmap='hot')
axes[0].set_ylabel('Sensor index'); axes[0].set_title('Heatmap: 8 sensores × posição (spacing=10mm)')
plt.colorbar(im, ax=axes[0])

# Linearidade para cada espaçamento
for spacing, color in zip(spacings, colors):
    calculated = []
    for lp in line_positions:
        readings = sensor_array(lp, spacing)
        calc = weighted_position(readings, spacing)
        calculated.append(calc if calc is not None else np.nan)
    calculated = np.array(calculated)

    # Zona linear: onde erro < 2mm
    valid = ~np.isnan(calculated)
    errors = np.abs(calculated[valid] - line_positions[valid])
    linear_mask = errors < 2.0
    linear_range = line_positions[valid][linear_mask]
    if len(linear_range) > 0:
        lin_zone = (linear_range.max() - linear_range.min()) / 2
    else:
        lin_zone = 0

    linearity = 100 * (1 - np.mean(errors[linear_mask]) / 40) if linear_mask.any() else 0
    print(f"Spacing={spacing}mm: Zona linear=±{lin_zone:.1f}mm  Linearidade={linearity:.1f}%")

    axes[1].plot(line_positions, calculated, color=color, label=f'{spacing}mm spacing', linewidth=2)
    axes[2].plot(line_positions[valid], errors, color=color, label=f'{spacing}mm', linewidth=1.5)

axes[1].plot(line_positions, line_positions, 'k--', label='Ideal', linewidth=1)
axes[1].set_ylabel('Posição calculada (mm)'); axes[1].legend(); axes[1].grid(True)
axes[1].set_title('Posição calculada vs real')
axes[2].axhline(2.0, color='k', linestyle='--', label='Limite 2mm')
axes[2].set_ylabel('Erro abs (mm)'); axes[2].set_xlabel('Posição real (mm)')
axes[2].legend(); axes[2].grid(True); axes[2].set_title('Erro de posição')

plt.tight_layout()
plt.savefig('/home/linux/dev/carro-pid/docs/simulations/sensor_linearity.png', dpi=120, bbox_inches='tight')
print("Salvo: docs/simulations/sensor_linearity.png")
