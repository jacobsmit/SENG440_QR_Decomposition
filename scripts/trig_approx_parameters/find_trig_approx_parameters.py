import os
import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import minimize

# 12-bit fixed-point scaling factor (1.0 = 2048)
SCALE = 2048
MAX_X = 0.786  # Max output of arctan approximation

# Breakpoints with denominators as powers of 2 (multiples of 0.125)
bp_candidates = [0.125, 0.25, 0.375, 0.5, 0.625, 0.75]

def optimize_sine_3seg(bp1, x_vals, y_vals):
    # Segments on [0, MAX_X]:
    # 1: [0, bp1] -> m1*x
    # 2: [bp1, MAX_X] -> m2*x + b2
    def error(params):
        m1, m2 = params
        b2 = (m1 - m2) * bp1
        
        preds = np.where(x_vals <= bp1, m1 * x_vals, m2 * x_vals + b2)
        return np.max(np.abs(y_vals - preds))
    
    m1_g = np.sin(bp1/2)/(bp1/2)
    m2_g = (np.sin(MAX_X) - np.sin(bp1))/(MAX_X - bp1)
    
    res = minimize(error, x0=[m1_g, m2_g], method='Nelder-Mead')
    m1, m2 = res.x
    b2 = (m1 - m2) * bp1
    return res.fun, (m1, m2, b2)

def optimize_cosine_4seg(bp1, x_vals, y_vals):
    # Segments on [0, MAX_X]:
    # 1: [0, bp1] -> m1*x + b1
    # 2: [bp1, MAX_X] -> m2*x + b2
    def error(params):
        m1, m2 = params
        b1 = 1.0
        b2 = (m1 - m2) * bp1 + b1
        
        preds = np.where(x_vals <= bp1, m1 * x_vals + b1, m2 * x_vals + b2)
        return np.max(np.abs(y_vals - preds))
    
    m1_g = (np.cos(bp1) - 1.0)/bp1
    m2_g = (np.cos(MAX_X) - np.cos(bp1))/(MAX_X - bp1)
    
    res = minimize(error, x0=[m1_g, m2_g], method='Nelder-Mead')
    m1, m2 = res.x
    b1 = 1.0
    b2 = (m1 - m2) * bp1 + b1
    return res.fun, (m1, m2, b1, b2)

x_vals = np.linspace(0, MAX_X, 2000)
y_sin = np.sin(x_vals)
y_cos = np.cos(x_vals)

best_bp_sin = None
best_err_sin = float('inf')
best_p_sin = None

best_bp_cos = None
best_err_cos = float('inf')
best_p_cos = None

print(f"Searching for optimal breakpoints over restricted domain [0, {MAX_X}]...")

for bp1 in bp_candidates:
    err_s, p_s = optimize_sine_3seg(bp1, x_vals, y_sin)
    if err_s < best_err_sin:
        best_err_sin = err_s
        best_bp_sin = bp1
        best_p_sin = p_s
        
    err_c, p_c = optimize_cosine_4seg(bp1, x_vals, y_cos)
    if err_c < best_err_cos:
        best_err_cos = err_c
        best_bp_cos = bp1
        best_p_cos = p_c

def print_results(name, bp1, err, p, is_sine):
    bp1_fixed = int(bp1 * SCALE)
    
    if is_sine:
        m1, m2, b2 = p
        b1 = 0.0
    else:
        m1, m2, b1, b2 = p
        
    m1_f = int(m1 * SCALE)
    b1_f = int(b1 * SCALE * SCALE)
    m2_f = int(m2 * SCALE)
    b2_f = int(b2 * SCALE * SCALE)

    print(f"\n[{name} Approx]")
    print(f"Max Error:   {err*100:.2f}%")
    print(f"Breakpoint:  {bp1_fixed}")
    print(f"Params Seg1: m={m1_f}, b={b1_f}")
    print(f"Params Seg2: m={m2_f}, b={b2_f}")

print_results("Sine (3 Segments)", best_bp_sin, best_err_sin, best_p_sin, is_sine=True)
print_results("Cosine (4 Segments)", best_bp_cos, best_err_cos, best_p_cos, is_sine=False)

# --- Evaluate Given Arctan Error ---
x_vals_atan = np.linspace(-1, 1, 2000)
y_true_atan = np.arctan(x_vals_atan)
y_approx_atan = np.zeros_like(x_vals_atan)

for k, x in enumerate(x_vals_atan):
    if x > 0.5:
        y_approx_atan[k] = 0.644 * x + 0.142
    elif x < -0.5:
        y_approx_atan[k] = 0.644 * x - 0.142
    else:
        y_approx_atan[k] = 0.928 * x

err_atan = np.max(np.abs(y_true_atan - y_approx_atan))

print(f"\n========================================")
print(f" Final Error Comparison (Domain Bounded to 0.786)")
print(f"========================================")
print(f"Given Arctan Approx Error:  {err_atan*100:.2f}%")
print(f"Optimized Sine Error:       {best_err_sin*100:.2f}%")
print(f"Optimized Cosine Error:     {best_err_cos*100:.2f}%")
print(f"\nRequirement check (Sine < Arctan):   {'PASS' if best_err_sin < err_atan else 'FAIL'}")
print(f"Requirement check (Cosine < Arctan): {'PASS' if best_err_cos < err_atan else 'FAIL'}")

# --- Generate Plot ---
print("\nGenerating plot...")
fig, axs = plt.subplots(1, 3, figsize=(15, 5))

axs[0].plot(x_vals_atan, y_true_atan, label='True arctan(x)', linewidth=2)
axs[0].plot(x_vals_atan, y_approx_atan, '--', label='Approx (3 Seg)', linewidth=2)
axs[0].set_title('Arctan Approximation')
axs[0].legend()
axs[0].grid(True)
axs[0].set_ylim(-1.1, 1.1)

x_trig = np.linspace(-MAX_X, MAX_X, 400)
y_sin_true = np.sin(x_trig)
m1_s, m2_s, b2_s = best_p_sin
bp1_s = best_bp_sin
sin_approx = np.where(np.abs(x_trig) <= bp1_s, m1_s * np.abs(x_trig),
                      m2_s * np.abs(x_trig) + b2_s) * np.sign(x_trig)
axs[1].plot(x_trig, y_sin_true, label='True sin(x)', linewidth=2)
axs[1].plot(x_trig, sin_approx, '--', label='Approx (3 Seg)', linewidth=2)
axs[1].set_title('Sine Approximation (Bounded)')
axs[1].legend()
axs[1].grid(True)
axs[1].set_ylim(-1.1, 1.1)

y_cos_true = np.cos(x_trig)
m1_c, m2_c, b1_c, b2_c = best_p_cos
bp1_c = best_bp_cos
cos_approx = np.where(np.abs(x_trig) <= bp1_c, m1_c * np.abs(x_trig) + b1_c,
                      m2_c * np.abs(x_trig) + b2_c)
axs[2].plot(x_trig, y_cos_true, label='True cos(x)', linewidth=2)
axs[2].plot(x_trig, cos_approx, '--', label='Approx (4 Seg)', linewidth=2)
axs[2].set_title('Cosine Approximation (Bounded)')
axs[2].legend()
axs[2].grid(True)
axs[2].set_ylim(-1.1, 1.1)

plt.tight_layout()
script_dir = os.path.dirname(os.path.abspath(__file__))
plot_path = os.path.join(script_dir, 'trig_approximations.png')
plt.savefig(plot_path, dpi=300)
print(f"Plot saved to {plot_path}!")