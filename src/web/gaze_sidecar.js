// gaze_sidecar.js
// Standalone JavaScript code for the Godot Gaze sidecar tracking loop.
// Design B: Uses native cv.FaceDetectorYN from custom compiled opencv.js.

(function() {
    if (window.gazeTracker) return;

    var FaceModelGeometry = {
        EYE_X: 30.0,
        EYE_Y: -28.676,
        EYE_Z: 0.0,
        DEFAULT_NOSE_Y: -0.5,
        DEFAULT_NOSE_Z: -52.0,
        MOUTH_X: 18.462,
        MOUTH_Y: 31.712,
        MOUTH_Z: -4.550
    };

    var gazeTracker = {
        active: false,
        video: null,
        stream: null,
        detector: null,
        gazeNet: null,
        frameMat: null,
        bgrMat: null,
        grayMat: null,
        facesMat: null,
        yunetBytes: null,
        gazeBytes: null,

        // Configurations sent dynamically from Godot
        faceDetectWidth: 160,
        faceDetectHeight: 128,

        setModels: function(hexYunet, hexGaze, fdWidth, fdHeight) {
            console.log('[GazeTracker] Received hex model bytes from Godot. Configured detection size: ' + fdWidth + 'x' + fdHeight);
            if (fdWidth) this.faceDetectWidth = fdWidth;
            if (fdHeight) this.faceDetectHeight = fdHeight;

            function hexToUint8Array(hexString) {
                if (!hexString) return new Uint8Array(0);
                var len = hexString.length;
                var bytes = new Uint8Array(len / 2);
                
                // Precompute hex-to-int mapping
                var charCodeMap = new Uint8Array(256);
                for (var i = 0; i < 10; i++) charCodeMap[48 + i] = i;       // '0'-'9'
                for (var i = 0; i < 6; i++) {
                    charCodeMap[97 + i] = 10 + i;  // 'a'-'f'
                    charCodeMap[65 + i] = 10 + i;  // 'A'-'F'
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
            console.log('[GazeTracker] Converted hex to Uint8Arrays: YuNet size = ' + this.yunetBytes.length + ', Gaze size = ' + this.gazeBytes.length);
        },

        // 1. Script injection helper (maintains page origin for WebAssembly pthreads workers)
        injectScript: function(localUrl, fallbackUrl, onload, onerror) {
            console.log('[GazeTracker] Injecting local script tag: ' + localUrl);
            var s = document.createElement('script');
            s.src = localUrl;
            s.onload = function() {
                console.log('[GazeTracker] Local script loaded successfully: ' + localUrl);
                onload();
            };
            s.onerror = function(err) {
                console.warn('[GazeTracker] Local script load failed for ' + localUrl + '. Trying CDN fallback...');
                var sCDN = document.createElement('script');
                sCDN.crossOrigin = 'anonymous';
                sCDN.src = fallbackUrl;
                sCDN.onload = function() {
                    console.log('[GazeTracker] CDN fallback script loaded successfully: ' + fallbackUrl);
                    onload();
                };
                sCDN.onerror = function(cdnErr) {
                    console.error('[GazeTracker] CDN fallback failed for: ' + fallbackUrl, cdnErr);
                    onerror(cdnErr);
                };
                document.head.appendChild(sCDN);
            };
            document.head.appendChild(s);
        },

        // 2. Main initializer and loop
        startTracking: function(yunetPath, gazeOnnxPath, isDebug, cameraWidth, cameraHeight) {
            if (this.active) return;
            this.active = true;
            isDebug = !!isDebug;
            this.desiredWidth = cameraWidth || 640;
            this.desiredHeight = cameraHeight || 480;

            var self = this;
            
            // Intercept console logs for debug panel
            if (isDebug && !this.logsIntercepted) {
                this.logsIntercepted = true;
                this.debugLogs = [];
                var originalLog = console.log;
                var originalError = console.error;
                console.log = function() {
                    var args = Array.prototype.slice.call(arguments);
                    originalLog.apply(console, args);
                    var msg = args.join(' ');
                    if (msg.includes('Gaze') || msg.includes('OpenCV')) {
                        self.debugLogs.push({ time: new Date().toLocaleTimeString(), type: 'info', message: msg });
                        if (self.debugLogs.length > 50) self.debugLogs.shift();
                    }
                };
                console.error = function() {
                    var args = Array.prototype.slice.call(arguments);
                    originalError.apply(console, args);
                    var msg = args.join(' ');
                    self.debugLogs.push({ time: new Date().toLocaleTimeString(), type: 'error', message: msg });
                    if (self.debugLogs.length > 50) self.debugLogs.shift();
                };
            }

            console.log('[GazeTracker] Initializing sidecar tracking pipeline...');
            
            if (isDebug) {
                try {
                    this.createDebugHUD();
                } catch (hudErr) {
                    console.error('[GazeTracker] Failed to create debug HUD:', hudErr);
                }
            }

            this.injectScript('opencv.js', 'https://cdn.jsdelivr.net/npm/@techstark/opencv-js@4.9.0-release.2/dist/opencv.js', function() {
                console.log('[GazeTracker] OpenCV.js loaded. Waiting for Runtime...');
                var checkInitialized = setInterval(function() {
                    if (typeof cv !== 'undefined') {
                        clearInterval(checkInitialized);
                        if (typeof cv.then === 'function') {
                            cv.then(function(instance) {
                                window.cv = instance;
                                proceed();
                            });
                        } else {
                            proceed();
                        }
                    }

                    function proceed() {
                        console.log('[GazeTracker] OpenCV.js Runtime initialized. Setting up networks...');
                        if (window.godotGaze && window.godotGaze.on_ready) {
                            console.log('[GazeTracker] Requesting model bytes from Godot...');
                            window.godotGaze.on_ready(self);
                        }
                        self.setupPipeline(yunetPath, gazeOnnxPath);
                    }
                }, 100);
            }, function(err) {
                console.error('[GazeTracker] Critical: failed to load opencv.js');
            });
        },

        setupPipeline: function(yunetPath, gazeOnnxPath) {
            var self = this;
            console.log('[GazeTracker] Writing models directly to cv.FS...');
            if (typeof cv.FS === 'undefined') {
                console.error('[GazeTracker] cv.FS is undefined! Cannot write to OpenCV VFS.');
                return;
            }

            function writeModelToCvFS(data, destPath) {
                try {
                    if (!data || data.length === 0) {
                        throw new Error("Model bytes not found on sidecar object.");
                    }
                    var parts = destPath.split('/');
                    var currentDir = '';
                    for (var i = 0; i < parts.length - 1; i++) {
                        if (parts[i] === '') continue;
                        currentDir += '/' + parts[i];
                        try { cv.FS.mkdir(currentDir); } catch(e) {}
                    }
                    cv.FS.writeFile(destPath, data);
                    console.log('[GazeTracker] Wrote model to cv.FS:', destPath, 'size:', data.length);
                } catch (e) {
                    console.error('[GazeTracker] Failed to write model to cv.FS:', destPath, e);
                }
            }

            var cvYunetPath = '/models/face_detection_yunet_2023mar.onnx';
            var cvGazeOnnxPath = '/models/gaze-estimation-adas-0002.onnx';
            writeModelToCvFS(this.yunetBytes, cvYunetPath);
            writeModelToCvFS(this.gazeBytes, cvGazeOnnxPath);

            // Clean up properties to free memory
            this.yunetBytes = null;
            this.gazeBytes = null;
            console.log('[GazeTracker] Cleaned up temporary model byte arrays.');

            try {
                // Design B: Use cv.FaceDetectorYN exclusively with the dynamically configured faceDetectWidth and faceDetectHeight
                var detectSize = { width: this.faceDetectWidth, height: this.faceDetectHeight };
                this.detector = new cv.FaceDetectorYN(cvYunetPath, '', detectSize, 0.6, 0.3, 5000);
                this.gazeNet = cv.readNet(cvGazeOnnxPath);
                console.log('[GazeTracker] OpenCV FaceDetectorYN and GazeNet loaded successfully at resolution: ' + this.faceDetectWidth + 'x' + this.faceDetectHeight);
            } catch (err) {
                var msg = err;
                if (typeof err === 'number' && typeof cv !== 'undefined' && cv.exceptionFromPtr) {
                    try { msg = cv.exceptionFromPtr(err).msg; } catch(e) {}
                } else if (err && err.message) {
                    msg = err.message;
                }
                console.error('[GazeTracker] Failed to load networks via OpenCV.js:', msg);
                return;
            }

            this.video = document.createElement('video');
            this.video.width = self.desiredWidth;
            this.video.height = self.desiredHeight;
            this.video.autoplay = true;
            this.video.playsInline = true;
            this.video.muted = true;
            this.video.style.display = 'none';
            document.body.appendChild(this.video);

            navigator.mediaDevices.getUserMedia({
                video: { width: self.desiredWidth, height: self.desiredHeight, facingMode: 'user' }
            }).then(function(stream) {
                self.stream = stream;
                self.video.srcObject = stream;
                self.video.play();

                var cap = new cv.VideoCapture(self.video);
                var detectSize = { width: self.faceDetectWidth, height: self.faceDetectHeight };

                function scheduleNextFrame() {
                    var frameCalled = false;
                    function triggerNext() {
                        if (frameCalled) return;
                        frameCalled = true;
                        trackFrame();
                    }
                    requestAnimationFrame(triggerNext);
                    setTimeout(triggerNext, 50);
                }

                function trackFrame() {
                    if (!self.active) {
                        self.cleanupPipeline();
                        return;
                    }
                    // Fix Video Metadata Race Condition
                    if (self.video.paused || self.video.ended || !self.video.videoWidth) {
                        scheduleNextFrame();
                        return;
                    }

                    if (!self.frameMat) {
                        var w = self.video.videoWidth;
                        var h = self.video.videoHeight;
                        console.log('[GazeTracker] Initializing Mats with actual video resolution: ' + w + 'x' + h);
                        self.frameMat = new cv.Mat(h, w, cv.CV_8UC4);
                        self.bgrMat = new cv.Mat();
                        self.grayMat = new cv.Mat();
                        self.facesMat = new cv.Mat();
                        self.detectMat = new cv.Mat();
                    }

                    try {
                        cap.read(self.frameMat);
                        cv.cvtColor(self.frameMat, self.bgrMat, cv.COLOR_RGBA2BGR);
                        cv.cvtColor(self.bgrMat, self.grayMat, cv.COLOR_BGR2GRAY);
                    } catch (readErr) {
                        console.error('[GazeTracker] Camera frame read/convert failed:', readErr.message || readErr);
                        scheduleNextFrame();
                        return;
                    }

                    var detected = false;
                    var landmarks = [];

                    // 1. Face detection step
                    try {
                        // Resize input image to the configured detection resolution (e.g. 160x128)
                        cv.resize(self.bgrMat, self.detectMat, detectSize, 0, 0, cv.INTER_LINEAR);
                        self.detector.setInputSize(detectSize);
                        self.detector.detect(self.detectMat, self.facesMat);
                        
                        if (self.facesMat.rows > 0) {
                            detected = true;
                            // Scale landmarks back to the actual frame buffer space
                            var scaleX = self.frameMat.cols / self.faceDetectWidth;
                            var scaleY = self.frameMat.rows / self.faceDetectHeight;
                            
                            // YuNet landmarks layout: right eye (4,5), left eye (6,7), nose (8,9), right mouth (10,11), left mouth (12,13)
                            for (var i = 0; i < 5; i++) {
                                landmarks.push({ 
                                    x: self.facesMat.data32F[4 + 2 * i] * scaleX, 
                                    y: self.facesMat.data32F[5 + 2 * i] * scaleY 
                                });
                            }
                        }
                    } catch (detErr) {
                        var msg = detErr;
                        if (typeof detErr === 'number' && typeof cv !== 'undefined' && cv.exceptionFromPtr) {
                            try { msg = cv.exceptionFromPtr(detErr).msg; } catch(e) {}
                        } else if (detErr && detErr.message) {
                            msg = detErr.message;
                        }
                        console.error('[GazeTracker] Face detection failed:', msg);
                    }

                    // 2. Gaze estimation step
                    if (detected && landmarks.length >= 5) {
                        try {
                            var model_points = cv.matFromArray(5, 3, cv.CV_32F, [
                                -FaceModelGeometry.EYE_X, FaceModelGeometry.EYE_Y, FaceModelGeometry.EYE_Z,
                                FaceModelGeometry.EYE_X, FaceModelGeometry.EYE_Y, FaceModelGeometry.EYE_Z,
                                0.0, FaceModelGeometry.DEFAULT_NOSE_Y, FaceModelGeometry.DEFAULT_NOSE_Z,
                                -FaceModelGeometry.MOUTH_X, FaceModelGeometry.MOUTH_Y, FaceModelGeometry.MOUTH_Z,
                                FaceModelGeometry.MOUTH_X, FaceModelGeometry.MOUTH_Y, FaceModelGeometry.MOUTH_Z
                            ]);
                            var image_points = cv.matFromArray(5, 2, cv.CV_32F, [
                                landmarks[0].x, landmarks[0].y,
                                landmarks[1].x, landmarks[1].y,
                                landmarks[2].x, landmarks[2].y,
                                landmarks[3].x, landmarks[3].y,
                                landmarks[4].x, landmarks[4].y
                            ]);
                            var cx = self.frameMat.cols / 2.0;
                            var cy = self.frameMat.rows / 2.0;
                            var fx = self.frameMat.cols * 1.5625;
                            var camera_matrix = cv.matFromArray(3, 3, cv.CV_64F, [
                                fx, 0.0, cx,
                                0.0, fx, cy,
                                0.0, 0.0, 1.0
                            ]);
                            var dist_coeffs = cv.Mat.zeros(4, 1, cv.CV_64F);
                            var rvec = cv.matFromArray(3, 1, cv.CV_64F, [0.0, 0.0, 0.0]);
                            var tvec = cv.matFromArray(3, 1, cv.CV_64F, [0.0, 0.0, 700.0]);
                            var pnp_success = cv.solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rvec, tvec, true, cv.SOLVEPNP_ITERATIVE);

                            if (pnp_success) {
                                var R = new cv.Mat();
                                cv.Rodrigues(rvec, R);
                                var r00 = R.doubleAt(0, 0); var r10 = R.doubleAt(1, 0); var r20 = R.doubleAt(2, 0);
                                var r01 = R.doubleAt(0, 1); var r11 = R.doubleAt(1, 1); var r21 = R.doubleAt(2, 1);
                                var r02 = R.doubleAt(0, 2); var r12 = R.doubleAt(1, 2); var r22 = R.doubleAt(2, 2);
                                var sy = Math.sqrt(r00 * r00 + r10 * r10);
                                var singular = sy < 1e-6;
                                var pitch = 0, yaw = 0, roll = 0;
                                if (!singular) {
                                    pitch = Math.atan2(r21, r22) * (180.0 / Math.PI);
                                    yaw   = Math.atan2(-r20, sy) * (180.0 / Math.PI);
                                    roll  = Math.atan2(r10, r00) * (180.0 / Math.PI);
                                } else {
                                    pitch = Math.atan2(-r12, r11) * (180.0 / Math.PI);
                                    yaw   = Math.atan2(-r20, sy) * (180.0 / Math.PI);
                                    roll  = 0;
                                }

                                function cropEye(isLeft) {
                                    var eye_center = isLeft ? landmarks[1] : landmarks[0];
                                    var pt = { x: eye_center.x, y: eye_center.y };
                                    var roll_dx = landmarks[1].x - landmarks[0].x;
                                    var roll_dy = landmarks[1].y - landmarks[0].y;
                                    var angle = Math.atan2(roll_dy, roll_dx) * (180.0 / Math.PI);
                                    var dist_px = Math.sqrt(roll_dx * roll_dx + roll_dy * roll_dy);
                                    var scale = 70.0 / (dist_px > 1e-6 ? dist_px : 70.0);
                                    var target_size = { width: 60, height: 60 };
                                    var M = cv.getRotationMatrix2D(pt, angle, scale);
                                    M.data64F[2] += (target_size.width / 2.0) - eye_center.x;
                                    M.data64F[5] += (target_size.height / 2.0) - eye_center.y;
                                    var warped = new cv.Mat();
                                    // Note: we crop the eye from the original high-resolution grayMat
                                    cv.warpAffine(self.grayMat, warped, M, target_size, cv.INTER_LINEAR, cv.BORDER_REPLICATE);
                                    var warped_bgr = new cv.Mat();
                                    cv.cvtColor(warped, warped_bgr, cv.COLOR_GRAY2BGR);
                                    M.delete();
                                    warped.delete();
                                    return warped_bgr;
                                }

                                var left_eye_mat = cropEye(true);
                                var right_eye_mat = cropEye(false);
                                var size60 = { width: 60, height: 60 };
                                var scalarZeros60 = new cv.Scalar(0, 0, 0, 0);
                                var left_blob = cv.blobFromImage(left_eye_mat, 1.0, size60, scalarZeros60, false, false);
                                var right_blob = cv.blobFromImage(right_eye_mat, 1.0, size60, scalarZeros60, false, false);

                                var head_pose_data = cv.matFromArray(1, 3, cv.CV_32F, [-yaw, pitch, -roll]);

                                self.gazeNet.setInput(right_blob, 'left_eye_image');
                                self.gazeNet.setInput(left_blob, 'right_eye_image');
                                self.gazeNet.setInput(head_pose_data, 'head_pose_angles');

                                 var outName = 'gaze_vector/sink_port_0';
                                 if (self.gazeNet.getUnconnectedOutLayersNames) {
                                     try {
                                         var outNames = self.gazeNet.getUnconnectedOutLayersNames();
                                         if (outNames && outNames.size() > 0) {
                                             outName = outNames.get(0);
                                         }
                                         if (outNames && outNames.delete) {
                                             outNames.delete();
                                         }
                                     } catch (e) {
                                         console.warn('[GazeTracker] getUnconnectedOutLayersNames failed, falling back to default:', e);
                                     }
                                 }

                                 var output = self.gazeNet.forward(outName);
                                 if (!output.empty()) {
                                     var dx = 0, dy = 0, dz = 0;
                                     if (output.cols === 2) {
                                         var pitch_gaze = output.floatAt(0, 0);
                                         var yaw_gaze = output.floatAt(0, 1);
                                         var cos_pitch = Math.cos(pitch_gaze);
                                         dx = Math.sin(yaw_gaze) * cos_pitch;
                                         dy = Math.sin(pitch_gaze);
                                         dz = Math.cos(yaw_gaze) * cos_pitch;
                                     } else if (output.cols === 3) {
                                         dx = output.floatAt(0, 0);
                                         dy = output.floatAt(0, 1);
                                         dz = output.floatAt(0, 2);
                                     }
                                     var len_gaze = Math.hypot(dx, dy, dz);
                                     if (len_gaze > 0) {
                                         dx /= len_gaze; dy /= len_gaze; dz /= len_gaze;
                                     }

                                     // Convert Gaze ADAS Space to OpenCV Camera Space: (dx, -dy, dz)
                                     var dx_cv = dx;
                                     var dy_cv = -dy;
                                     var dz_cv = dz;

                                     var tx = tvec.doubleAt(0, 0);
                                     var ty = tvec.doubleAt(1, 0);
                                     var tz = tvec.doubleAt(2, 0);
                                     
                                     var lex = r00 * FaceModelGeometry.EYE_X + r01 * FaceModelGeometry.EYE_Y + r02 * FaceModelGeometry.EYE_Z + tx;
                                     var ley = r10 * FaceModelGeometry.EYE_X + r11 * FaceModelGeometry.EYE_Y + r12 * FaceModelGeometry.EYE_Z + ty;
                                     var lez = r20 * FaceModelGeometry.EYE_X + r21 * FaceModelGeometry.EYE_Y + r22 * FaceModelGeometry.EYE_Z + tz;
                                     
                                     var rex = r00 * -FaceModelGeometry.EYE_X + r01 * FaceModelGeometry.EYE_Y + r02 * FaceModelGeometry.EYE_Z + tx;
                                     var rey = r10 * -FaceModelGeometry.EYE_X + r11 * FaceModelGeometry.EYE_Y + r12 * FaceModelGeometry.EYE_Z + ty;
                                     var rez = r20 * -FaceModelGeometry.EYE_X + r21 * FaceModelGeometry.EYE_Y + r22 * FaceModelGeometry.EYE_Z + tz;

                                     var rx = rvec.doubleAt(0, 0);
                                     var ry = rvec.doubleAt(1, 0);
                                     var rz = rvec.doubleAt(2, 0);

                                     var coords = self.getCanvasScreenCoordinates();
                                     var canvasX = coords.x;
                                     var canvasY = coords.y;

                                       // Update live diagnostics HUD
                                       var debugPanel = document.getElementById('gaze-hud-panel');
                                       if (debugPanel && debugPanel.style.display !== 'none') {
                                           var debugCanvas = document.getElementById('gaze-hud-canvas');
                                           if (debugCanvas) {
                                               var ctx = debugCanvas.getContext('2d');
                                               ctx.drawImage(self.video, 0, 0, debugCanvas.width, debugCanvas.height);
                                               
                                               var hudScaleX = debugCanvas.width / self.frameMat.cols;
                                               var hudScaleY = debugCanvas.height / self.frameMat.rows;
                                               
                                               // Draw face landmarks
                                               ctx.fillStyle = '#00ffcc';
                                               for (var i = 0; i < landmarks.length; i++) {
                                                   ctx.beginPath();
                                                   ctx.arc(landmarks[i].x * hudScaleX, landmarks[i].y * hudScaleY, 3, 0, 2 * Math.PI);
                                                   ctx.fill();
                                               }
                                               
                                               // Draw gaze direction vector from midpoint of eyes
                                               var eye_mid_x = (landmarks[0].x + landmarks[1].x) / 2;
                                               var eye_mid_y = (landmarks[0].y + landmarks[1].y) / 2;
                                               ctx.strokeStyle = '#ff3366';
                                               ctx.lineWidth = 2;
                                               ctx.beginPath();
                                               ctx.moveTo(eye_mid_x * hudScaleX, eye_mid_y * hudScaleY);
                                               ctx.lineTo((eye_mid_x + dx_cv * 100) * hudScaleX, (eye_mid_y + dy_cv * 100) * hudScaleY);
                                               ctx.stroke();
                                           }

                                           var leftEyeCanvas = document.getElementById('gaze-hud-left-eye-canvas');
                                           if (leftEyeCanvas) {
                                               cv.imshow('gaze-hud-left-eye-canvas', left_eye_mat);
                                           }
                                           var rightEyeCanvas = document.getElementById('gaze-hud-right-eye-canvas');
                                           if (rightEyeCanvas) {
                                               cv.imshow('gaze-hud-right-eye-canvas', right_eye_mat);
                                           }
                                           
                                           var metricsEl = document.getElementById('gaze-hud-metrics');
                                          if (metricsEl) {
                                              self.lastMetrics = {
                                                  faceDetected: true,
                                                  gazeVector: { x: dx_cv, y: dy_cv, z: dz_cv },
                                                  headPose: { tx: tx, ty: ty, tz: tz, rx: rx, ry: ry, rz: rz },
                                                  canvasX: canvasX,
                                                  canvasY: canvasY
                                              };
                                              metricsEl.innerHTML = 
                                                  '<div>Face Tracked:</div><div style="color:#00ffcc">true</div>' +
                                                  '<div>Gaze Vector:</div><div>(' + dx_cv.toFixed(3) + ', ' + dy_cv.toFixed(3) + ', ' + dz_cv.toFixed(3) + ')</div>' +
                                                  '<div>Head Pos:</div><div>(' + tx.toFixed(1) + ', ' + ty.toFixed(1) + ', ' + tz.toFixed(1) + ')</div>' +
                                                  '<div>Canvas Pos:</div><div>(' + canvasX + ', ' + canvasY + ')</div>' +
                                                  '<div>Camera Res:</div><div>' + self.video.videoWidth + 'x' + self.video.videoHeight + '</div>';
                                          }
                                      }

                                      if (window.godotGaze && window.godotGaze.feed_gaze) {
                                          window.godotGaze.feed_gaze(true, lex, ley, lez, rex, rey, rez, dx_cv, dy_cv, dz_cv, tx, ty, tz, rx, ry, rz, canvasX, canvasY);
                                      }
                                 }

                                 R.delete();
                                 left_eye_mat.delete();
                                 right_eye_mat.delete();
                                 left_blob.delete();
                                 right_blob.delete();
                                 head_pose_data.delete();
                                 output.delete();
                             }
                             model_points.delete();
                             image_points.delete();
                             camera_matrix.delete();
                             dist_coeffs.delete();
                             rvec.delete();
                             tvec.delete();
                         } catch (gazeErr) {
                             var msg = gazeErr;
                             if (typeof gazeErr === 'number' && typeof cv !== 'undefined' && cv.exceptionFromPtr) {
                                 try { msg = cv.exceptionFromPtr(gazeErr).msg; } catch(e) {}
                             } else if (gazeErr && gazeErr.message) {
                                 msg = gazeErr.message;
                             }
                             console.error('[GazeTracker] Gaze estimation failed:', msg);
                         }
                     } else {
                         // Push face lost explicitly to Godot
                         if (window.godotGaze && window.godotGaze.feed_gaze) {
                             window.godotGaze.feed_gaze(false, 0, 0, 0, 0, 0, 0);
                         }

                         // Update HUD for face lost
                         var debugPanel = document.getElementById('gaze-hud-panel');
                         if (debugPanel && debugPanel.style.display !== 'none') {
                             var debugCanvas = document.getElementById('gaze-hud-canvas');
                             if (debugCanvas) {
                                 var ctx = debugCanvas.getContext('2d');
                                 ctx.drawImage(self.video, 0, 0, debugCanvas.width, debugCanvas.height);
                             }
                             var metricsEl = document.getElementById('gaze-hud-metrics');
                             if (metricsEl) {
                                 self.lastMetrics = {
                                     faceDetected: false,
                                     gazeVector: null,
                                     headPose: null,
                                     canvasX: 0,
                                     canvasY: 0
                                 };
                                 metricsEl.innerHTML = 
                                     '<div>Face Tracked:</div><div style="color:#ff3366">false</div>' +
                                     '<div>Gaze Vector:</div><div>N/A</div>' +
                                     '<div>Head Pos:</div><div>N/A</div>' +
                                     '<div>Canvas Pos:</div><div>N/A</div>' +
                                     '<div>Camera Res:</div><div>' + self.video.videoWidth + 'x' + self.video.videoHeight + '</div>';
                             }
                         }
                     }
                    scheduleNextFrame();
                }
                trackFrame();
            }).catch(function(err) {
                console.error('[GazeTracker] Camera failed to start:', err);
            });
        },

        getCanvasScreenCoordinates: function() {
            var canvas = document.getElementById('canvas') || document.querySelector('canvas') || this.canvas;
            var rect = canvas ? canvas.getBoundingClientRect() : { left: 0, top: 0 };

            var isFullscreen = !!(document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement);
            if (isFullscreen) {
                return { x: 0, y: 0 };
            }

            var winLeft = 0;
            var winTop = 0;
            try {
                winLeft = (window.screenLeft !== undefined) ? window.screenLeft : window.screenX;
                winTop = (window.screenTop !== undefined) ? window.screenTop : window.screenY;
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
                        this._borderOffset = Math.max(0, (window.outerWidth - window.innerWidth) / 2);
                    }
                    if (window.outerHeight && window.innerHeight) {
                        this._chromeOffset = Math.max(0, window.outerHeight - window.innerHeight - this._borderOffset);
                    }
                } catch (e) {}
            }
            var dpr = window.devicePixelRatio || 1.0;
            return {
                x: (winLeft + this._borderOffset + rect.left) * dpr,
                y: (winTop + this._chromeOffset + rect.top) * dpr
            };
        },

        createDebugHUD: function() {
            if (document.getElementById('gaze-hud-trigger')) return;

            var trigger = document.createElement('button');
            trigger.id = 'gaze-hud-trigger';
            trigger.innerHTML = '👁️ Gaze HUD';
            Object.assign(trigger.style, {
                position: 'fixed',
                bottom: '20px',
                right: '20px',
                zIndex: '999999',
                padding: '10px 16px',
                background: 'rgba(18, 18, 24, 0.85)',
                color: '#00ffcc',
                border: '1px solid rgba(0, 255, 204, 0.3)',
                borderRadius: '20px',
                cursor: 'pointer',
                fontFamily: 'system-ui, -apple-system, sans-serif',
                fontSize: '13px',
                fontWeight: '600',
                boxShadow: '0 4px 20px rgba(0,0,0,0.3)',
                backdropFilter: 'blur(8px)',
                webkitBackdropFilter: 'blur(8px)',
                transition: 'all 0.2s ease-in-out'
            });

            var panel = document.createElement('div');
            panel.id = 'gaze-hud-panel';
            Object.assign(panel.style, {
                position: 'fixed',
                bottom: '80px',
                right: '20px',
                width: '340px',
                maxHeight: '520px',
                zIndex: '999998',
                background: 'rgba(18, 18, 24, 0.9)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                borderRadius: '12px',
                boxShadow: '0 8px 32px rgba(0, 0, 0, 0.4)',
                backdropFilter: 'blur(16px)',
                webkitBackdropFilter: 'blur(16px)',
                display: 'none',
                flexDirection: 'column',
                color: '#e2e8f0',
                fontFamily: 'system-ui, -apple-system, sans-serif',
                overflow: 'hidden'
            });

            var header = document.createElement('div');
            Object.assign(header.style, {
                padding: '12px 16px',
                borderBottom: '1px solid rgba(255, 255, 255, 0.08)',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                background: 'rgba(255, 255, 255, 0.02)'
            });
            header.innerHTML = '<span style="font-weight:700; font-size:12px; letter-spacing:0.5px; color:#00ffcc;">GAZE DIAGNOSTICS</span>';
            
            var closeBtn = document.createElement('button');
            closeBtn.innerHTML = '✕';
            Object.assign(closeBtn.style, {
                background: 'none',
                border: 'none',
                color: '#a0aec0',
                cursor: 'pointer',
                fontSize: '14px',
                padding: '0'
            });
            closeBtn.onclick = function() { panel.style.display = 'none'; };
            header.appendChild(closeBtn);
            panel.appendChild(header);

            var body = document.createElement('div');
            Object.assign(body.style, {
                padding: '12px',
                overflowY: 'auto',
                display: 'flex',
                flexDirection: 'column',
                gap: '10px'
            });

            var canvas = document.createElement('canvas');
            canvas.id = 'gaze-hud-canvas';
            canvas.width = 320;
            canvas.height = 240;
            Object.assign(canvas.style, {
                width: '100%',
                borderRadius: '6px',
                background: '#0a0a0f',
                border: '1px solid rgba(255, 255, 255, 0.05)'
            });
            body.appendChild(canvas);

            var eyesContainer = document.createElement('div');
            Object.assign(eyesContainer.style, {
                display: 'flex',
                justifyContent: 'space-around',
                gap: '10px',
                margin: '4px 0'
            });

            var leftEyeBox = document.createElement('div');
            Object.assign(leftEyeBox.style, {
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '4px'
            });
            leftEyeBox.innerHTML = '<span style="font-size: 10px; color: #a0aec0; font-family: system-ui;">Left Eye (Img Right)</span>';
            var leftEyeCanvas = document.createElement('canvas');
            leftEyeCanvas.id = 'gaze-hud-left-eye-canvas';
            leftEyeCanvas.width = 60;
            leftEyeCanvas.height = 60;
            Object.assign(leftEyeCanvas.style, {
                width: '60px',
                height: '60px',
                borderRadius: '4px',
                background: '#0a0a0f',
                border: '1px solid rgba(255, 255, 255, 0.1)'
            });
            leftEyeBox.appendChild(leftEyeCanvas);

            var rightEyeBox = document.createElement('div');
            Object.assign(rightEyeBox.style, {
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '4px'
            });
            rightEyeBox.innerHTML = '<span style="font-size: 10px; color: #a0aec0; font-family: system-ui;">Right Eye (Img Left)</span>';
            var rightEyeCanvas = document.createElement('canvas');
            rightEyeCanvas.id = 'gaze-hud-right-eye-canvas';
            rightEyeCanvas.width = 60;
            rightEyeCanvas.height = 60;
            Object.assign(rightEyeCanvas.style, {
                width: '60px',
                height: '60px',
                borderRadius: '4px',
                background: '#0a0a0f',
                border: '1px solid rgba(255, 255, 255, 0.1)'
            });
            rightEyeBox.appendChild(rightEyeCanvas);

            eyesContainer.appendChild(rightEyeBox);
            eyesContainer.appendChild(leftEyeBox);
            body.appendChild(eyesContainer);

            var metrics = document.createElement('div');
            metrics.id = 'gaze-hud-metrics';
            Object.assign(metrics.style, {
                fontSize: '11px',
                fontFamily: 'monospace',
                display: 'grid',
                gridTemplateColumns: '1fr 1.5fr',
                gap: '4px 8px',
                padding: '8px',
                background: 'rgba(0,0,0,0.25)',
                borderRadius: '6px',
                border: '1px solid rgba(255, 255, 255, 0.05)',
                color: '#cbd5e0'
            });
            body.appendChild(metrics);

            var copyBtn = document.createElement('button');
            copyBtn.innerHTML = '📋 Copy Debug Logs & Info';
            Object.assign(copyBtn.style, {
                padding: '8px 10px',
                background: 'rgba(0, 255, 204, 0.1)',
                border: '1px solid rgba(0, 255, 204, 0.3)',
                color: '#00ffcc',
                borderRadius: '6px',
                cursor: 'pointer',
                fontSize: '11px',
                fontWeight: '600',
                transition: 'all 0.2s ease'
            });
            copyBtn.onmouseenter = function() { copyBtn.style.background = 'rgba(0, 255, 204, 0.2)'; };
            copyBtn.onmouseleave = function() { copyBtn.style.background = 'rgba(0, 255, 204, 0.1)'; };
            
            var self = this;
            copyBtn.onclick = function() {
                var info = {
                    timestamp: new Date().toISOString(),
                    active: self.active,
                    faceDetected: self.lastMetrics ? self.lastMetrics.faceDetected : false,
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
                        chromeOffset: self._chromeOffset
                    },
                    userAgent: navigator.userAgent,
                    logs: self.debugLogs || []
                };
                navigator.clipboard.writeText(JSON.stringify(info, null, 2)).then(function() {
                    copyBtn.innerHTML = '✅ Copied!';
                    setTimeout(function() { copyBtn.innerHTML = '📋 Copy Debug Logs & Info'; }, 2000);
                });
            };
            body.appendChild(copyBtn);

            panel.appendChild(body);
            document.body.appendChild(trigger);
            document.body.appendChild(panel);

            trigger.onclick = function() {
                if (panel.style.display === 'none') {
                    panel.style.display = 'flex';
                } else {
                    panel.style.display = 'none';
                }
            };

            trigger.onmouseenter = function() {
                trigger.style.background = '#00ffcc';
                trigger.style.color = '#121214';
            };
            trigger.onmouseleave = function() {
                trigger.style.background = 'rgba(18, 18, 24, 0.85)';
                trigger.style.color = '#00ffcc';
            };
        },

        cleanupPipeline: function() {
            if (this.stream) {
                this.stream.getTracks().forEach(function(track) { track.stop(); });
            }
            if (this.video) {
                this.video.remove();
            }
            var trigger = document.getElementById('gaze-hud-trigger');
            if (trigger) trigger.remove();
            var panel = document.getElementById('gaze-hud-panel');
            if (panel) panel.remove();

            if (this.frameMat) { this.frameMat.delete(); this.frameMat = null; }
            if (this.bgrMat) { this.bgrMat.delete(); this.bgrMat = null; }
            if (this.grayMat) { this.grayMat.delete(); this.grayMat = null; }
            if (this.facesMat) { this.facesMat.delete(); this.facesMat = null; }
            if (this.detectMat) { this.detectMat.delete(); this.detectMat = null; }
            if (this.detector && this.detector.delete) this.detector.delete();
            if (this.gazeNet) this.gazeNet.delete();

            this.stream = null;
            this.video = null;
            console.log('[GazeTracker] Web tracking loop stopped and resources cleaned up.');
        },

        stopTracking: function() {
            this.active = false;
        }
    };

    window.gazeTracker = gazeTracker;
})();
