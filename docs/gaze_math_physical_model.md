# Gaze Tracking Physical Geometry & Projection Model

This document explains the physical geometric models and mathematical formulas used by the **godot-gaze** projection engine to map a 3D gaze vector in camera space to 2D screen pixels.

---

## 1. Coordinate Systems

### OpenCV Camera Space
* **Origin ($C_{\text{cv}}$)**: Optical center of the camera lens.
* **$X_{\text{cv}}$-axis**: Horizontal, pointing right from the camera's perspective (corresponding to the user's left).
* **$Y_{\text{cv}}$-axis**: Vertical, pointing down.
* **$Z_{\text{cv}}$-axis**: Optical axis, pointing forward into the camera's view cone (towards the user).

### OpenCV Face Space
This is the space of the 3D facial model used by YuNet's `solvePnP` solver (eyes, nose, mouth corners):
* **Origin**: Midpoint of the eyes at depth 0.
* **$X_{\text{face\_cv}}$-axis**: Horizontal, pointing to the face's own left (image right).
* **$Y_{\text{face\_cv}}$-axis**: Vertical, pointing down.
* **$Z_{\text{face\_cv}}$-axis**: Perpendicular to the face plane, pointing to the back of the head. (The nose points along $-Z_{\text{face\_cv}}$).

### GodotGaze Camera Space (Standard Camera Space)
This space is aligned with standard right-handed graphics conventions:
* **Origin ($C_{\text{cam}}$)**: Optical center of the camera lens.
* **$X_{\text{cam}}$-axis**: Horizontal, pointing right from the camera's perspective (left of display/user's left).
* **$Y_{\text{cam}}$-axis**: Vertical, pointing up.
* **$Z_{\text{cam}}$-axis**: Perpendicular to the display, pointing out the back of the camera **away** from the camera's view cone. Thus, $-Z_{\text{cam}}$ points towards the user in front of the display, meaning the user is located at negative Z ($z_{\text{cam}} < 0$).
* **Mapping from OpenCV Camera Space**:
  To correct the vertical direction (Y points down in OpenCV, up in GodotGaze) and the forward/backward direction (Z points forward/into-view-cone in OpenCV, backward/away-from-view-cone in GodotGaze), we apply a **$180^\circ$ pitch rotation around the X-axis**:
  $$M = R_X(180^\circ) = \begin{pmatrix} 1 & 0 & 0 \\ 0 & -1 & 0 \\ 0 & 0 & -1 \end{pmatrix}$$
  This maps the coordinates as:
  $$X_{\text{cam}} = X_{\text{cv}}$$
  $$Y_{\text{cam}} = -Y_{\text{cv}}$$
  $$Z_{\text{cam}} = -Z_{\text{cv}}$$

### GodotGaze Face Space (Face Space)
This is the standard local coordinate space of the user's head:
* **Origin**: Midpoint of the eyes.
* **$X_{\text{local}}$-axis**: Horizontal, pointing to the user's right ear.
* **$Y_{\text{local}}$-axis**: Vertical, pointing up the face.
* **$Z_{\text{local}}$-axis**: Perpendicular to the face plane, pointing to the back of the head. Thus, the nose points along $-Z_{\text{local}}$ (forward).
* **Mapping to OpenCV Face Space**:
  To align with OpenCV's face coordinate system (where Y points down the face and X points to the face's own left), we apply a **$180^\circ$ roll rotation around the Z-axis**:
  $$R_Z(180^\circ) = \begin{pmatrix} -1 & 0 & 0 \\ 0 & -1 & 0 \\ 0 & 0 & 1 \end{pmatrix}$$
  This flips both the local X and Y axes while keeping the Z-axis (pointing to the back of the head) unchanged:
  $$X_{\text{face\_cv}} = -X_{\text{local}}$$
  $$Y_{\text{face\_cv}} = -Y_{\text{local}}$$
  $$Z_{\text{face\_cv}} = Z_{\text{local}}$$

### 1.5. Transformation between Spaces
Any local point $P_{\text{local}}$ in GodotGaze Face Space is mapped to GodotGaze Camera Space $P_{\text{cam}}$ via:
$$P_{\text{cam}} = R_{\text{cam}} \cdot P_{\text{local}} + t_{\text{cam}}$$
where:
$$R_{\text{cam}} = M \cdot R_{\text{cv}} \cdot M \cdot R_Y(180^\circ)$$
$$t_{\text{cam}} = M \cdot t_{\text{cv}}$$
Here, $R_{\text{cv}}$ and $t_{\text{cv}}$ are the rotation and translation returned by OpenCV's `solvePnP` solver.

This can be expressed as a chain of 3D transforms:
$$T_{\text{ggaze\_face\_to\_ggaze\_cam}} = T_{\text{cv\_cam\_to\_ggaze\_cam}} \cdot T_{\text{cv\_face\_to\_cv\_cam}} \cdot T_{\text{ggaze\_face\_to\_cv\_face}}$$
where:
* $T_{\text{cv\_cam\_to\_ggaze\_cam}} = \text{Transform}(R_X(180^\circ), \vec{0})$ is the $180^\circ$ pitch rotation mapping OpenCV Camera Space to GodotGaze Camera Space.
* $T_{\text{cv\_face\_to\_cv\_cam}} = \text{Transform}(R_{\text{cv}}, t_{\text{cv}})$ is the OpenCV Face-to-Camera transform.
* $T_{\text{ggaze\_face\_to\_cv\_face}} = \text{Transform}(R_Z(180^\circ), \vec{0})$ is the $180^\circ$ roll rotation mapping GodotGaze Face Space to OpenCV Face Space.
  *(Note that the net effect $R_X(180^\circ) \cdot R_Z(180^\circ) = R_Y(180^\circ)$, which is a $180^\circ$ yaw rotation around the vertical axis).*


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

---

## 7. OpenVINO Gaze Estimation Model (ADAS-0002) Conventions

The OpenModelZoo gaze estimation network (`gaze-estimation-adas-0002`) operates with distinct input/output coordinate space and sign conventions which are reconciled in the wrapper layer:

### Input Feature Preprocessing
*   **Eye Crop Inputs**: The model defines its inputs from the camera/viewer's perspective.
    *   `"left_eye_image"` receives the crop of the eye appearing on the **left side of the image frame** (which is the subject's anatomical **right eye**).
    *   `"right_eye_image"` receives the crop of the eye appearing on the **right side of the image frame** (the subject's anatomical **left eye**).
    *   *Effect*: The eye crops are swapped relative to anatomical labeling when passed to the model.
*   **Head Pose Sign Alignment**: The model expects input head pose angles in degrees with positive-left (yaw), positive-down (pitch), and positive-clockwise (roll) orientations:
    *   **Yaw**: Negated (`-crops.head_pose_rotation.y`), mapping negative SolvePnP yaw to positive model yaw.
    *   **Pitch**: Direct (`crops.head_pose_rotation.x`).
    *   **Roll**: Negated (`-crops.head_pose_rotation.z`), aligning the roll coordinate signs.

### Output Vector Mapping
The 3D direction vector output by the model (`raw_gaze_dir`) is mapped to GodotGaze Camera Space:
*   **X Component**: Direct (`raw_gaze_dir.x`), as $+X$ points left in both spaces.
*   **Y Component**: Direct (`raw_gaze_dir.y`), as $+Y$ points up in both spaces.
*   **Z Component**: Negated (`-raw_gaze_dir.z`), reversing the optical direction so the unit vector points forward towards the screen plane ($Z_{\text{cam}} = 0$, $v_z > 0$) rather than backward into the camera ($v_z < 0$).
