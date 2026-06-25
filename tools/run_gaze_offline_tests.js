#!/usr/bin/env node
// tools/run_gaze_offline_tests.js
// Standalone zero-dependency test runner that serves project files and runs test_gaze_offline.html in headless Chrome.

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const PORT = 9081;
const TIMEOUT_MS = 60000; // 60 seconds timeout
const BASE_DIR = path.resolve(__dirname, '..');

// Helper to determine Content-Type
function getContentType(filePath) {
    const ext = path.extname(filePath).toLowerCase();
    switch (ext) {
        case '.html': return 'text/html';
        case '.js': return 'application/javascript';
        case '.json': return 'application/json';
        case '.jpg': case '.jpeg': return 'image/jpeg';
        case '.png': return 'image/png';
        case '.wasm': return 'application/wasm';
        case '.onnx': return 'application/octet-stream';
        default: return 'application/octet-stream';
    }
}

// 1. Start HTTP Static File Server
const server = http.createServer((req, res) => {
    // Handle POST report endpoint
    if (req.method === 'POST' && req.url === '/report') {
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            res.writeHead(200, { 'Content-Type': 'text/plain' });
            res.end('OK');
            
            try {
                const report = JSON.parse(body);
                console.log("\n=================== BROWSER TEST LOGS ===================");
                if (report.logs && Array.isArray(report.logs)) {
                    report.logs.forEach(l => console.log(l));
                } else {
                    console.log("No logs reported.");
                }
                console.log("=========================================================\n");
                
                if (report.status === 'SUCCESS') {
                    console.log("PASS: Web offline integration tests succeeded.");
                    shutdown(0);
                } else {
                    console.error(`FAIL: Web offline integration tests failed: ${report.status}`);
                    shutdown(1);
                }
            } catch (err) {
                console.error("FAIL: Error parsing test report:", err.message);
                shutdown(1);
            }
        });
        return;
    }

    // Handle static file requests
    if (req.method === 'GET') {
        let reqPath = decodeURIComponent(req.url.split('?')[0]);
        if (reqPath === '/' || reqPath === '') {
            reqPath = '/tests/test_gaze_offline.html';
        }
        
        const filePath = path.join(BASE_DIR, reqPath);
        
        // Prevent directory traversal attacks
        if (!filePath.startsWith(BASE_DIR)) {
            res.writeHead(403);
            res.end('Forbidden');
            return;
        }

        fs.stat(filePath, (err, stats) => {
            if (err || !stats.isFile()) {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end(`File not found: ${reqPath}`);
                return;
            }

            res.writeHead(200, {
                'Content-Type': getContentType(filePath),
                'Content-Length': stats.size,
                // Add CORP/COEP headers to support WebAssembly threads just in case
                'Cross-Origin-Opener-Policy': 'same-origin',
                'Cross-Origin-Embedder-Policy': 'require-corp'
            });

            const stream = fs.createReadStream(filePath);
            stream.pipe(res);
        });
        return;
    }

    res.writeHead(405);
    res.end('Method Not Allowed');
});

server.listen(PORT, () => {
    console.log(`Test server running at http://localhost:${PORT}/`);
    launchBrowser();
});

// 2. Headless Browser Launcher
let chromeProcess = null;
let watchdog = null;

function launchBrowser() {
    const url = `http://localhost:${PORT}/tests/test_gaze_offline.html`;
    
    // Default Chrome path on macOS
    const chromePath = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
    
    if (!fs.existsSync(chromePath)) {
        console.error(`Error: Google Chrome not found at ${chromePath}. Please install Chrome or update the runner script path.`);
        shutdown(1);
        return;
    }

    console.log(`Spawning headless Chrome to load: ${url}`);
    
    chromeProcess = spawn(chromePath, [
        '--headless=new',
        '--disable-gpu',
        '--no-sandbox',
        '--disable-software-rasterizer',
        '--disable-dev-shm-usage',
        url
    ]);

    chromeProcess.on('error', (err) => {
        console.error("Failed to start Google Chrome process:", err);
        shutdown(1);
    });

    chromeProcess.on('exit', (code) => {
        if (code !== null && code !== 0) {
            console.log(`Google Chrome exited prematurely with code ${code}.`);
        }
    });

    // Set timeout watchdog
    watchdog = setTimeout(() => {
        console.error(`FAIL: Test timed out after ${TIMEOUT_MS / 1000} seconds without reporting back.`);
        shutdown(1);
    }, TIMEOUT_MS);
}

// 3. Graceful Shutdown Helper
function shutdown(exitCode) {
    if (watchdog) {
        clearTimeout(watchdog);
        watchdog = null;
    }
    
    if (chromeProcess) {
        console.log("Terminating Google Chrome process...");
        chromeProcess.kill('SIGKILL');
        chromeProcess = null;
    }

    server.close(() => {
        console.log("Test server stopped. Exiting with code:", exitCode);
        process.exit(exitCode);
    });
}
