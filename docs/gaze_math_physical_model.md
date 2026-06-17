# Gaze Tracking Physical Geometry & Projection Model

This document explains the physical geometric models and mathematical formulas used by the **godot-gaze** projection engine to map a 3D gaze vector in camera space to 2D screen pixels.

---

## 1. Coordinate Systems

### Camera Space (OpenCV Standard)
* **Origin ($C$)**: Optical center of the camera lens.
* **X-axis**: Horizontal, pointing right.
* **Y-axis**: Vertical, pointing down.
* **Z-axis**: Optical axis, pointing forward (toward the user).

### Screen Local Space
* **Origin ($S$)**: Center of the display screen.
* **X-axis**: Horizontal, pointing right (in mm).
* **Y-axis**: Vertical, pointing down (in mm).
* **Z-axis**: Perpendicular to the screen plane, pointing toward the user (in mm).
* The flat screen plane is defined by the equation $z_{screen} = 0$.

---

## 2. Camera-to-Screen Transformation

Let the camera's physical position in Screen Local Space be configured as:
* **Camera Offset ($O_{cam}$)**: Vector $(x_{off}, y_{off}, z_{off})$ in mm.
* **Camera Tilt ($\theta$)**: Downward tilt angle in degrees about the camera's local X-axis.

The rotation matrix $R$ rotating vectors from Camera Space to Screen Space (for tilt angle $\theta$ in radians) is:
$$R = R_x(\theta) = \begin{pmatrix} 1 & 0 & 0 \\ 0 & \cos\theta & -\sin\theta \\ 0 & \sin\theta & \cos\theta \end{pmatrix}$$

For any point $P_{cam}$ in Camera Space, its position in Screen Local Space is:
$$P_{screen} = R \cdot P_{cam} + O_{cam}$$

Substituting components:
$$x_s = x_{cam} + x_{off}$$
$$y_s = y_{cam} \cos\theta - z_{cam} \sin\theta + y_{off}$$
$$z_s = y_{cam} \sin\theta + z_{cam} \cos\theta + z_{off}$$

---

## 3. 3D Ray-Plane Intersection

A gaze ray starting at origin $P_{0\_cam} = (x_0, y_0, z_0)$ with normalized direction vector $V_{cam} = (v_x, v_y, v_z)$ in Camera Space is parameterized by $t$:
$$P_{cam}(t) = P_{0\_cam} + t \cdot V_{cam}$$

To find where it intersects the screen plane, we project the ray into Screen Local Space and solve for $z_s(t) = 0$:
$$z_s(t) = (y_0 + t v_y) \sin\theta + (z_0 + t v_z) \cos\theta + z_{off} = 0$$

Solving for $t$:
$$t = - \frac{y_0 \sin\theta + z_0 \cos\theta + z_{off}}{v_y \sin\theta + v_z \cos\theta}$$

If $t < 0$, the gaze ray points away from the screen (no intersection). 
Otherwise, we compute the camera-space intersection point:
$$P_{int\_cam} = P_{0\_cam} + t \cdot V_{cam}$$

And transform it to the screen physical coordinates $(x_s, y_s)$ in mm:
$$x_s = P_{int\_cam}.x + x_{off}$$
$$y_s = P_{int\_cam}.y \cos\theta - P_{int\_cam}.z \sin\theta + y_{off}$$

---

## 4. Screen MM-to-Pixel Mapping

Let:
* Screen resolution in pixels be $(W_{px}, H_{px})$ (e.g., $1920 \times 1080$).
* Screen physical size in mm be $(W_{mm}, H_{mm})$ (e.g., $527 \times 296$).

We map $(x_s, y_s)$ in mm relative to the screen center to screen pixels $(x_{px}, y_{px})$, where top-left is $(0, 0)$:
$$x_{px} = \frac{W_{px}}{2} + x_s \cdot \frac{W_{px}}{W_{mm}}$$
$$y_{px} = \frac{H_{px}}{2} + y_s \cdot \frac{H_{px}}{H_{mm}}$$

---

## 5. Calibration & Error Correction

Individual eye shape, eyeball depth, and camera mount errors cause systematic estimation deviations (typically $O(2^\circ - 5^\circ)$). We correct this using two models:

### 3D Spherical Angular Calibration
We apply pitch/yaw angle biases to the raw gaze vector $V = (v_x, v_y, v_z)$ before intersection calculation:
1. Extract spherical angles:
   $$\phi_{yaw} = \operatorname{atan2}(v_x, v_z)$$
   $$\psi_{pitch} = \operatorname{asin}(v_y)$$
2. Apply calibration biases:
   $$\phi_{calib} = \phi_{yaw} + \text{bias\_yaw}$$
   $$\psi_{calib} = \psi_{pitch} + \text{bias\_pitch}$$
3. Re-project to the calibrated unit vector:
   $$V_{calib} = (\sin\phi_{calib} \cos\psi_{calib},\; \sin\psi_{calib},\; \cos\phi_{calib} \cos\psi_{calib})$$

During calibration trigger (staring at a target screen pixel $P_{target}$):
1. Transform $P_{target}$ back to Camera Space ($P_{cam\_target}$) by reversing the rotation and translation:
   $$x_{cam\_target} = x_{s\_target} - x_{off}$$
   $$y_{cam\_target} = (y_{s\_target} - y_{off}) \cos\theta - z_{off} \sin\theta$$
   $$z_{cam\_target} = -(y_{s\_target} - y_{off}) \sin\theta - z_{off} \cos\theta$$
2. The required gaze vector is:
   $$V_{req} = (P_{cam\_target} - P_{0\_cam}).normalized()$$
3. Compute the angular differences to store as $\text{bias\_pitch}$ and $\text{bias\_yaw}$:
   $$\text{bias\_yaw} = \phi_{req} - \phi_{yaw}$$
   $$\text{bias\_pitch} = \psi_{req} - \psi_{pitch}$$

### 2D Pixel-Space Calibration
Simple translational delta applied after the pixel mapping:
$$x_{px\_final} = x_{px\_projected} + \text{bias\_pixel\_x}$$
$$y_{px\_final} = y_{px\_projected} + \text{bias\_pixel\_y}$$

During calibration trigger, the biases are directly calculated as:
$$\text{bias\_pixel\_x} = x_{target} - x_{projected}$$
$$\text{bias\_pixel\_y} = y_{target} - y_{projected}$$

---

## 6. Real-Time Depth Triangulation (Z Engine)

Using a pinhole camera model, the distance $Z$ from the camera sensor is calculated from:
* **Interpupillary Distance (IPD)**: $63.0$ mm (constant average adult).
* **Focal Length in Pixels ($f_{px}$)**: Screen width or height scale multiplier.
* **Pixel Distance ($d_{px}$)**: Detected distance between eye centers in the 2D frame.

$$Z_{mm} = \frac{IPD_{mm} \cdot f_{px}}{d_{px}}$$
$$Z_{cm} = \frac{Z_{mm}}{10.0}$$
