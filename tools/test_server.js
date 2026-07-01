#!/usr/bin/env node

const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 8000;
const EXPORTS_DIR = path.join(__dirname, '../project/exports');

const MIME_TYPES = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.wasm': 'application/wasm',
    '.pck': 'application/octet-stream',
    '.ico': 'image/x-icon'
};

// 1. Ensure exports directory exists
if (!fs.existsSync(EXPORTS_DIR)) {
    fs.mkdirSync(EXPORTS_DIR, { recursive: true });
}

// 2. Start static server
function startServer() {
    const server = http.createServer((req, res) => {
        // Strip query parameters
        const urlPath = req.url.split('?')[0];
        let filePath = path.join(EXPORTS_DIR, urlPath === '/' ? 'godot-gaze.html' : urlPath);
        
        // Prevent directory traversal
        const relative = path.relative(EXPORTS_DIR, filePath);
        if (relative.startsWith('..') || path.isAbsolute(relative)) {
            res.statusCode = 403;
            res.end('403 Forbidden');
            return;
        }

        fs.stat(filePath, (err, stats) => {
            if (err || !stats.isFile()) {
                res.statusCode = 404;
                res.setHeader('Content-Type', 'text/plain');
                res.end(`404 Not Found: ${urlPath}`);
                return;
            }

            const ext = path.extname(filePath).toLowerCase();
            const contentType = MIME_TYPES[ext] || 'application/octet-stream';

            // Essential COOP / COEP Headers for WebAssembly SharedArrayBuffer / GDExtension
            res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
            res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
            res.setHeader('Access-Control-Allow-Origin', '*');
            res.setHeader('Content-Type', contentType);
            res.setHeader('Cache-Control', 'no-store, no-cache, must-revalidate, proxy-revalidate');
            res.setHeader('Pragma', 'no-cache');
            res.setHeader('Expires', '0');

            const stream = fs.createReadStream(filePath);
            stream.on('error', (streamErr) => {
                console.error(`[DevServer] Error reading file: ${streamErr.message}`);
                if (!res.headersSent) {
                    res.statusCode = 500;
                    res.end('500 Internal Server Error');
                }
            });
            stream.pipe(res);
        });
    });

    server.listen(PORT, () => {
        console.log(`\n======================================================`);
        console.log(`[DevServer] Running at: http://localhost:${PORT}/`);
        console.log(`[DevServer] COOP/COEP isolation headers are ENABLED.`);
        console.log(`======================================================\n`);
    });
}

startServer();

