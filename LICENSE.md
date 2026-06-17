# License Agreements

This project, **godot-gaze**, is licensed under the MIT License. A copy of the license is included below, followed by attribution and licensing requirements for third-party libraries and models.

---

## MIT License (Project License)

Copyright (c) 2026 Gaze GDExtension Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## Third-Party Software & Models Licensing

### 1. OpenCV (Open Source Computer Vision Library)
* **License**: Apache License 2.0 (for OpenCV 4.x and newer)
* **Details**: OpenCV is used for frame grabbing, face detection (YuNet DNN), perspective eye warps, and ONNX model evaluation.
* **Apache 2.0 Terms**: You may obtain a copy of the Apache 2.0 License at `http://www.apache.org/licenses/LICENSE-2.0`. Subject to the terms and conditions of this License, each contributor grants to you a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable copyright license to reproduce, prepare derivative works of, publicly display, publicly perform, sublicense, and distribute the Work.

### 2. 1 Euro Filter C++ Implementation
* **License**: BSD 3-Clause License
* **Details**: Used for adaptive smoothing of the projected pixel gaze coordinates.
* **Attribution**: Copyright (c) Nicolas Roussel.
* **BSD 3-Clause Terms**: Redistribution and use in source and binary forms, with or without modification, are permitted provided that:
  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

### 3. YuNet Face Detection Model (cv::FaceDetectorYN)
* **License**: MIT License
* **Details**: Fast, lightweight face detection network distributed in OpenCV's model zoo.
* **Attribution**: Copyright (c) 2021 Shiqi Yu.

### 4. Appearance-Based Gaze CNN Model (ONNX weights)
* **License**: MIT License or Custom Academic Research License
* **Details**: CNN models trained on public datasets (like MPIIFaceGaze or ETH-XGaze) for gaze estimation. Ensure check of model source license if distributing commercial weights.
