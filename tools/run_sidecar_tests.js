#!/usr/bin/env node

const { spawn } = require('child_process');
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 9223; // Chrome debugger port
const SERVER_PORT = 8001; // test server port
const CHROME_PATH = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const TARGET_URL = `http://localhost:${SERVER_PORT}/tests/test_sidecar.html`;

// 1. Start the HTTP test server
const server = http.createServer((req, res) => {
    const urlPath = req.url.split('?')[0];
    
    if (urlPath === '/tests/test_sidecar.html') {
        res.setHeader('Content-Type', 'text/html');
        res.end(fs.readFileSync(path.join(__dirname, '../tests/test_sidecar.html')));
    } else if (urlPath === '/src/web/gaze_sidecar.js') {
        res.setHeader('Content-Type', 'application/javascript');
        res.end(fs.readFileSync(path.join(__dirname, '../src/web/gaze_sidecar.js')));
    } else if (urlPath === '/mock_local.js') {
        res.setHeader('Content-Type', 'application/javascript');
        res.end('window.lastLoadedMethod = "local";');
    } else if (urlPath === '/mock_cdn.js') {
        res.setHeader('Content-Type', 'application/javascript');
        res.end('window.lastLoadedMethod = "cdn";');
    } else {
        res.statusCode = 404;
        res.end('404 Not Found');
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

        // Wait for navigation to complete
        await sleep(2000);

        console.log('\n--- Running Test 1: Successful same-origin script load via Blob URL ---');
        // Reset state
        await sendCommand('Runtime.evaluate', { expression: 'window.lastLoadedMethod = undefined;' });
        const res1 = await sendCommand('Runtime.evaluate', {
            expression: "window.runLoaderTest('/mock_local.js', '/mock_cdn.js')",
            awaitPromise: true,
            returnByValue: true
        });
        const result1 = res1.result.value;
        console.log('Result 1:', result1);
        if (!result1.success || result1.method !== 'local') {
            throw new Error(`Test 1 Failed: Expected success with method 'local', got: ${JSON.stringify(result1)}`);
        }
        console.log('Test 1 PASSED!\n');

        console.log('--- Running Test 2: Local fetch fails, falls back to CDN script ---');
        // Reset state
        await sendCommand('Runtime.evaluate', { expression: 'window.lastLoadedMethod = undefined;' });
        const res2 = await sendCommand('Runtime.evaluate', {
            expression: "window.runLoaderTest('/does_not_exist.js', '/mock_cdn.js')",
            awaitPromise: true,
            returnByValue: true
        });
        const result2 = res2.result.value;
        console.log('Result 2:', result2);
        if (!result2.success || result2.method !== 'cdn') {
            throw new Error(`Test 2 Failed: Expected success with method 'cdn' via fallback, got: ${JSON.stringify(result2)}`);
        }
        console.log('Test 2 PASSED!\n');

        console.log('=== ALL INTEGRATION TESTS PASSED! ===');
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
