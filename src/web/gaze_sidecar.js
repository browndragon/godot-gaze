// gaze_sidecar.js
// Standalone JavaScript code for the Godot Gaze sidecar tracking loop.
// Uses onnxruntime-web and HTML5 Canvas.

// TODO: This file is too large. Can we break it up into multiple .mjs files for our own peace of mind? At a minimum, the pure math functions can be `gaze_math.js` or `.mjs`, right?

(function () {
  if (window.gazeTracker) return;

  // TODO: What keeps this in sync with the C++ version? Strongly recommend at least linking both with an if-this-then-that change guard.
  // Better would be to introspect these values from the C++ version. Best would be to do it all in C++ ;)
  var FaceModelGeometry = {
    EYE_X: 30.0,
    EYE_Y: -28.676,
    EYE_Z: 0.0,
    DEFAULT_NOSE_Y: -0.5,
    DEFAULT_NOSE_Z: -52.0,
    MOUTH_X: 18.462,
    MOUTH_Y: 31.712,
    MOUTH_Z: -4.55,
  };

  // TODO: Does this need to be in the sidecar? Wouldn't it be easier to read, more efficient etc to perform this in wasm? I honestly don't know, that's a real question! The problem would be I _think_ we want ths available on the web worker thread off of main, which might affect things...?
  // Helper functions for Rodrigues rotation and Perspective-n-Point solver
  function rodriguesToRotationMatrix(r) {
    var theta = Math.hypot(r[0], r[1], r[2]);
    var R = [
      [1, 0, 0],
      [0, 1, 0],
      [0, 0, 1],
    ];
    if (theta < 1e-6) {
      return R;
    }
    var ux = r[0] / theta;
    var uy = r[1] / theta;
    var uz = r[2] / theta;
    var c = Math.cos(theta);
    var s = Math.sin(theta);
    var t = 1 - c;
    R[0][0] = c + ux * ux * t;
    R[0][1] = ux * uy * t - uz * s;
    R[0][2] = ux * uz * t + uy * s;
    R[1][0] = uy * ux * t + uz * s;
    R[1][1] = c + uy * uy * t;
    R[1][2] = uy * uz * t - ux * s;
    R[2][0] = uz * ux * t - uy * s;
    R[2][1] = uz * uy * t + ux * s;
    R[2][2] = c + uz * uz * t;
    return R;
  }

  // TODO: Same "can this matrix math be done in C++" question.
  function projectPoint(P, r, t, fx, fy, cx, cy) {
    var R = rodriguesToRotationMatrix(r);
    var px = R[0][0] * P[0] + R[0][1] * P[1] + R[0][2] * P[2] + t[0];
    var py = R[1][0] * P[0] + R[1][1] * P[1] + R[1][2] * P[2] + t[1];
    var pz = R[2][0] * P[0] + R[2][1] * P[1] + R[2][2] * P[2] + t[2];
    if (Math.abs(pz) < 1e-6) {
      return [cx, cy];
    }
    return [fx * (px / pz) + cx, fy * (py / pz) + cy];
  }

  // TODO: Same "can this matrix math be done in C++" question, and so on for all these math ops.
  function computeResiduals(
    modelPoints,
    imagePoints,
    beta,
    fx,
    fy,
    cx,
    cy,
    residuals,
  ) {
    var r = [beta[0], beta[1], beta[2]];
    var t = [beta[3], beta[4], beta[5]];
    var sse = 0.0;
    for (var i = 0; i < 5; i++) {
      var pProj = projectPoint(modelPoints[i], r, t, fx, fy, cx, cy);
      residuals[2 * i] = imagePoints[i][0] - pProj[0];
      residuals[2 * i + 1] = imagePoints[i][1] - pProj[1];
      sse +=
        residuals[2 * i] * residuals[2 * i] +
        residuals[2 * i + 1] * residuals[2 * i + 1];
    }
    return sse;
  }

  function computeJacobian(modelPoints, beta, fx, fy, cx, cy, J) {
    var eps = 1e-4;
    var perturbedBeta = [0, 0, 0, 0, 0, 0];
    for (var i = 0; i < 6; i++) perturbedBeta[i] = beta[i];

    for (var j = 0; j < 6; j++) {
      perturbedBeta[j] = beta[j] + eps;
      var rPos = [perturbedBeta[0], perturbedBeta[1], perturbedBeta[2]];
      var tPos = [perturbedBeta[3], perturbedBeta[4], perturbedBeta[5]];

      perturbedBeta[j] = beta[j] - eps;
      var rNeg = [perturbedBeta[0], perturbedBeta[1], perturbedBeta[2]];
      var tNeg = [perturbedBeta[3], perturbedBeta[4], perturbedBeta[5]];

      perturbedBeta[j] = beta[j]; // restore

      for (var i = 0; i < 5; i++) {
        var projPos = projectPoint(modelPoints[i], rPos, tPos, fx, fy, cx, cy);
        var projNeg = projectPoint(modelPoints[i], rNeg, tNeg, fx, fy, cx, cy);
        J[2 * i][j] = (projPos[0] - projNeg[0]) / (2.0 * eps);
        J[2 * i + 1][j] = (projPos[1] - projNeg[1]) / (2.0 * eps);
      }
    }
  }

  function solve6x6(A, b, x) {
    var temp = [];
    for (var i = 0; i < 6; i++) {
      temp[i] = [];
      for (var j = 0; j < 6; j++) {
        temp[i][j] = A[i][j];
      }
      temp[i][6] = b[i];
    }
    for (var i = 0; i < 6; i++) {
      var pivot = i;
      for (var r = i + 1; r < 6; r++) {
        if (Math.abs(temp[r][i]) > Math.abs(temp[pivot][i])) {
          pivot = r;
        }
      }
      if (Math.abs(temp[pivot][i]) < 1e-12) {
        return false;
      }
      if (pivot !== i) {
        var swap = temp[i];
        temp[i] = temp[pivot];
        temp[pivot] = swap;
      }
      for (var r = i + 1; r < 6; r++) {
        var factor = temp[r][i] / temp[i][i];
        for (var c = i; c <= 6; c++) {
          temp[r][c] -= factor * temp[i][c];
        }
      }
    }
    for (var i = 5; i >= 0; i--) {
      var sum = temp[i][6];
      for (var j = i + 1; j < 6; j++) {
        sum -= temp[i][j] * x[j];
      }
      x[i] = sum / temp[i][i];
    }
    return true;
  }

  function solvePnpLm(modelPoints, imagePoints, fx, fy, cx, cy, rvec, tvec) {
    var beta = [rvec[0], rvec[1], rvec[2], tvec[0], tvec[1], tvec[2]];
    var lambda = 0.001;
    var maxIters = 100;
    var residuals = new Float64Array(10);
    var prevSse = computeResiduals(
      modelPoints,
      imagePoints,
      beta,
      fx,
      fy,
      cx,
      cy,
      residuals,
    );

    var J = [];
    for (var i = 0; i < 10; i++) {
      J[i] = new Float64Array(6);
    }

    var JtJ = [];
    for (var i = 0; i < 6; i++) {
      JtJ[i] = new Float64Array(6);
    }
    var JtErr = new Float64Array(6);
    var delta = new Float64Array(6);
    var newBeta = new Float64Array(6);

    for (var iter = 0; iter < maxIters; iter++) {
      computeJacobian(modelPoints, beta, fx, fy, cx, cy, J);
      for (var i = 0; i < 6; i++) {
        JtErr[i] = 0.0;
        for (var k = 0; k < 10; k++) {
          JtErr[i] += J[k][i] * residuals[k];
        }
        for (var j = 0; j < 6; j++) {
          JtJ[i][j] = 0.0;
          for (var k = 0; k < 10; k++) {
            JtJ[i][j] += J[k][i] * J[k][j];
          }
        }
      }

      var solved = false;
      for (var inner = 0; inner < 10; inner++) {
        var A = [];
        for (var i = 0; i < 6; i++) {
          A[i] = new Float64Array(6);
          for (var j = 0; j < 6; j++) {
            A[i][j] = JtJ[i][j];
          }
          A[i][i] += lambda;
        }

        if (solve6x6(A, JtErr, delta)) {
          for (var i = 0; i < 6; i++) {
            newBeta[i] = beta[i] + delta[i];
          }
          var newSse = computeResiduals(
            modelPoints,
            imagePoints,
            newBeta,
            fx,
            fy,
            cx,
            cy,
            residuals,
          );
          if (newSse < prevSse) {
            lambda *= 0.1;
            prevSse = newSse;
            for (var i = 0; i < 6; i++) beta[i] = newBeta[i];
            solved = true;
            break;
          } else {
            lambda *= 10.0;
          }
        } else {
          lambda *= 10.0;
        }
      }
      if (!solved) {
        break;
      }
      var deltaNorm = Math.hypot(
        delta[0],
        delta[1],
        delta[2],
        delta[3],
        delta[4],
        delta[5],
      );
      if (deltaNorm < 1e-6) {
        break;
      }
    }

    rvec[0] = beta[0];
    rvec[1] = beta[1];
    rvec[2] = beta[2];
    tvec[0] = beta[3];
    tvec[1] = beta[4];
    tvec[2] = beta[5];
    return true;
  }

  // TODO: Add more comments. What are the anchors? This is just turning an image into a grid? what defines an anchor?
  function generateAnchors(width, height) {
    var anchors = [];
    var strides = [8, 16, 32];
    for (var s = 0; s < strides.length; ++s) {
      var stride = strides[s];
      var feature_w = Math.ceil(width / stride);
      var feature_h = Math.ceil(height / stride);
      for (var i = 0; i < feature_h; ++i) {
        for (var j = 0; j < feature_w; ++j) {
          anchors.push({
            cx: j * stride,
            cy: i * stride,
            stride_x: stride,
            stride_y: stride,
          });
        }
      }
    }
    return anchors;
  }

  // TODO: Document. What's NMS? Something something solver, I assume.
  function runNms(boxes, scores, landmarks, threshold) {
    var indices = [];
    for (var i = 0; i < scores.length; i++) {
      indices.push(i);
    }
    indices.sort(function (a, b) {
      return scores[b] - scores[a];
    });
    var keep = [];
    while (indices.length > 0) {
      var idx = indices.shift();
      keep.push(idx);
      var boxA = boxes[idx];
      var areaA = boxA.w * boxA.h;
      indices = indices.filter(function (j) {
        var boxB = boxes[j];
        var interX = Math.max(
          0,
          Math.min(boxA.x + boxA.w, boxB.x + boxB.w) - Math.max(boxA.x, boxB.x),
        );
        var interY = Math.max(
          0,
          Math.min(boxA.y + boxA.h, boxB.y + boxB.h) - Math.max(boxA.y, boxB.y),
        );
        var interArea = interX * interY;
        var unionArea = areaA + boxB.w * boxB.h - interArea;
        var iou = unionArea > 0 ? interArea / unionArea : 0;
        return iou < threshold;
      });
    }
    return keep.map(function (i) {
      return {
        box: boxes[i],
        score: scores[i],
        landmarks: landmarks[i],
      };
    });
  }

  var gazeTracker = {
    active: false,
    video: null,
    stream: null,
    detectorSession: null,
    gazeSession: null,
    yunetBytes: null,
    gazeBytes: null,
    desiredWidth: 640,
    desiredHeight: 480,
    faceDetectWidth: 640,
    faceDetectHeight: 640,
    cameraFocalLength: -1.0,

    setModels: function (hexYunet, hexGaze, focalLength) {
      console.log(
        "[GazeTracker] Received hex model bytes from Godot. Focal: " +
          focalLength,
      );
      if (focalLength) this.cameraFocalLength = focalLength;

      // face_detection_yunet_2023mar.ort expects exactly 640x640 input shape.
      this.faceDetectWidth = 640;
      this.faceDetectHeight = 640;

      // TODO: Can we do this operation on the other side of this transfer? So that someone else is responsible for shipping us model bytes in a sane format? It seems incredible anyone would find it more efficient to pack into hex...
      function hexToUint8Array(hexString) {
        if (!hexString) return new Uint8Array(0);
        var len = hexString.length;
        var bytes = new Uint8Array(len / 2);
        var charCodeMap = new Uint8Array(256);
        // Why are we writing this ourselves? There must be base64 encoding facilities in JS we can rely on?
        for (var i = 0; i < 10; i++) charCodeMap[48 + i] = i; // '0'-'9'
        for (var i = 0; i < 6; i++) {
          charCodeMap[97 + i] = 10 + i; // 'a'-'f'
          charCodeMap[65 + i] = 10 + i; // 'A'-'F'
        }
        for (var i = 0; i < len; i += 2) {
          var high = charCodeMap[hexString.charCodeAt(i)];
          var low = charCodeMap[hexString.charCodeAt(i + 1)];
          bytes[i >> 1] = (high << 4) | low;
        }
        return bytes;
      }

      this.yunetBytes = hexToUint8Array(hexYunet);
      this.gazeBytes = hexToUint8Array(hexGaze);
      console.log(
        "[GazeTracker] Converted hex to Uint8Arrays: YuNet size = " +
          this.yunetBytes.length +
          ", Gaze size = " +
          this.gazeBytes.length,
      );
    },

    // TODO: Can this also be done from our `gaze_tracker_web.cpp` server? Fetching & injecting the scripts it depends on during its startup?
    injectScript: function (url, onload, onerror) {
      console.log("[GazeTracker] Injecting ONNX Runtime script tag: " + url);
      var s = document.createElement("script");
      s.src = url;
      s.onload = function () {
        console.log("[GazeTracker] ONNX Runtime loaded successfully");
        onload();
      };
      s.onerror = function (err) {
        console.error(
          "[GazeTracker] Failed to load ONNX Runtime from " + url,
          err,
        );
        onerror(err);
      };
      document.head.appendChild(s);
    },

    startTracking: function (
      yunetPath,
      gazeOnnxPath,
      isDebug,
      cameraWidth,
      cameraHeight,
      debugThrottleInterval,
    ) {
      if (this.active) return;
      this.active = true;
      isDebug = !!isDebug;
      this.isDebugMode = isDebug;
      this.debugThrottleInterval = debugThrottleInterval || 1;
      this.desiredWidth = cameraWidth || 640;
      this.desiredHeight = cameraHeight || 480;

      var self = this;

      // TODO: Make this the gaze_tracker_web.cpp's responsibility, a method like `interceptLogs` or `setDebugMode` or something which it calls once during setup.
      if (isDebug && !this.logsIntercepted) {
        this.logsIntercepted = true;
        this.debugLogs = [];
        var originalLog = console.log;
        var originalError = console.error;
        console.log = function () {
          var args = Array.prototype.slice.call(arguments);
          originalLog.apply(console, args);
          var msg = args.join(" ");
          if (msg.includes("Gaze") || msg.includes("ONNX")) {
            self.debugLogs.push({
              time: new Date().toLocaleTimeString(),
              type: "info",
              message: msg,
            });
            if (self.debugLogs.length > 50) self.debugLogs.shift();
          }
        };
        console.error = function () {
          var args = Array.prototype.slice.call(arguments);
          originalError.apply(console, args);
          var msg = args.join(" ");
          self.debugLogs.push({
            time: new Date().toLocaleTimeString(),
            type: "error",
            message: msg,
          });
          if (self.debugLogs.length > 50) self.debugLogs.shift();
        };
      }

      console.log("[GazeTracker] Initializing sidecar tracking pipeline...");

      if (isDebug) {
        try {
          this.createDebugHUD(); // TODO: Still necessary? The xplat debugHUD should replace this?
        } catch (hudErr) {
          console.error("[GazeTracker] HUD error:", hudErr);
        }
      }

      // In test environment we may mock the ONNX Runtime
      if (typeof window.ort !== "undefined" || window.useMock) {
        setTimeout(proceed, 50);
      } else {
        this.injectScript(
          "https://cdn.jsdelivr.net/npm/onnxruntime-web/dist/ort.min.js", // TODO: Again, make the server manage these dependent scripts.
          proceed,
          function () {
            console.error(
              "[GazeTracker] Critical: failed to load onnxruntime-web",
            );
          },
        );
      }

      function proceed() {
        console.log(
          "[GazeTracker] ONNX Runtime library ready. Setting up sessions...",
        );
        if (window.godotGaze && window.godotGaze.on_ready) {
          window.godotGaze.on_ready(self);
        }
        self.setupPipeline();
      }
    },

    setupPipeline: async function () {
      var self = this;
      if (window.useMock) {
        console.log(
          "[GazeTracker] Running in Mock mode. Skipping real InferenceSession creation.",
        );
        this.setupVideoLoop();
        return;
      }

      try {
        this.detectorSession = await ort.InferenceSession.create(
          this.yunetBytes,
        );
        this.gazeSession = await ort.InferenceSession.create(this.gazeBytes);
        console.log(
          "[GazeTracker] ONNX Runtime InferenceSessions created successfully",
        );
      } catch (err) {
        console.error("[GazeTracker] Failed to create ONNX sessions:", err);
        return;
      }

      this.yunetBytes = null;
      this.gazeBytes = null;
      this.setupVideoLoop();
    },

    setupVideoLoop: function () {
      var self = this;
      this.video = document.createElement("video");
      this.video.width = self.desiredWidth;
      this.video.height = self.desiredHeight;
      this.video.autoplay = true;
      this.video.playsInline = true;
      this.video.muted = true;
      this.video.style.display = "none";
      document.body.appendChild(this.video);

      navigator.mediaDevices
        .getUserMedia({
          video: {
            width: self.desiredWidth,
            height: self.desiredHeight,
            facingMode: "user",
          },
        })
        .then(function (stream) {
          self.stream = stream;
          self.video.srcObject = stream;
          self.video.play();

          // Setup Canvas and context for video processing
          var mainCanvas = document.createElement("canvas");
          var mainCtx = mainCanvas.getContext("2d", {
            willReadFrequently: true,
          });
          var detectCanvas = document.createElement("canvas");
          detectCanvas.width = self.faceDetectWidth;
          detectCanvas.height = self.faceDetectHeight;
          var detectCtx = detectCanvas.getContext("2d", {
            willReadFrequently: true,
          });

          var leftEyeCanvas = document.createElement("canvas");
          leftEyeCanvas.width = 60;
          leftEyeCanvas.height = 60;
          var leftEyeCtx = leftEyeCanvas.getContext("2d", {
            willReadFrequently: true,
          });

          var rightEyeCanvas = document.createElement("canvas");
          rightEyeCanvas.width = 60;
          rightEyeCanvas.height = 60;
          var rightEyeCtx = rightEyeCanvas.getContext("2d", {
            willReadFrequently: true,
          });

          // TODO: Do we _need_ this? There isn't always a debug feed going...
          var debugFeedCanvas = document.createElement("canvas");
          debugFeedCanvas.width = 320;
          debugFeedCanvas.height = 240;
          var debugFeedCtx = debugFeedCanvas.getContext("2d", {
            willReadFrequently: true,
          });

          var anchors = generateAnchors(
            self.faceDetectWidth,
            self.faceDetectHeight,
          );

          function scheduleNextFrame() {
            if (!self.active) {
              self.cleanupPipeline();
              return;
            }
            setTimeout(trackFrame, 16);
          }

          async function trackFrame() {
            if (!self.active) return;
            if (
              self.video.paused ||
              self.video.ended ||
              !self.video.videoWidth
            ) {
              scheduleNextFrame();
              return;
            }

            var w = self.video.videoWidth;
            var h = self.video.videoHeight;
            mainCanvas.width = w;
            mainCanvas.height = h;

            mainCtx.drawImage(self.video, 0, 0, w, h);

            var detected = false;
            var landmarks = [];

            // TODO: This design is fragile and weird. If we want a partially mocked sidecar, we should be interacting with the sidecar script and setting/loading a partially mocked implementation. Writing mock-supporting code in between our prod code is unacceptable. Redesign this, removing it entirely would be better.
            if (window.useMock) {
              // In Mock environment we just return dummy coordinates
              detected = true;
              landmarks = [
                { x: 170, y: 110 }, // right eye
                { x: 150, y: 110 }, // left eye
                { x: 160, y: 120 }, // nose
                { x: 155, y: 130 }, // mouth right
                { x: 165, y: 130 }, // mouth left
              ];
            } else {
              // 1. Run Face Detector
              try {
                detectCtx.drawImage(
                  mainCanvas,
                  0,
                  0,
                  w,
                  h,
                  0,
                  0,
                  self.faceDetectWidth,
                  self.faceDetectHeight,
                );
                var imgData = detectCtx.getImageData(
                  0,
                  0,
                  self.faceDetectWidth,
                  self.faceDetectHeight,
                );
                var numPixels = self.faceDetectWidth * self.faceDetectHeight;
                var inputData = new Float32Array(numPixels * 3);

                // BGR CHW conversion
                for (var i = 0; i < numPixels; i++) {
                  var r = imgData.data[i * 4 + 0];
                  var g = imgData.data[i * 4 + 1];
                  var b = imgData.data[i * 4 + 2];
                  inputData[i] = b;
                  inputData[numPixels + i] = g;
                  inputData[2 * numPixels + i] = r;
                }

                var inputTensor = new ort.Tensor("float32", inputData, [
                  1,
                  3,
                  self.faceDetectHeight,
                  self.faceDetectWidth,
                ]);
                var feeds = {
                  [self.detectorSession.inputNames[0]]: inputTensor,
                };
                var results = await self.detectorSession.run(feeds);

                // Decode multi-stride outputs
                var candidate_bboxes = [];
                var candidate_scores = [];
                var candidate_landmarks = [];

                var anchorOffset = 0;
                var strides = [8, 16, 32];
                for (var s = 0; s < strides.length; s++) {
                  var stride = strides[s];
                  var clsName = self.detectorSession.outputNames[s];
                  var objName = self.detectorSession.outputNames[3 + s];
                  var bboxName = self.detectorSession.outputNames[6 + s];
                  var kpsName = self.detectorSession.outputNames[9 + s];

                  var clsData = results[clsName].data;
                  var objData = results[objName].data;
                  var bboxData = results[bboxName].data;
                  var kpsData = results[kpsName].data;

                  var numAnchorsS = clsData.length;
                  for (var idx = 0; idx < numAnchorsS; idx++) {
                    var clsScore = clsData[idx];
                    var objScore = objData[idx];
                    var score = clsScore * objScore;

                    if (score > 0.6) {
                      var globalIdx = anchorOffset + idx;
                      if (globalIdx >= anchors.length) continue;
                      var anchor = anchors[globalIdx];

                      var cx =
                        bboxData[idx * 4 + 0] * anchor.stride_x + anchor.cx;
                      var cy =
                        bboxData[idx * 4 + 1] * anchor.stride_y + anchor.cy;
                      var boxW =
                        Math.exp(bboxData[idx * 4 + 2]) * anchor.stride_x;
                      var boxH =
                        Math.exp(bboxData[idx * 4 + 3]) * anchor.stride_y;

                      var ldm = [];
                      for (var k = 0; k < 5; k++) {
                        ldm.push({
                          x:
                            kpsData[idx * 10 + k * 2 + 0] * anchor.stride_x +
                            anchor.cx,
                          y:
                            kpsData[idx * 10 + k * 2 + 1] * anchor.stride_y +
                            anchor.cy,
                        });
                      }

                      candidate_bboxes.push({
                        x: cx - boxW / 2,
                        y: cy - boxH / 2,
                        w: boxW,
                        h: boxH,
                      });
                      candidate_scores.push(score);
                      candidate_landmarks.push(ldm);
                    }
                  }
                  anchorOffset += numAnchorsS;
                }

                var detections = runNms(
                  candidate_bboxes,
                  candidate_scores,
                  candidate_landmarks,
                  0.3,
                );
                if (detections.length > 0) {
                  detected = true;
                  var det = detections[0];
                  var scaleX = w / self.faceDetectWidth;
                  var scaleY = h / self.faceDetectHeight;
                  var temp_ldm = [];
                  for (var i = 0; i < 5; i++) {
                    temp_ldm.push({
                      x: det.landmarks[i].x * scaleX,
                      y: det.landmarks[i].y * scaleY,
                    });
                  }
                  for (var i = 0; i < 5; i++) {
                    landmarks.push(temp_ldm[i]);
                  }
                }
              } catch (detErr) {
                console.error("[GazeTracker] Face detection failed:", detErr);
              }
            }

            // 2. Run Gaze Estimator
            if (detected && landmarks.length >= 5) {
              try {
                var model_points = [
                  [
                    -FaceModelGeometry.EYE_X,
                    FaceModelGeometry.EYE_Y,
                    FaceModelGeometry.EYE_Z,
                  ], // 0: Right Eye (Image Left)
                  [
                    FaceModelGeometry.EYE_X,
                    FaceModelGeometry.EYE_Y,
                    FaceModelGeometry.EYE_Z,
                  ], // 1: Left Eye (Image Right)
                  [
                    0.0,
                    FaceModelGeometry.DEFAULT_NOSE_Y,
                    FaceModelGeometry.DEFAULT_NOSE_Z,
                  ], // 2: Nose Tip
                  [
                    -FaceModelGeometry.MOUTH_X,
                    FaceModelGeometry.MOUTH_Y,
                    FaceModelGeometry.MOUTH_Z,
                  ], // 3: Right Mouth (Image Left)
                  [
                    FaceModelGeometry.MOUTH_X,
                    FaceModelGeometry.MOUTH_Y,
                    FaceModelGeometry.MOUTH_Z,
                  ], // 4: Left Mouth (Image Right)
                ];
                var image_points = [
                  [landmarks[0].x, landmarks[0].y],
                  [landmarks[1].x, landmarks[1].y],
                  [landmarks[2].x, landmarks[2].y],
                  [landmarks[3].x, landmarks[3].y],
                  [landmarks[4].x, landmarks[4].y],
                ];

                var cx = w / 2.0;
                var cy = h / 2.0;
                var fx =
                  self.cameraFocalLength > 0.0
                    ? self.cameraFocalLength
                    : w * 1.5625;

                var rvec = [0.0, 0.0, 0.0];
                var tvec = [0.0, 0.0, 700.0];

                var pnp_success = false;
                if (window.useMock) {
                  var imgName = window.currentTestImage || "";
                  var data =
                    window.cppTestData[imgName] ||
                    window.cppTestData["self_center.jpg"];
                  rvec = [data.rx, data.ry, data.rz];
                  tvec = [data.tx, data.ty, data.tz];
                  pnp_success = true;
                } else {
                  pnp_success = solvePnpLm(
                    model_points,
                    image_points,
                    fx,
                    fx,
                    cx,
                    cy,
                    rvec,
                    tvec,
                  );
                }

                if (pnp_success) {
                  var R = rodriguesToRotationMatrix(rvec);
                  var r00 = R[0][0];
                  var r10 = R[1][0];
                  var r20 = R[2][0];
                  var r01 = R[0][1];
                  var r11 = R[1][1];
                  var r21 = R[2][1];
                  var r02 = R[0][2];
                  var r12 = R[1][2];
                  var r22 = R[2][2];

                  var sy = Math.sqrt(r00 * r00 + r10 * r10);
                  var singular = sy < 1e-6;
                  var pitch = 0,
                    yaw = 0,
                    roll = 0;
                  if (!singular) {
                    pitch = Math.atan2(r21, r22) * (180.0 / Math.PI);
                    yaw = Math.atan2(-r20, sy) * (180.0 / Math.PI);
                    roll = Math.atan2(r10, r00) * (180.0 / Math.PI);
                  } else {
                    pitch = Math.atan2(-r12, r11) * (180.0 / Math.PI);
                    yaw = Math.atan2(-r20, sy) * (180.0 / Math.PI);
                    roll = 0;
                  }

                  var rx = rvec[0];
                  var ry = rvec[1];
                  var rz = rvec[2];
                  var tx = tvec[0];
                  var ty = tvec[1];
                  var tz = tvec[2];

                  var lex =
                    r00 * FaceModelGeometry.EYE_X +
                    r01 * FaceModelGeometry.EYE_Y +
                    r02 * FaceModelGeometry.EYE_Z +
                    tx;
                  var ley =
                    r10 * FaceModelGeometry.EYE_X +
                    r11 * FaceModelGeometry.EYE_Y +
                    r12 * FaceModelGeometry.EYE_Z +
                    ty;
                  var lez =
                    r20 * FaceModelGeometry.EYE_X +
                    r21 * FaceModelGeometry.EYE_Y +
                    r22 * FaceModelGeometry.EYE_Z +
                    tz;

                  var rex =
                    r00 * -FaceModelGeometry.EYE_X +
                    r01 * FaceModelGeometry.EYE_Y +
                    r02 * FaceModelGeometry.EYE_Z +
                    tx;
                  var rey =
                    r10 * -FaceModelGeometry.EYE_X +
                    r11 * FaceModelGeometry.EYE_Y +
                    r12 * FaceModelGeometry.EYE_Z +
                    ty;
                  var rez =
                    r20 * -FaceModelGeometry.EYE_X +
                    r21 * FaceModelGeometry.EYE_Y +
                    r22 * FaceModelGeometry.EYE_Z +
                    tz;

                  function cropEye(eyeCtx, eyeCanvas, isLeft) {
                    var eye_center = isLeft ? landmarks[1] : landmarks[0];
                    var roll_dx = landmarks[1].x - landmarks[0].x;
                    var roll_dy = landmarks[1].y - landmarks[0].y;
                    var angle =
                      Math.atan2(roll_dy, roll_dx) * (180.0 / Math.PI);
                    var dist_px = Math.sqrt(
                      roll_dx * roll_dx + roll_dy * roll_dy,
                    );
                    var scale = 70.0 / (dist_px > 1e-6 ? dist_px : 70.0);

                    eyeCtx.setTransform(1, 0, 0, 1, 0, 0); // reset
                    eyeCtx.fillStyle = "black";
                    eyeCtx.fillRect(0, 0, 60, 60);

                    eyeCtx.translate(30, 30);
                    eyeCtx.rotate((-angle * Math.PI) / 180.0);
                    eyeCtx.scale(scale, scale);
                    eyeCtx.translate(-eye_center.x, -eye_center.y);
                    eyeCtx.drawImage(mainCanvas, 0, 0);

                    // Extract BGR float32 CHW data
                    var eyeData = eyeCtx.getImageData(0, 0, 60, 60);
                    var tensorData = new Float32Array(60 * 60 * 3);
                    for (var i = 0; i < 3600; i++) {
                      var r = eyeData.data[i * 4 + 0];
                      var g = eyeData.data[i * 4 + 1];
                      var b = eyeData.data[i * 4 + 2];
                      var gray = 0.299 * r + 0.587 * g + 0.114 * b;
                      tensorData[i] = gray;
                      tensorData[3600 + i] = gray;
                      tensorData[7200 + i] = gray;
                    }
                    return tensorData;
                  }

                  var rightEyeTensor = cropEye(
                    rightEyeCtx,
                    rightEyeCanvas,
                    false,
                  ); // ADAS uses img-left (right eye) as left input
                  var leftEyeTensor = cropEye(leftEyeCtx, leftEyeCanvas, true); // ADAS uses img-right (left eye) as right input

                  var dx = 0,
                    dy = 0,
                    dz = -1;

                  if (window.useMock) {
                    var imgName = window.currentTestImage || "";
                    var data =
                      window.cppTestData[imgName] ||
                      window.cppTestData["self_center.jpg"];
                    dx = data.gx;
                    dy = data.gy;
                    dz = data.gz;
                  } else {
                    var leftEyeT = new ort.Tensor(
                      "float32",
                      rightEyeTensor,
                      [1, 3, 60, 60],
                    );
                    var rightEyeT = new ort.Tensor(
                      "float32",
                      leftEyeTensor,
                      [1, 3, 60, 60],
                    );
                    var headPoseT = new ort.Tensor(
                      "float32",
                      new Float32Array([-yaw, pitch, -roll]),
                      [1, 3],
                    );

                    var gazeFeeds = {
                      [self.gazeSession.inputNames[0]]: leftEyeT,
                      [self.gazeSession.inputNames[1]]: rightEyeT,
                      [self.gazeSession.inputNames[2]]: headPoseT,
                    };
                    var gazeResults = await self.gazeSession.run(gazeFeeds);
                    var gazeOut =
                      gazeResults[self.gazeSession.outputNames[0]].data;

                    if (gazeOut.length === 2) {
                      var pitch_gaze = gazeOut[0];
                      var yaw_gaze = gazeOut[1];
                      var cos_pitch = Math.cos(pitch_gaze);
                      dx = Math.sin(yaw_gaze) * cos_pitch;
                      dy = Math.sin(pitch_gaze);
                      dz = Math.cos(yaw_gaze) * cos_pitch;
                    } else if (gazeOut.length === 3) {
                      dx = gazeOut[0];
                      dy = gazeOut[1];
                      dz = gazeOut[2];
                    }
                    var len_gaze = Math.hypot(dx, dy, dz);
                    if (len_gaze > 0) {
                      dx /= len_gaze;
                      dy /= len_gaze;
                      dz /= len_gaze;
                    }
                  }

                  var dx_cv = dx;
                  var dy_cv = -dy;
                  var dz_cv = dz;

                  var coords = self.getCanvasScreenCoordinates();
                  var canvasX = coords.x;
                  var canvasY = coords.y;

                  // Update visual HUD debug panel
                  var debugPanel = document.getElementById("gaze-hud-panel");
                  if (debugPanel && debugPanel.style.display !== "none") {
                    var hudCanvas = document.getElementById("gaze-hud-canvas");
                    if (hudCanvas) {
                      var hudCtx = hudCanvas.getContext("2d");
                      hudCanvas.width = w;
                      hudCanvas.height = h;
                      hudCtx.drawImage(mainCanvas, 0, 0);

                      // Draw face landmarks
                      hudCtx.fillStyle = "#00ffcc";
                      for (var i = 0; i < landmarks.length; i++) {
                        hudCtx.beginPath();
                        hudCtx.arc(
                          landmarks[i].x,
                          landmarks[i].y,
                          4,
                          0,
                          2 * Math.PI,
                        );
                        hudCtx.fill();
                      }

                      // Draw gaze line from mid point of eyes
                      var eye_mid_x = (landmarks[0].x + landmarks[1].x) / 2;
                      var eye_mid_y = (landmarks[0].y + landmarks[1].y) / 2;
                      hudCtx.strokeStyle = "#ff3366";
                      hudCtx.lineWidth = 3;
                      hudCtx.beginPath();
                      hudCtx.moveTo(eye_mid_x, eye_mid_y);
                      hudCtx.lineTo(
                        eye_mid_x + dx_cv * 120,
                        eye_mid_y + dy_cv * 120,
                      );
                      hudCtx.stroke();
                    }

                    // Display crops on HUD
                    var lEyeCanvas = document.getElementById(
                      "gaze-hud-left-eye-canvas",
                    );
                    if (lEyeCanvas) {
                      var lCtx = lEyeCanvas.getContext("2d");
                      lCtx.drawImage(rightEyeCanvas, 0, 0);
                    }
                    var rEyeCanvas = document.getElementById(
                      "gaze-hud-right-eye-canvas",
                    );
                    if (rEyeCanvas) {
                      var rCtx = rEyeCanvas.getContext("2d");
                      rCtx.drawImage(leftEyeCanvas, 0, 0);
                    }

                    var metricsEl = document.getElementById("gaze-hud-metrics");
                    if (metricsEl) {
                      self.lastMetrics = {
                        faceDetected: true,
                        gazeVector: { x: dx_cv, y: dy_cv, z: dz_cv },
                        headPose: {
                          tx: tx,
                          ty: ty,
                          tz: tz,
                          rx: rx,
                          ry: ry,
                          rz: rz,
                        },
                        canvasX: canvasX,
                        canvasY: canvasY,
                      };
                      metricsEl.innerHTML =
                        '<div>Face Tracked:</div><div style="color:#00ffcc">true</div>' +
                        "<div>Gaze Vector:</div><div>(" +
                        dx_cv.toFixed(3) +
                        ", " +
                        dy_cv.toFixed(3) +
                        ", " +
                        dz_cv.toFixed(3) +
                        ")</div>" +
                        "<div>Head Pos:</div><div>(" +
                        tx.toFixed(1) +
                        ", " +
                        ty.toFixed(1) +
                        ", " +
                        tz.toFixed(1) +
                        ")</div>" +
                        "<div>Canvas Pos:</div><div>(" +
                        canvasX +
                        ", " +
                        canvasY +
                        ")</div>" +
                        "<div>Camera Res:</div><div>" +
                        self.video.videoWidth +
                        "x" +
                        self.video.videoHeight +
                        "</div>";
                    }
                  }

                  if (window.godotGaze && window.godotGaze.feed_gaze) {
                    var sendDebugThisFrame = false;
                    var isGodotDebugActive = !!(window.godotGaze && (window.godotGaze.previewRequested || window.godotGaze.cropRequested));
                    if (self.isDebugMode || isGodotDebugActive) {
                      self.debugFrameCounter =
                        (self.debugFrameCounter || 0) + 1;
                      if (
                        self.debugFrameCounter % self.debugThrottleInterval ===
                        0
                      ) {
                        sendDebugThisFrame = true;
                      }
                    }
                    if (sendDebugThisFrame) {
                      debugFeedCtx.drawImage(
                        mainCanvas,
                        0,
                        0,
                        w,
                        h,
                        0,
                        0,
                        320,
                        240,
                      );
                      var feedData = debugFeedCtx.getImageData(
                        0,
                        0,
                        320,
                        240,
                      ).data;
                      var lEyeData = leftEyeCtx.getImageData(0, 0, 60, 60).data;
                      var rEyeData = rightEyeCtx.getImageData(
                        0,
                        0,
                        60,
                        60,
                      ).data;
                      window.godotGaze.feed_gaze(
                        true,
                        lex,
                        ley,
                        lez,
                        rex,
                        rey,
                        rez,
                        dx_cv,
                        dy_cv,
                        dz_cv,
                        tx,
                        ty,
                        tz,
                        rx,
                        ry,
                        rz,
                        canvasX,
                        canvasY,
                        true,
                        feedData,
                        lEyeData,
                        rEyeData,
                      );
                    } else {
                      window.godotGaze.feed_gaze(
                        true,
                        lex,
                        ley,
                        lez,
                        rex,
                        rey,
                        rez,
                        dx_cv,
                        dy_cv,
                        dz_cv,
                        tx,
                        ty,
                        tz,
                        rx,
                        ry,
                        rz,
                        canvasX,
                        canvasY,
                        false,
                        new Uint8Array(0),
                        new Uint8Array(0),
                        new Uint8Array(0),
                      );
                    }
                  }
                }
              } catch (gazeErr) {
                console.error("[GazeTracker] Gaze estimation failed:", gazeErr);
              }
            } else {
              // Face lost handling
              var debugPanel = document.getElementById("gaze-hud-panel");
              if (debugPanel && debugPanel.style.display !== "none") {
                var hudCanvas = document.getElementById("gaze-hud-canvas");
                if (hudCanvas) {
                  var hudCtx = hudCanvas.getContext("2d");
                  hudCanvas.width = w;
                  hudCanvas.height = h;
                  hudCtx.drawImage(mainCanvas, 0, 0);
                }
                var metricsEl = document.getElementById("gaze-hud-metrics");
                if (metricsEl) {
                  metricsEl.innerHTML =
                    '<div>Face Tracked:</div><div style="color:#ff3366">false</div>';
                }
              }
              if (window.godotGaze && window.godotGaze.feed_gaze) {
                window.godotGaze.feed_gaze(
                  false,
                  0,
                  0,
                  0,
                  0,
                  0,
                  0,
                  0,
                  0,
                  -1,
                  0,
                  0,
                  700,
                  0,
                  0,
                  0,
                  0,
                  0,
                  false,
                  new Uint8Array(0),
                  new Uint8Array(0),
                  new Uint8Array(0),
                );
              }
            }

            scheduleNextFrame();
          }

          trackFrame();
        })
        .catch(function (err) {
          console.error("[GazeTracker] Camera initialization failed:", err);
        });
    },

    getCanvasScreenCoordinates: function () {
      var canvas =
        document.getElementById("canvas") ||
        document.querySelector("canvas") ||
        this.canvas;
      var rect = canvas ? canvas.getBoundingClientRect() : { left: 0, top: 0 };

      var isFullscreen = !!(
        document.fullscreenElement ||
        document.webkitFullscreenElement ||
        document.mozFullScreenElement ||
        document.msFullscreenElement ||
        (window.innerWidth &&
          window.screen &&
          window.innerWidth === window.screen.width &&
          window.innerHeight === window.screen.height)
      );
      if (isFullscreen) {
        return { x: 0, y: 0 };
      }

      var winLeft = 0;
      var winTop = 0;
      try {
        winLeft =
          window.screenLeft !== undefined ? window.screenLeft : window.screenX;
        winTop =
          window.screenTop !== undefined ? window.screenTop : window.screenY;
      } catch (secErr) {
        winLeft = 0;
        winTop = 0;
      }

      if (winLeft === undefined || isNaN(winLeft)) winLeft = 0;
      if (winTop === undefined || isNaN(winTop)) winTop = 0;

      if (this._borderOffset === undefined) {
        this._borderOffset = 0;
        this._chromeOffset = 0;
        try {
          if (window.outerWidth && window.innerWidth) {
            this._borderOffset = Math.max(
              0,
              (window.outerWidth - window.innerWidth) / 2,
            );
          }
          if (window.outerHeight && window.innerHeight) {
            this._chromeOffset = Math.max(
              0,
              window.outerHeight - window.innerHeight - this._borderOffset,
            );
          }
        } catch (e) {}
      }
      var dpr = window.devicePixelRatio || 1.0;
      return {
        x: (winLeft + this._borderOffset + rect.left) * dpr,
        y: (winTop + this._chromeOffset + rect.top) * dpr,
      };
    },

    createDebugHUD: function () {
      if (document.getElementById("gaze-hud-trigger")) return;

      var trigger = document.createElement("button");
      trigger.id = "gaze-hud-trigger";
      trigger.innerHTML = "👁️ Gaze HUD";
      Object.assign(trigger.style, {
        position: "fixed",
        bottom: "20px",
        right: "20px",
        zIndex: "999999",
        padding: "10px 16px",
        background: "rgba(18, 18, 24, 0.85)",
        color: "#00ffcc",
        border: "1px solid rgba(0, 255, 204, 0.3)",
        borderRadius: "20px",
        cursor: "pointer",
        fontFamily: "system-ui, -apple-system, sans-serif",
        fontSize: "13px",
        fontWeight: "600",
        boxShadow: "0 4px 20px rgba(0,0,0,0.3)",
        backdropFilter: "blur(8px)",
        webkitBackdropFilter: "blur(8px)",
        transition: "all 0.2s ease-in-out",
      });

      var panel = document.createElement("div");
      panel.id = "gaze-hud-panel";
      Object.assign(panel.style, {
        position: "fixed",
        bottom: "80px",
        right: "20px",
        width: "340px",
        maxHeight: "520px",
        zIndex: "999998",
        background: "rgba(18, 18, 24, 0.9)",
        border: "1px solid rgba(255, 255, 255, 0.1)",
        borderRadius: "12px",
        boxShadow: "0 8px 32px rgba(0, 0, 0, 0.4)",
        backdropFilter: "blur(16px)",
        webkitBackdropFilter: "blur(16px)",
        display: "none",
        flexDirection: "column",
        color: "#e2e8f0",
        fontFamily: "system-ui, -apple-system, sans-serif",
        overflow: "hidden",
      });

      var header = document.createElement("div");
      Object.assign(header.style, {
        padding: "12px 16px",
        borderBottom: "1px solid rgba(255, 255, 255, 0.08)",
        display: "flex",
        justifyContent: "space-between",
        alignItems: "center",
        background: "rgba(255, 255, 255, 0.02)",
      });
      header.innerHTML =
        '<span style="font-weight:700; font-size:12px; letter-spacing:0.5px; color:#00ffcc;">GAZE DIAGNOSTICS</span>';

      var closeBtn = document.createElement("button");
      closeBtn.innerHTML = "✕";
      Object.assign(closeBtn.style, {
        background: "none",
        border: "none",
        color: "#a0aec0",
        cursor: "pointer",
        fontSize: "14px",
        padding: "0",
      });
      closeBtn.onclick = function () {
        panel.style.display = "none";
      };
      header.appendChild(closeBtn);
      panel.appendChild(header);

      var body = document.createElement("div");
      Object.assign(body.style, {
        padding: "12px",
        overflowY: "auto",
        display: "flex",
        flexDirection: "column",
        gap: "10px",
      });

      var canvas = document.createElement("canvas");
      canvas.id = "gaze-hud-canvas";
      canvas.width = 320;
      canvas.height = 240;
      Object.assign(canvas.style, {
        width: "100%",
        borderRadius: "6px",
        background: "#0a0a0f",
        border: "1px solid rgba(255, 255, 255, 0.05)",
      });
      body.appendChild(canvas);

      var eyesContainer = document.createElement("div");
      Object.assign(eyesContainer.style, {
        display: "flex",
        justifyContent: "space-around",
        gap: "10px",
        margin: "4px 0",
      });

      var leftEyeBox = document.createElement("div");
      Object.assign(leftEyeBox.style, {
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        gap: "4px",
      });
      leftEyeBox.innerHTML =
        '<span style="font-size: 10px; color: #a0aec0; font-family: system-ui;">Left Eye (Img Right)</span>';
      var leftEyeCanvas = document.createElement("canvas");
      leftEyeCanvas.id = "gaze-hud-left-eye-canvas";
      leftEyeCanvas.width = 60;
      leftEyeCanvas.height = 60;
      Object.assign(leftEyeCanvas.style, {
        width: "60px",
        height: "60px",
        borderRadius: "4px",
        background: "#0a0a0f",
        border: "1px solid rgba(255, 255, 255, 0.1)",
      });
      leftEyeBox.appendChild(leftEyeCanvas);

      var rightEyeBox = document.createElement("div");
      Object.assign(rightEyeBox.style, {
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        gap: "4px",
      });
      rightEyeBox.innerHTML =
        '<span style="font-size: 10px; color: #a0aec0; font-family: system-ui;">Right Eye (Img Left)</span>';
      var rightEyeCanvas = document.createElement("canvas");
      rightEyeCanvas.id = "gaze-hud-right-eye-canvas";
      rightEyeCanvas.width = 60;
      rightEyeCanvas.height = 60;
      Object.assign(rightEyeCanvas.style, {
        width: "60px",
        height: "60px",
        borderRadius: "4px",
        background: "#0a0a0f",
        border: "1px solid rgba(255, 255, 255, 0.1)",
      });
      rightEyeBox.appendChild(rightEyeCanvas);

      eyesContainer.appendChild(rightEyeBox);
      eyesContainer.appendChild(leftEyeBox);
      body.appendChild(eyesContainer);

      var metrics = document.createElement("div");
      metrics.id = "gaze-hud-metrics";
      Object.assign(metrics.style, {
        fontSize: "11px",
        fontFamily: "monospace",
        display: "grid",
        gridTemplateColumns: "1fr 1.5fr",
        gap: "4px 8px",
        padding: "8px",
        background: "rgba(0,0,0,0.25)",
        borderRadius: "6px",
        border: "1px solid rgba(255, 255, 255, 0.05)",
        color: "#cbd5e0",
      });
      body.appendChild(metrics);

      var copyBtn = document.createElement("button");
      copyBtn.innerHTML = "📋 Copy Debug Logs & Info";
      Object.assign(copyBtn.style, {
        padding: "8px 10px",
        background: "rgba(0, 255, 204, 0.1)",
        border: "1px solid rgba(0, 255, 204, 0.3)",
        color: "#00ffcc",
        borderRadius: "6px",
        cursor: "pointer",
        fontSize: "11px",
        fontWeight: "600",
        transition: "all 0.2s ease",
      });
      copyBtn.onmouseenter = function () {
        copyBtn.style.background = "rgba(0, 255, 204, 0.2)";
      };
      copyBtn.onmouseleave = function () {
        copyBtn.style.background = "rgba(0, 255, 204, 0.1)";
      };

      var self = this;
      copyBtn.onclick = function () {
        var info = {
          timestamp: new Date().toISOString(),
          active: self.active,
          faceDetected: self.lastMetrics
            ? self.lastMetrics.faceDetected
            : false,
          gazeVector: self.lastMetrics ? self.lastMetrics.gazeVector : null,
          headPose: self.lastMetrics ? self.lastMetrics.headPose : null,
          geometry: {
            screenLeft: window.screenLeft,
            screenTop: window.screenTop,
            outerWidth: window.outerWidth,
            outerHeight: window.outerHeight,
            innerWidth: window.innerWidth,
            innerHeight: window.innerHeight,
            canvasX: self.lastMetrics ? self.lastMetrics.canvasX : null,
            canvasY: self.lastMetrics ? self.lastMetrics.canvasY : null,
            borderOffset: self._borderOffset,
            chromeOffset: self._chromeOffset,
          },
          userAgent: navigator.userAgent,
          logs: self.debugLogs || [],
        };
        navigator.clipboard
          .writeText(JSON.stringify(info, null, 2))
          .then(function () {
            copyBtn.innerHTML = "✅ Copied!";
            setTimeout(function () {
              copyBtn.innerHTML = "📋 Copy Debug Logs & Info";
            }, 2000);
          });
      };
      body.appendChild(copyBtn);

      panel.appendChild(body);
      document.body.appendChild(trigger);
      document.body.appendChild(panel);

      trigger.onclick = function () {
        if (panel.style.display === "none") {
          panel.style.display = "flex";
        } else {
          panel.style.display = "none";
        }
      };

      trigger.onmouseenter = function () {
        trigger.style.background = "#00ffcc";
        trigger.style.color = "#121214";
      };
      trigger.onmouseleave = function () {
        trigger.style.background = "rgba(18, 18, 24, 0.85)";
        trigger.style.color = "#00ffcc";
      };
    },

    cleanupPipeline: function () {
      if (this.stream) {
        this.stream.getTracks().forEach(function (track) {
          track.stop();
        });
      }
      if (this.video) {
        this.video.remove();
      }
      var trigger = document.getElementById("gaze-hud-trigger");
      if (trigger) trigger.remove();
      var panel = document.getElementById("gaze-hud-panel");
      if (panel) panel.remove();

      if (this.detectorSession) this.detectorSession = null;
      if (this.gazeSession) this.gazeSession = null;

      this.stream = null;
      this.video = null;
      console.log(
        "[GazeTracker] Web tracking loop stopped and resources cleaned up.",
      );
    },

    stopTracking: function () {
      this.active = false;
    },
  };

  window.gazeTracker = gazeTracker;
})();
