# Mathematical Calibration Model

This document outlines the mathematical model and optimization formulation behind the 3D screen geometry and biological calibration system in `godot-gaze`.

---

## 1. Parameterization

The forward projection system maps a 3D gaze ray defined in OpenCV camera space back to 2D local viewport pixels. Instead of treating total monitor size as an independent parameter, we parameterize the display using the display **Pixel Size** (pixel pitch) in millimeters:

$$\mathbf{s}_{\text{pixel\_size}} = (s_x, s_y) \quad \text{[mm/pixel]}$$

For a display with hardware resolution $(W_{\text{pixels}}, H_{\text{pixels}})$, the physical dimensions of the monitor in millimeters are dynamically derived:

$$W_{\text{mm}} = W_{\text{pixels}} \cdot s_x$$
$$H_{\text{mm}} = H_{\text{pixels}} \cdot s_y$$

This formulation is **scale-invariant**: if the window size, window position, or screen resolution change (e.g., entering fullscreen, resizing, or switching displays), the physical pixel pitch $s_x, s_y$ remains constant, allowing the system to scale physical screen coordinate transformations dynamically without introducing projection errors.

---

## 2. Coordinate Systems and Projection

Let:
- $\mathbf{o}_{\text{cam}}$ be the 3D gaze origin in camera space (in millimeters).
- $\mathbf{v}_{\text{raw}}$ be the raw 3D gaze direction vector in camera space.

### Step 2.1: Biological Calibration Correction (Angle Kappa)
User-specific biological offsets (such as the angle kappa between the eye's visual and optical axes) are corrected by applying angular pitch ($\alpha$) and yaw ($\beta$) biases to the raw gaze vector:

$$\theta_{\text{yaw}} = \text{atan2}(v_x, v_z) + \beta$$
$$\theta_{\text{pitch}} = \text{asin}(v_y) + \alpha$$

$$\mathbf{v}'_{\text{cam}} = \begin{pmatrix} \sin(\theta_{\text{yaw}}) \cos(\theta_{\text{pitch}}) \\ \sin(\theta_{\text{pitch}}) \\ \cos(\theta_{\text{yaw}}) \cos(\theta_{\text{pitch}}) \end{pmatrix}$$

### Step 2.2: Ray-Plane Projection
The physical screen plane is defined in camera space by the camera placement offset $\mathbf{O}_{\text{camera}} = (O_x, O_y, O_z)$ and camera tilt angle $\theta_{\text{tilt}}$ (degrees).

Using the generic ray-plane intersection, we solve for the distance parameter $t$ along the ray:

$$\mathbf{p}_{\text{cam}}(t) = \mathbf{o}_{\text{cam}} + t \cdot \mathbf{v}'_{\text{cam}}$$

Intersecting this with the tilted screen plane at $Z_{\text{screen}} = 0$ in Display Space yields the intersection point in millimeters relative to the screen center:

$$x_{\text{screen, mm}} = O_x - p_{\text{cam}, x}$$
$$y_{\text{screen, mm}} = -O_y - (p_{\text{cam}, y} \cos \theta_{\text{tilt}} + p_{\text{cam}, z} \sin \theta_{\text{tilt}})$$

Converting to physical display pixels:

$$x_{\text{pixel}} = \frac{x_{\text{screen, mm}}}{s_x} + \frac{W_{\text{pixels}}}{2}$$
$$y_{\text{pixel}} = \frac{y_{\text{screen, mm}}}{s_y} + \frac{H_{\text{pixels}}}{2}$$

---

## 3. Calibration Optimizer (Inverse Solver)

During calibration, the user looks at $N = 5$ screen targets. The system collects samples containing:
- $\mathbf{o}_i$: measured eye origin in camera space.
- $\mathbf{v}_i$: measured raw eye direction in camera space.
- $\mathbf{T}_i$: physical target pixel coordinate on the screen.

We optimize the parameter vector $\mathbf{x} = (s_x, s_y, O_y, O_z, \theta_{\text{tilt}}, \alpha, \beta)$ to minimize the sum of squared screen-space projection errors.

### Bayesian Regularization (Soft Priors)
To prevent parameter correlation and overfitting from a 5-point dataset, we add quadratic penalties to constrain the parameters to physically realistic values:

$$\text{Loss}(\mathbf{x}) = \sum_{i=1}^{N} \|\text{ProjectedPixel}(\mathbf{o}_i, \mathbf{v}_i; \mathbf{x}) - \mathbf{T}_i\|^2 + \sum_{j} \lambda_j (x_j - x_{j,\text{initial}})^2$$

Where:
- $\lambda_{\text{aspect}}$ enforces that the physical pixel size keeps a standard square aspect ratio ($s_x \approx s_y$).
- $\lambda_{\text{size}}$ keeps the pixel size near the platform-estimated DPI/DPR default.
- $\lambda_{\text{camera}}$ keeps camera offsets and tilt near their physical mounting defaults.
- $\lambda_{\text{bias}}$ penalizes large angular biases.

### Nelder-Mead Optimization
The solver uses the derivative-free Nelder-Mead (downhill simplex) algorithm, which maintains an $M+1$ dimensional simplex (where $M=7$ parameters) and updates it via reflection, expansion, contraction, and shrinkage until convergence.

---

## 4. Web & Sandboxing Consistency
On Web platforms where hardware dimensions cannot be queried:
1. The tracker starts with standard laptop/desktop fallback parameters.
2. The solver finds an **"effective screen geometry"** (effective pixel size and camera placements) that is internally consistent with the camera's focal length and raw gaze directions.
3. This effective geometry absorbs browser estimation errors (such as incorrect canvas offsets or page scaling) and maps the projected gaze rays back to target viewport pixels with near-zero error.
