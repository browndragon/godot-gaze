#!/usr/bin/env node

const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 8000;
const EXPORTS_DIR = path.join(__dirname, '../project/exports');
const OPENCV_DEST = path.join(EXPORTS_DIR, 'opencv.js');
const OPENCV_URL = 'https://cdn.jsdelivr.net/npm/@techstark/opencv-js@4.9.0-release.2/dist/opencv.js';

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

// 2. Download opencv.js if missing
function downloadOpenCV(callback) {
    if (fs.existsSync(OPENCV_DEST)) {
        console.log(`[DevServer] Found opencv.js at ${OPENCV_DEST}`);
        return callback();
    }

    console.log(`[DevServer] Downloading opencv.js from ${OPENCV_URL}...`);
    console.log(`[DevServer] Saving to: ${OPENCV_DEST}`);
    
    const file = fs.createWriteStream(OPENCV_DEST);
    https.get(OPENCV_URL, (response) => {
        if (response.statusCode !== 200) {
            console.error(`[DevServer] Error downloading: Status Code ${response.statusCode}`);
            fs.unlinkSync(OPENCV_DEST);
            process.exit(1);
        }
        
        response.pipe(file);
        
        file.on('finish', () => {
            file.close();
            console.log('[DevServer] opencv.js download completed successfully.');
            callback();
        });
    }).on('error', (err) => {
        try { fs.unlinkSync(OPENCV_DEST); } catch (e) {}
        console.error(`[DevServer] Failed to download opencv.js: ${err.message}`);
        process.exit(1);
    });
}

// 3. Start static server
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

downloadOpenCV(startServer);
