#!/usr/bin/env node

const { spawn } = require('child_process');
const http = require('http');

const CHROME_PATH = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const PORT = 9222;
const TARGET_URL = 'http://localhost:8000/godot-gaze.html';

console.log('[TestRunner] Launching Google Chrome headlessly...');
const chromeProcess = spawn(CHROME_PATH, [
    '--headless=new',
    `--remote-debugging-port=${PORT}`,
    '--user-data-dir=/Users/acunningham/src/godot-gaze/build/chrome_profile',
    '--use-fake-ui-for-media-stream',
    '--use-fake-device-for-media-stream',
    'about:blank'
]);

chromeProcess.on('error', (err) => {
    console.error('[TestRunner] Failed to start Chrome:', err);
    process.exit(1);
});

// Safe cleanup on exit
process.on('exit', () => {
    try { chromeProcess.kill(); } catch (e) {}
});
process.on('SIGINT', () => {
    process.exit();
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
        } catch (e) {
            // Chrome might not be ready yet
        }
        await sleep(500);
    }
    throw new Error('Timeout waiting for Chrome Remote Debugging port');
}

async function run() {
    try {
        const wsUrl = await getWsUrl();
        console.log('[TestRunner] Connected to Chrome DevTools Protocol at:', wsUrl);

        const ws = new WebSocket(wsUrl);

        ws.onopen = () => {
            console.log('[TestRunner] WebSocket connection established.');
            // Enable Page, Runtime and Console events
            ws.send(JSON.stringify({ id: 1, method: 'Runtime.enable' }));
            ws.send(JSON.stringify({ id: 2, method: 'Page.enable' }));
            
            // Navigate to our target page
            console.log(`[TestRunner] Navigating to ${TARGET_URL}...`);
            ws.send(JSON.stringify({
                id: 3,
                method: 'Page.navigate',
                params: { url: TARGET_URL }
            }));
        };

        ws.onmessage = (event) => {
            const msg = JSON.parse(event.data);
            
            if (msg.method === 'Runtime.consoleAPICalled') {
                const type = msg.params.type;
                const args = msg.params.args.map(arg => {
                    if (arg.value !== undefined) return arg.value;
                    if (arg.description !== undefined) return arg.description;
                    return JSON.stringify(arg);
                }).join(' ');
                console.log(`[BROWSER CONSOLE] [${type.toUpperCase()}] ${args}`);
            } else if (msg.method === 'Runtime.exceptionThrown') {
                console.error(`[BROWSER EXCEPTION]`, msg.params.exceptionDetails.exception.description);
            }
        };

        ws.onerror = (err) => {
            console.error('[TestRunner] WebSocket error:', err);
        };

        // Let the test run for 15 seconds to observe model loading and camera initialization
        console.log('[TestRunner] Monitoring page logs for 15 seconds...');
        await sleep(15000);

        console.log('[TestRunner] Test duration completed. Cleaning up...');
        ws.close();
        chromeProcess.kill();
        process.exit(0);

    } catch (err) {
        console.error('[TestRunner] Test run failed:', err);
        try { chromeProcess.kill(); } catch (e) {}
        process.exit(1);
    }
}

run();
