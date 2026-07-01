import cv2
import numpy as np

# TODO: Is this a real test? THEN RUN IT AS A TEST.

print("Testing Gaze Model with OpenCV DNN...")
net = cv2.dnn.readNetFromONNX("project/addons/godot-gaze/models/gaze-estimation-adas-0002.onnx")

# Inputs are left_eye_image (1, 3, 60, 60), right_eye_image (1, 3, 60, 60), head_pose_angles (1, 3)
left_eye = np.random.randn(1, 3, 60, 60).astype(np.float32)
right_eye = np.random.randn(1, 3, 60, 60).astype(np.float32)
head_pose = np.array([[0.0, 0.0, 0.0]], dtype=np.float32)

net.setInput(left_eye, "left_eye_image")
net.setInput(right_eye, "right_eye_image")
net.setInput(head_pose, "head_pose_angles")

out = net.forward("gaze_vector/sink_port_0")
print("Output shape:", out.shape)
print("Output values:", out)
