#!/usr/bin/env node

const { spawn } = require('child_process');
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 9224; // Chrome debugger port
const SERVER_PORT = 8002; // test server port
const CHROME_PATH = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const TARGET_URL = `http://localhost:${SERVER_PORT}/tests/test_sidecar_gaze.html`;

const server = http.createServer((req, res) => {
    const urlPath = decodeURIComponent(req.url.split('?')[0]);
    let filePath = '';

    // Enable cross-origin isolation headers for WebAssembly SharedArrayBuffer / threads support
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');

    if (urlPath === '/tests/test_sidecar_gaze.html') {
        filePath = path.join(__dirname, '../tests/test_sidecar_gaze.html');
        res.setHeader('Content-Type', 'text/html');
    } else if (urlPath === '/src/web/gaze_sidecar.js') {
        filePath = path.join(__dirname, '../src/web/gaze_sidecar.js');
        res.setHeader('Content-Type', 'application/javascript');
    } else if (urlPath.startsWith('/project/')) {
        filePath = path.join(__dirname, '..', urlPath);
        if (urlPath.endsWith('.js')) {
            res.setHeader('Content-Type', 'application/javascript');
        } else if (urlPath.endsWith('.wasm')) {
            res.setHeader('Content-Type', 'application/wasm');
        } else {
            res.setHeader('Content-Type', 'application/octet-stream');
        }
    } else if (urlPath.startsWith('/tests/resources/')) {
        filePath = path.join(__dirname, '..', urlPath);
        res.setHeader('Content-Type', 'image/jpeg');
    } else {
        res.statusCode = 404;
        return res.end('404 Not Found');
    }

    try {
        if (fs.existsSync(filePath)) {
            res.end(fs.readFileSync(filePath));
        } else {
            console.error(`[TestServer] File not found: ${filePath}`);
            res.statusCode = 404;
            res.end('404 Not Found');
        }
    } catch (err) {
        console.error(`[TestServer] Error serving ${filePath}:`, err);
        res.statusCode = 500;
        res.end('500 Internal Error');
    }
});

server.listen(SERVER_PORT, () => {
    console.log(`[TestServer] Running at http://localhost:${SERVER_PORT}/`);
});

// 2. Launch Chrome
console.log('[TestRunner] Launching Google Chrome headlessly...');
const chromeProcess = spawn(CHROME_PATH, [
    '--headless=new',
    `--remote-debugging-port=${PORT}`,
    'about:blank'
]);

chromeProcess.on('error', (err) => {
    console.error('[TestRunner] Failed to start Chrome:', err);
    server.close();
    process.exit(1);
});

process.on('exit', () => {
    try { chromeProcess.kill(); } catch (e) {}
    try { server.close(); } catch (e) {}
});

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function getWsUrl() {
    for (let i = 0; i < 20; i++) {
        try {
            const res = await fetch(`http://127.0.0.1:${PORT}/json/list`);
            const tabs = await res.json();
            const tab = tabs.find(t => t.type === 'page');
            if (tab && tab.webSocketDebuggerUrl) {
                return tab.webSocketDebuggerUrl;
            }
        } catch (e) {}
        await sleep(500);
    }
    throw new Error('Timeout waiting for Chrome Remote Debugging port');
}

async function run() {
    let ws;
    try {
        const wsUrl = await getWsUrl();
        console.log('[TestRunner] Connected to Chrome DevTools Protocol.');

        ws = new WebSocket(wsUrl);

        const responsePromises = {};
        let messageId = 1;

        function sendCommand(method, params = {}) {
            const id = messageId++;
            const payload = JSON.stringify({ id, method, params });
            ws.send(payload);
            return new Promise((resolve, reject) => {
                responsePromises[id] = { resolve, reject };
            });
        }

        ws.onopen = async () => {
            try {
                await sendCommand('Runtime.enable');
                await sendCommand('Page.enable');
                
                console.log(`[TestRunner] Navigating to ${TARGET_URL}...`);
                await sendCommand('Page.navigate', { url: TARGET_URL });
            } catch (err) {
                console.error('[TestRunner] Failed to initialize page:', err);
            }
        };

        ws.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            if (msg.id && responsePromises[msg.id]) {
                if (msg.error) {
                    responsePromises[msg.id].reject(msg.error);
                } else {
                    responsePromises[msg.id].resolve(msg.result);
                }
                delete responsePromises[msg.id];
            } else if (msg.method === 'Runtime.consoleAPICalled') {
                const args = msg.params.args.map(arg => arg.value || arg.description || '').join(' ');
                console.log(`[BROWSER CONSOLE] ${args}`);
            }
        };

        // Wait for page load and model initialization
        console.log('[TestRunner] Waiting for page and models to initialize (approx 4s)...');
        await sleep(4000);

        // Ensure initPromise resolved
        await sendCommand('Runtime.evaluate', {
            expression: "window.initPromise",
            awaitPromise: true
        });

        const testImages = [
            'self_center.jpg',
            'self_left_left.jpg',
            'self_right_right.jpg',
            'self_top_top.jpg',
            'self_down_down.jpg'
        ];

        const results = {};

        console.log('\n--- Running Sidecar Coordinate Extraction Tests ---');
        for (const imgName of testImages) {
            console.log(`[TestRunner] Testing image: ${imgName}...`);
            const res = await sendCommand('Runtime.evaluate', {
                expression: `window.runGazeTest("${imgName}")`,
                awaitPromise: true,
                returnByValue: true
            });

            if (res.exceptionDetails) {
                throw new Error(`Test image ${imgName} failed with exception: ${res.exceptionDetails.exception.description}`);
            }

            const data = res.result.value;
            results[imgName] = data;

            console.log(`  Face Tracked: ${data.face_detected}`);
            console.log(`  Left Eye Center:  (${data.lex.toFixed(2)}, ${data.ley.toFixed(2)}, ${data.lez.toFixed(2)})`);
            console.log(`  Right Eye Center: (${data.rex.toFixed(2)}, ${data.rey.toFixed(2)}, ${data.rez.toFixed(2)})`);
            console.log(`  Gaze Dir:         (${data.dx.toFixed(4)}, ${data.dy.toFixed(4)}, ${data.dz.toFixed(4)})`);
            console.log(`  Head Translation: (${data.tx.toFixed(2)}, ${data.ty.toFixed(2)}, ${data.tz.toFixed(2)})`);
            console.log(`  Head Rotation:    (${data.rx.toFixed(4)}, ${data.ry.toFixed(4)}, ${data.rz.toFixed(4)})`);
            console.log();
        }

        // 3. Verification Assertions
        console.log('--- Running Assertions on Sidecar Results ---');

        // Check face detected
        for (const imgName of testImages) {
            if (!results[imgName].face_detected) {
                throw new Error(`Assertion Failed: Face should be detected in ${imgName}`);
            }
        }

        const center = results['self_center.jpg'];
        const left = results['self_left_left.jpg'];
        const right = results['self_right_right.jpg'];
        const top = results['self_top_top.jpg'];
        const down = results['self_down_down.jpg'];

        // Left eye center X should be greater than right eye center X
        for (const imgName of testImages) {
            const data = results[imgName];
            if (data.lex <= data.rex) {
                throw new Error(`Assertion Failed: Left eye X should be greater than right eye X in ${imgName}. lex=${data.lex}, rex=${data.rex}`);
            }
        }

        // Translation monotonicity (X: left head center > right head center)
        // Wait, since +X is camera-right (image-right/viewer-left):
        // When user tilts/shifts left (from viewer perspective, this is user-right, so negative X on camera):
        // Let's check native:
        // In native test_native.cpp line 407: left->translation.x > right->translation.x
        // Where left translation has a positive X (+47.5) and right has a negative X (-14.6).
        // Let's assert: left.tx > right.tx
        console.log(`  Verifying translation monotonicity: left.tx (${left.tx.toFixed(2)}) > right.tx (${right.tx.toFixed(2)})`);
        if (left.tx <= right.tx) {
            throw new Error(`Assertion Failed: Translation X monotonicity failed. left=${left.tx}, right=${right.tx}`);
        }

        // Gaze direction monotonicity: left.dx > right.dx
        console.log(`  Verifying gaze direction monotonicity: left.dx (${left.dx.toFixed(4)}) > right.dx (${right.dx.toFixed(4)})`);
        if (left.dx <= right.dx) {
            throw new Error(`Assertion Failed: Gaze direction X monotonicity failed. left=${left.dx}, right=${right.dx}`);
        }

        console.log('\n=== ALL SIDECAR COORDINATE TESTS PASSED SUCCESSFULLY! ===');
        ws.close();
        chromeProcess.kill();
        server.close();
        process.exit(0);

    } catch (err) {
        console.error('[TestRunner] Test run failed with error:', err);
        if (ws) ws.close();
        chromeProcess.kill();
        server.close();
        process.exit(1);
    }
}

run();
