# Cart_Pole

# CartPole-v1 Physics Controller

## Overview
This project implements a **Model-Based Controller** for the gymnasium `CartPole-v1` environment. Instead of using Reinforcement Learning to approximate a value function, this controller uses the exact **Equations of Motion** (derived from Newtonian mechanics) to predict the future state of the system.

By performing a 1-step (or multi-step) lookahead using these physics equations, the agent can greedily select the action that minimizes the pole's angle and stabilizes the cart.

---

## 1. Physics Model & Constants
The dynamics are governed by a specific set of physical constants defined in the OpenAI Gym source code.

| Constant | Symbol | Value | Description |
| :--- | :--- | :--- | :--- |
| **Gravity** | $g$ | $9.8 \, m/s^2$ | Standard earth gravity. |
| **Cart Mass** | $M_c$ | $1.0 \, kg$ | Mass of the moving cart. |
| **Pole Mass** | $M_p$ | $0.1 \, kg$ | Mass of the pole. |
| **Total Mass** | $M$ | $1.1 \, kg$ | $M_c + M_p$ |
| **Length** | $L$ | $0.5 \, m$ | Half-length (distance to center of mass). |
| **Force** | $F$ | $10.0 \, N$ | Force applied by the motor. |
| **Time Step** | $\tau$ | $0.02 \, s$ | Duration of one simulation frame (50Hz). |

---

## 2. State Space
The environment state is a continuous 4-dimensional vector:

1.  **$x$**: Cart Position ($0$ is center).
2.  **$\dot{x}$**: Cart Velocity.
3.  **$\theta$**: Pole Angle ($0$ is upright; radians).
4.  **$\dot{\theta}$**: Pole Angular Velocity.

---

## 3. Equations of Motion
To predict the next state, we solve the differential equations for linear ($\ddot{x}$) and angular ($\ddot{\theta}$) acceleration.

### A. Intermediate Force Calculation
First, we compute a helper term `temp` that represents the force contribution from the pole's rotation and the applied motor force:

$$\text{temp} = \frac{F_{app} + M_p L \dot{\theta}^2 \sin\theta}{M}$$

### B. Angular Acceleration ($\ddot{\theta}$)
This equation balances the torque from gravity against the cart's linear motion. The factor $\frac{4}{3}$ accounts for the moment of inertia of a pole rotating around one end:

$$\ddot{\theta} = \frac{g \sin\theta - \cos\theta \cdot \text{temp}}{L \left( \frac{4}{3} - \frac{M_p \cos^2\theta}{M} \right)}$$

### C. Linear Acceleration ($\ddot{x}$)
Once $\ddot{\theta}$ is known, we solve for the cart's acceleration:

$$\ddot{x} = \text{temp} - \frac{M_p L \ddot{\theta} \cos\theta}{M}$$

---

## 4. Euler Integration (Update Step)
The simulation advances time discretely. We calculate the state at time $t+1$ using Euler integration:

* **Position:** $x_{new} = x + \tau \cdot \dot{x}$
* **Velocity:** $\dot{x}_{new} = \dot{x} + \tau \cdot \ddot{x}$
* **Angle:** $\theta_{new} = \theta + \tau \cdot \dot{\theta}$
* **Angular Velocity:** $\dot{\theta}_{new} = \dot{\theta} + \tau \cdot \ddot{\theta}$

---

## 5. Implementation
The following Python function implements the physics logic described above to predict the next state given a current state and action.
