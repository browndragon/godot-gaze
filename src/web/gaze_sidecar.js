// gaze_sidecar.js
// Standalone JavaScript code for the Godot Gaze sidecar tracking loop.
// Design B: Uses native cv.FaceDetectorYN from custom compiled opencv.js.

(function() {
    if (window.gazeTracker) return;

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

        // 1. Script fetch and inject helper (bypasses CORS/CORP server issues)
        fetchAndInject: function(localUrl, fallbackUrl, onload, onerror) {
            console.log('[GazeTracker] Fetching local script: ' + localUrl);
            fetch(localUrl)
                .then(function(res) {
                    if (!res.ok) throw new Error('HTTP status ' + res.status);
                    return res.blob();
                })
                .then(function(blob) {
                    var url = URL.createObjectURL(blob);
                    var s = document.createElement('script');
                    s.src = url;
                    s.onload = function() {
                        URL.revokeObjectURL(url);
                        console.log('[GazeTracker] Local script loaded successfully via Blob URL: ' + localUrl);
                        onload();
                    };
                    s.onerror = function(err) {
                        console.error('[GazeTracker] Failed to load Blob URL for: ' + localUrl, err);
                        onerror(err);
                    };
                    document.head.appendChild(s);
                })
                .catch(function(err) {
                    console.warn('[GazeTracker] Local fetch failed for ' + localUrl + ': ' + err.message + '. Trying CDN fallback...');
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
                });
        },

        // 2. Main initializer and loop
        startTracking: function(yunetPath, gazeOnnxPath) {
            if (this.active) return;
            this.active = true;

            var self = this;
            console.log('[GazeTracker] Initializing sidecar tracking pipeline...');

            this.fetchAndInject('opencv.js', 'https://cdn.jsdelivr.net/npm/@techstark/opencv-js@4.9.0-release.2/dist/opencv.js', function() {
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
            this.video.width = 320;
            this.video.height = 240;
            this.video.autoplay = true;
            this.video.playsInline = true;
            this.video.muted = true;
            this.video.style.display = 'none';
            document.body.appendChild(this.video);

            var self = this;
            navigator.mediaDevices.getUserMedia({
                video: { width: 320, height: 240, facingMode: 'user' }
            }).then(function(stream) {
                self.stream = stream;
                self.video.srcObject = stream;
                self.video.play();

                var cap = new cv.VideoCapture(self.video);
                self.frameMat = new cv.Mat(self.video.height, self.video.width, cv.CV_8UC4);
                self.bgrMat = new cv.Mat();
                self.grayMat = new cv.Mat();
                self.facesMat = new cv.Mat();

                // Allocate reusable downscaled Mat for face detection
                var detectMat = new cv.Mat();
                var detectSize = { width: self.faceDetectWidth, height: self.faceDetectHeight };

                function trackFrame() {
                    if (!self.active) {
                        detectMat.delete();
                        self.cleanupPipeline();
                        return;
                    }
                    // Fix Video Metadata Race Condition
                    if (self.video.paused || self.video.ended || !self.video.videoWidth) {
                        requestAnimationFrame(trackFrame);
                        return;
                    }
                    try {
                        cap.read(self.frameMat);
                        cv.cvtColor(self.frameMat, self.bgrMat, cv.COLOR_RGBA2BGR);
                        cv.cvtColor(self.bgrMat, self.grayMat, cv.COLOR_BGR2GRAY);
                    } catch (readErr) {
                        console.error('[GazeTracker] Camera frame read/convert failed:', readErr.message || readErr);
                        requestAnimationFrame(trackFrame);
                        return;
                    }

                    var detected = false;
                    var landmarks = [];

                    // 1. Face detection step
                    try {
                        // Resize input image to the configured detection resolution (e.g. 160x128)
                        cv.resize(self.bgrMat, detectMat, detectSize, 0, 0, cv.INTER_LINEAR);
                        self.detector.setInputSize(detectSize);
                        self.detector.detect(detectMat, self.facesMat);
                        
                        if (self.facesMat.rows > 0) {
                            detected = true;
                            // Scale landmarks back to the original 320x240 video space
                            var scaleX = self.video.videoWidth / self.faceDetectWidth;
                            var scaleY = self.video.videoHeight / self.faceDetectHeight;
                            
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
                                -30.0, -28.676, 0.0,
                                30.0, -28.676, 0.0,
                                0.0, -5.0, -45.0,
                                -18.462, 31.712, -4.55,
                                18.462, 31.712, -4.55
                            ]);
                            var image_points = cv.matFromArray(5, 2, cv.CV_32F, [
                                landmarks[0].x, landmarks[0].y,
                                landmarks[1].x, landmarks[1].y,
                                landmarks[2].x, landmarks[2].y,
                                landmarks[3].x, landmarks[3].y,
                                landmarks[4].x, landmarks[4].y
                            ]);
                            var cx = self.video.videoWidth / 2.0;
                            var cy = self.video.videoHeight / 2.0;
                            var fx = self.video.videoWidth * 1.5625;
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
                                pitch += 0.017 * tvec.doubleAt(1, 0);
                                yaw += 0.017 * tvec.doubleAt(0, 0);

                                function cropEye(isLeft) {
                                    var eye_center = isLeft ? landmarks[1] : landmarks[0];
                                    var pt = { x: eye_center.x, y: eye_center.y };
                                    var roll_dx = landmarks[1].x - landmarks[0].x;
                                    var roll_dy = landmarks[1].y - landmarks[0].y;
                                    var angle = Math.atan2(roll_dy, roll_dx) * (180.0 / Math.PI);
                                    var scale = 1.0;
                                    var target_size = { width: 60, height: 60 };
                                    var M = cv.getRotationMatrix2D(pt, angle, scale);
                                    M.data64F[2] += (target_size.width / 2.0) - eye_center.x;
                                    M.data64F[5] += (target_size.height / 2.0) - eye_center.y;
                                    var warped = new cv.Mat();
                                    // Note: we crop the eye from the original high-resolution grayMat (320x240)
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

                                var output = self.gazeNet.forward('gaze_vector/sink_port_0');
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
                                    var tx = tvec.doubleAt(0, 0);
                                    var ty = tvec.doubleAt(1, 0);
                                    var tz = tvec.doubleAt(2, 0);
                                    var lex = r00 * 30.0 + r01 * -20.0 + tx;
                                    var ley = r10 * 30.0 + r11 * -20.0 + ty;
                                    var lez = r20 * 30.0 + r21 * -20.0 + tz;
                                    var rex = r00 * -30.0 + r01 * -20.0 + tx;
                                    var rey = r10 * -30.0 + r11 * -20.0 + ty;
                                    var rez = r20 * -30.0 + r21 * -20.0 + tz;
                                    var ox = (lex + rex) / 2.0;
                                    var oy = (ley + rey) / 2.0;
                                    var oz = (lez + rez) / 2.0;

                                    if (window.godotGaze && window.godotGaze.feed_gaze) {
                                        window.godotGaze.feed_gaze(true, ox, oy, oz, dx, dy, dz);
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
                    }
                    requestAnimationFrame(trackFrame);
                }
                trackFrame();
            }).catch(function(err) {
                console.error('[GazeTracker] Camera failed to start:', err);
            });
        },

        cleanupPipeline: function() {
            if (this.stream) {
                this.stream.getTracks().forEach(function(track) { track.stop(); });
            }
            if (this.video) {
                this.video.remove();
            }
            if (this.frameMat) this.frameMat.delete();
            if (this.bgrMat) this.bgrMat.delete();
            if (this.grayMat) this.grayMat.delete();
            if (this.facesMat) this.facesMat.delete();
            if (this.detector && this.detector.delete) this.detector.delete();
            if (this.gazeNet) this.gazeNet.delete();

            this.stream = null;
            this.video = null;
            this.frameMat = null;
            this.bgrMat = null;
            this.grayMat = null;
            this.facesMat = null;
            this.detector = null;
            this.gazeNet = null;
            console.log('[GazeTracker] Web tracking loop stopped and resources cleaned up.');
        },

        stopTracking: function() {
            this.active = false;
        }
    };

    window.gazeTracker = gazeTracker;
})();
