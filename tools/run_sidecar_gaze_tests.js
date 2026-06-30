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
    '--enable-features=SharedArrayBuffer',
    '--js-flags=--experimental-wasm-threads',
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

// Safety timeout: abort tests if they hang (e.g. due to wasm compile error)
const safetyTimeout = setTimeout(() => {
    console.error('[TestRunner] Error: Safety timeout reached. Headless tests took too long and were aborted.');
    try { chromeProcess.kill(); } catch (e) {}
    try { server.close(); } catch (e) {}
    process.exit(1);
}, 25000);
if (typeof safetyTimeout.unref === 'function') {
    safetyTimeout.unref();
}

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
            } else if (msg.method === 'Runtime.exceptionThrown') {
                console.error(`[BROWSER EXCEPTION]`, msg.params.exceptionDetails.exception.description || msg.params.exceptionDetails.text);
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

            // Math Parity Validation: Compare with C++ expected values (precomputed from native logs)
            const expected = await sendCommand('Runtime.evaluate', {
                expression: `window.cppTestData["${imgName}"]`,
                returnByValue: true
            });
            const exp = expected.result.value;
            
            // Check that left/right eye centers and gaze direction match to within 0.05
            const tol = 0.05;
            
            const diffLex = Math.abs(data.lex - exp.lex);
            const diffLey = Math.abs(data.ley - exp.ley);
            const diffLez = Math.abs(data.lez - exp.lez);
            
            const diffRex = Math.abs(data.rex - exp.rex);
            const diffRey = Math.abs(data.rey - exp.rey);
            const diffRez = Math.abs(data.rez - exp.rez);
            
            const diffDx = Math.abs(data.dx - exp.gx);
            const diffDy = Math.abs(data.dy - (-exp.gy)); // dy in Godot feed is dy_cv, which is -gy
            const diffDz = Math.abs(data.dz - exp.gz);

            console.log(`  [Parity Check] Left Eye Diff:  (${diffLex.toFixed(4)}, ${diffLey.toFixed(4)}, ${diffLez.toFixed(4)})`);
            console.log(`  [Parity Check] Right Eye Diff: (${diffRex.toFixed(4)}, ${diffRey.toFixed(4)}, ${diffRez.toFixed(4)})`);
            console.log(`  [Parity Check] Gaze Dir Diff:  (${diffDx.toFixed(4)}, ${diffDy.toFixed(4)}, ${diffDz.toFixed(4)})`);
            console.log();

            if (diffLex > tol || diffLey > tol || diffLez > tol ||
                diffRex > tol || diffRey > tol || diffRez > tol ||
                diffDx > tol || diffDy > tol || diffDz > tol) {
                throw new Error(`Math Parity Assertion Failed for ${imgName}! Diff exceeded tolerance of ${tol}.`);
            }
        }

        // --- Web Headless Automation Tests for Window Shifting and Resizing ---
        console.log('\n--- Running Window Relocation & Resizing Tests ---');

        // First, get the windowId
        const listRes = await fetch(`http://127.0.0.1:${PORT}/json/list`);
        const tabsList = await listRes.json();
        const activeTab = tabsList.find(t => t.type === 'page');
        const targetId = activeTab.id;
        const { windowId } = await sendCommand('Browser.getWindowForTarget', { targetId });
        console.log(`[TestRunner] Got Browser windowId: ${windowId}`);

        // Helper to run a single gaze frame projection and return canvasX, canvasY
        async function getCanvasGazeCoords() {
            const res = await sendCommand('Runtime.evaluate', {
                expression: `window.runGazeTest("self_center.jpg")`,
                awaitPromise: true,
                returnByValue: true
            });
            if (res.exceptionDetails) {
                throw new Error(`Window test frame failed: ${res.exceptionDetails.exception.description}`);
            }
            return {
                canvasX: res.result.value.canvasX,
                canvasY: res.result.value.canvasY
            };
        }

        // Test Case 3: Window Displacement Monotonicity
        console.log('[TestRunner] Test Case 3: Window Displacement Monotonicity');
        
        // Move window to (100, 200)
        console.log('  Moving window to (100, 200)...');
        await sendCommand('Browser.setWindowBounds', { windowId, bounds: { left: 100, top: 200, width: 800, height: 600, windowState: 'normal' } });
        await sleep(1000); // Wait for transition
        const coords1 = await getCanvasGazeCoords();
        console.log(`  At (100, 200): canvasX=${coords1.canvasX}, canvasY=${coords1.canvasY}`);

        // Move window to (250, 100)
        console.log('  Moving window to (250, 100)...');
        await sendCommand('Browser.setWindowBounds', { windowId, bounds: { left: 250, top: 100, width: 800, height: 600, windowState: 'normal' } });
        await sleep(1000); // Wait for transition
        const coords2 = await getCanvasGazeCoords();
        console.log(`  At (250, 100): canvasX=${coords2.canvasX}, canvasY=${coords2.canvasY}`);

        const deltaWinLeft = (250 - 100) * 2.0;
        const deltaWinTop = (100 - 200) * 2.0;
        const deltaCanvasX = coords2.canvasX - coords1.canvasX;
        const deltaCanvasY = coords2.canvasY - coords1.canvasY;

        console.log(`  Asserting X delta: Expected ${deltaWinLeft}, Got ${deltaCanvasX}`);
        console.log(`  Asserting Y delta: Expected ${deltaWinTop}, Got ${deltaCanvasY}`);

        if (Math.abs(deltaCanvasX - deltaWinLeft) > 2) {
            throw new Error(`Assertion Failed: canvasX delta (${deltaCanvasX}) does not match window displacement (${deltaWinLeft})`);
        }
        if (Math.abs(deltaCanvasY - deltaWinTop) > 2) {
            throw new Error(`Assertion Failed: canvasY delta (${deltaCanvasY}) does not match window displacement (${deltaWinTop})`);
        }

        // Test Case 4: Window Resizing Stability
        console.log('[TestRunner] Test Case 4: Window Resizing Stability');
        
        // Resize window to 1024x768 (keeping top-left at (250, 100))
        console.log('  Resizing window to 1024x768...');
        await sendCommand('Browser.setWindowBounds', { windowId, bounds: { left: 250, top: 100, width: 1024, height: 768, windowState: 'normal' } });
        await sleep(1000);
        const coords3 = await getCanvasGazeCoords();
        console.log(`  At 1024x768: canvasX=${coords3.canvasX}, canvasY=${coords3.canvasY}`);

        if (Math.abs(coords3.canvasX - coords2.canvasX) > 2 || Math.abs(coords3.canvasY - coords2.canvasY) > 2) {
            throw new Error(`Assertion Failed: Canvas screen coordinates shifted during window resize: coords2=(${coords2.canvasX}, ${coords2.canvasY}), coords3=(${coords3.canvasX}, ${coords3.canvasY})`);
        }
        console.log('  Canvas screen coordinates remained invariant to window resizing.');

        // Test Case 6: Dynamic Device Pixel Ratio (DPR) Scaling Stability
        console.log('\n--- Running Dynamic DPR Coordinate Scaling Tests ---');
        console.log('[TestRunner] Test Case 6: Dynamic Device Pixel Ratio (DPR) Scaling Stability');
        
        const dprValues = [1.0, 2.0, 3.0];
        // At DPR=2.0, logical coordinates are: logicalX = 250, logicalY = 187 (using coords2 which is at (250,100))
        const logicalX = 250;
        const logicalY = 187;

        for (const dprVal of dprValues) {
            console.log(`  Testing with window.devicePixelRatio = ${dprVal}...`);
            await sendCommand('Runtime.evaluate', {
                expression: `
                    Object.defineProperty(window, 'devicePixelRatio', {
                        get: function() { return ${dprVal}; },
                        configurable: true
                    });
                `
            });
            const coords = await getCanvasGazeCoords();
            console.log(`    At DPR=${dprVal}: canvasX=${coords.canvasX}, canvasY=${coords.canvasY}`);
            
            const expectedX = logicalX * dprVal;
            const expectedY = logicalY * dprVal;

            if (Math.abs(coords.canvasX - expectedX) > 2 || Math.abs(coords.canvasY - expectedY) > 2) {
                throw new Error(`Assertion Failed: Coordinates at DPR=${dprVal} (${coords.canvasX}, ${coords.canvasY}) do not match expected scaling (${expectedX}, ${expectedY})`);
            }
        }
        console.log('  Canvas screen coordinates scaled linearly and correctly with changing devicePixelRatio.');

        // Clean up: Restore DPR to 2.0
        await sendCommand('Runtime.evaluate', {
            expression: `
                Object.defineProperty(window, 'devicePixelRatio', {
                    get: function() { return 2.0; },
                    configurable: true
                });
            `
        });

        // Test Case 5: Sandbox and Fullscreen Fallbacks
        console.log('[TestRunner] Test Case 5: Sandbox and Fullscreen Fallbacks');

        // Evaluate code to mock window screen access restriction (throws SecurityError)
        console.log('  Injecting window.screenX/screenLeft security error simulation...');
        await sendCommand('Runtime.evaluate', {
            expression: `
                Object.defineProperty(window, 'screenX', { get: function() { throw new DOMException("SecurityError: Permission denied", "SecurityError"); } });
                Object.defineProperty(window, 'screenLeft', { get: function() { throw new DOMException("SecurityError: Permission denied", "SecurityError"); } });
                Object.defineProperty(window, 'screenY', { get: function() { throw new DOMException("SecurityError: Permission denied", "SecurityError"); } });
                Object.defineProperty(window, 'screenTop', { get: function() { throw new DOMException("SecurityError: Permission denied", "SecurityError"); } });
            `
        });

        // Run tracking on center image
        const sandboxCoords = await getCanvasGazeCoords();
        console.log(`  Simulated sandbox windowed coordinates: canvasX=${sandboxCoords.canvasX}, canvasY=${sandboxCoords.canvasY}`);
        if (isNaN(sandboxCoords.canvasX) || isNaN(sandboxCoords.canvasY)) {
            throw new Error(`Assertion Failed: Sandbox coordinates are NaN`);
        }

        // Simulate Fullscreen mode
        console.log('  Simulating Fullscreen mode...');
        await sendCommand('Runtime.evaluate', {
            expression: `
                Object.defineProperty(document, 'fullscreenElement', { get: function() { return document.getElementById('testCanvas') || true; }, configurable: true });
            `
        });
        const fullscreenCoords = await getCanvasGazeCoords();
        console.log(`  Simulated sandbox fullscreen coordinates: canvasX=${fullscreenCoords.canvasX}, canvasY=${fullscreenCoords.canvasY}`);
        if (isNaN(fullscreenCoords.canvasX) || isNaN(fullscreenCoords.canvasY)) {
            throw new Error(`Assertion Failed: Fullscreen coordinates are NaN`);
        }

        console.log('[TestRunner] Window relocation, resizing, and sandbox tests passed!');



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
