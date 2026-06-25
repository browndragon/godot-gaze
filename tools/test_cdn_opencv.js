// tools/test_cdn_opencv.js
const fs = require('fs');
const path = require('path');
const https = require('https');

const CDN_URL = 'https://docs.opencv.org/4.9.0/opencv.js';
const DEST_PATH = path.join(__dirname, '../project/exports/opencv_official.js');

function downloadOfficial(callback) {
    if (fs.existsSync(DEST_PATH)) {
        return callback();
    }
    console.log('Downloading official OpenCV.js from CDN...');
    const file = fs.createWriteStream(DEST_PATH);
    https.get(CDN_URL, (response) => {
        response.pipe(file);
        file.on('finish', () => {
            file.close();
            console.log('Download completed.');
            callback();
        });
    }).on('error', (err) => {
        fs.unlinkSync(DEST_PATH);
        console.error('Download failed:', err.message);
    });
}

function runTest() {
    console.log('Loading official opencv.js...');
    const cv = require(DEST_PATH);
    
    function check() {
        if (cv.Mat && cv.FS) {
            console.log('Official OpenCV.js runtime initialized.');
            console.log('cv.FaceDetectorYN defined:', !!cv.FaceDetectorYN);
            if (cv.FaceDetectorYN) {
                console.log('cv.FaceDetectorYN.create defined:', !!cv.FaceDetectorYN.create);
            }
            process.exit(0);
        } else {
            setTimeout(check, 100);
        }
    }
    check();
}

downloadOfficial(runTest);
