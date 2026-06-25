// tools/test_opencv.js
// Node.js scratch script to inspect opencv.js capabilities.

const fs = require('fs');
const path = require('path');

const OPENCV_PATH = path.join(__dirname, '../project/exports/opencv.js');

if (!fs.existsSync(OPENCV_PATH)) {
    console.error('Error: opencv.js not found at:', OPENCV_PATH);
    process.exit(1);
}

console.log('Loading opencv.js...');
const cv = require(OPENCV_PATH);

function checkInitialization() {
    if (cv.Mat && cv.FS && cv.FS.writeFile) {
        console.log('OpenCV.js Runtime Initialized successfully.');
        runInspection();
    } else {
        setTimeout(checkInitialization, 100);
    }
}

function runInspection() {
    console.log('--- OpenCV.js Inspection ---');
    console.log('cv version:', cv.version || 'unknown');
    console.log('cv.readNet defined:', !!cv.readNet);
    
    // 1. Test YuNet ONNX with 640x640 input shape
    const yunetPath = path.join(__dirname, '../project/models/face_detection_yunet_2023mar.onnx');
    if (fs.existsSync(yunetPath)) {
        console.log('\n--- Testing YuNet (640x640 input shape) ---');
        const data = fs.readFileSync(yunetPath);
        try {
            cv.FS.writeFile('yunet.onnx', data);
            const net = cv.readNet('yunet.onnx');
            
            // YuNet model input size is statically [1, 3, 640, 640] in this ONNX file
            const dummyImg = new cv.Mat(640, 640, cv.CV_8UC3, new cv.Scalar(0, 0, 0));
            const size640 = new cv.Size(640, 640);
            const scalarZeros = new cv.Scalar(0, 0, 0);
            
            console.log('Creating input blob via blobFromImage at 640x640...');
            const blob = cv.blobFromImage(dummyImg, 1.0, size640, scalarZeros, false, false);
            console.log('Input blob created. Dims:', blob.dims);
            
            net.setInput(blob);
            
            // Let's call forward with no arguments to see if it works with 640x640
            console.log('Running net.forward()...');
            const outs = net.forward();
            console.log('YuNet Forward PASS! Output shape:', outs.size());
            
            dummyImg.delete();
            size640.delete();
            scalarZeros.delete();
            blob.delete();
            outs.delete();
        } catch (err) {
            let msg = err;
            if (typeof err === 'number' && cv.exceptionFromPtr) {
                try { msg = cv.exceptionFromPtr(err).msg; } catch(e) {}
            } else if (err && err.message) {
                msg = err.message;
            }
            console.error('YuNet Forward FAIL:', msg);
        }
    }

    // 2. Test Gaze Estimation ONNX with correct output layer name
    const gazePath = path.join(__dirname, '../project/models/gaze-estimation-adas-0002.onnx');
    if (fs.existsSync(gazePath)) {
        console.log('\n--- Testing Gaze Estimation ADAS (with gaze_vector/sink_port_0 output name) ---');
        const data = fs.readFileSync(gazePath);
        try {
            cv.FS.writeFile('gaze.onnx', data);
            const net = cv.readNet('gaze.onnx');
            
            const leftEyeImg = new cv.Mat(60, 60, cv.CV_8UC3, new cv.Scalar(0, 0, 0));
            const rightEyeImg = new cv.Mat(60, 60, cv.CV_8UC3, new cv.Scalar(0, 0, 0));
            
            const size60 = new cv.Size(60, 60);
            const scalarZeros = new cv.Scalar(0, 0, 0);
            const leftBlob = cv.blobFromImage(leftEyeImg, 1.0, size60, scalarZeros, false, false);
            const rightBlob = cv.blobFromImage(rightEyeImg, 1.0, size60, scalarZeros, false, false);
            
            const headPose = cv.matFromArray(1, 3, cv.CV_32F, [0.0, 0.0, 0.0]);
            
            net.setInput(rightBlob, 'left_eye_image');
            net.setInput(leftBlob, 'right_eye_image');
            net.setInput(headPose, 'head_pose_angles');
            
            console.log('Running net.forward("gaze_vector/sink_port_0")...');
            const output = net.forward('gaze_vector/sink_port_0');
            console.log('Gaze ADAS Forward PASS! Output cols:', output.cols, 'rows:', output.rows);
            if (output.cols > 0 && output.rows > 0) {
                console.log('Gaze output vector values:', output.floatAt(0, 0), output.floatAt(0, 1), output.floatAt(0, 2));
            }
            
            leftEyeImg.delete();
            rightEyeImg.delete();
            leftBlob.delete();
            rightBlob.delete();
            headPose.delete();
            output.delete();
        } catch (err) {
            let msg = err;
            if (typeof err === 'number' && cv.exceptionFromPtr) {
                try { msg = cv.exceptionFromPtr(err).msg; } catch(e) {}
            } else if (err && err.message) {
                msg = err.message;
            }
            console.error('Gaze ADAS Forward FAIL:', msg);
        }
    }
    
    process.exit(0);
}

checkInitialization();
