// tools/web_test_harness.js
// Shared test harness module for serving web assets and automating headless Chrome via CDP.

const { spawn } = require("child_process");
const http = require("http");
const fs = require("fs");
const path = require("path");
const os = require("os");

const WEB_TEST_PORTS = {
  cdpPort: 9224,
  serverPort: 8209,
};

const SIDECAR_TEST_PORTS = {
  cdpPort: 9225,
  serverPort: 8210,
};

function findChrome() {
  if (process.env.CHROME_PATH && fs.existsSync(process.env.CHROME_PATH)) {
    return process.env.CHROME_PATH;
  }
  const candidates = [
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "/Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary",
    "/Applications/Chromium.app/Contents/MacOS/Chromium",
    path.join(process.env.HOME || "", "Applications/Google Chrome.app/Contents/MacOS/Google Chrome"),
    "/usr/bin/google-chrome",
    "/usr/bin/chromium",
    "/usr/bin/chromium-browser"
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) return c;
  }
  return "google-chrome";
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function launchWebTestHarness(config) {
  const {
    cdpPort = WEB_TEST_PORTS.cdpPort,
    serverPort = WEB_TEST_PORTS.serverPort,
    serveFile, // function(urlPath, req) -> { filePath, contentType } or null
    targetUrl,
    timeoutMs = 30000
  } = config;

  const chromePath = findChrome();
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "godot-gaze-chrome-"));

  // 1. Static HTTP Server with Cross-Origin Isolation headers for WebAssembly
  const server = http.createServer((req, res) => {
    res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
    res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");

    const urlPath = decodeURIComponent(req.url.split("?")[0]);
    const fileInfo = serveFile(urlPath, req);

    if (!fileInfo || !fileInfo.filePath || !fs.existsSync(fileInfo.filePath)) {
      res.statusCode = 404;
      return res.end("404 Not Found");
    }

    try {
      if (fileInfo.contentType) {
        res.setHeader("Content-Type", fileInfo.contentType);
      }
      const data = fs.readFileSync(fileInfo.filePath);
      res.end(data);
    } catch (err) {
      res.statusCode = 500;
      res.end("500 Internal Error");
    }
  });

  server.on("error", (err) => {
    console.error(`[TestServer] Error: ${err.message}`);
    process.exit(1);
  });

  await new Promise((resolve) => server.listen(serverPort, resolve));
  console.log(`[TestServer] Serving test assets at http://localhost:${serverPort}/`);

  // 2. Launch Chrome
  console.log(`[TestRunner] Launching Chrome (${chromePath}) headlessly...`);
  const chromeProcess = spawn(chromePath, [
    "--headless=new",
    `--user-data-dir=${tmpDir}`,
    `--remote-debugging-port=${cdpPort}`,
    "--enable-features=SharedArrayBuffer",
    "--js-flags=--experimental-wasm-threads",
    "about:blank",
  ]);

  const cleanup = () => {
    try { chromeProcess.kill(); } catch (e) {}
    try { server.close(); } catch (e) {}
    try { fs.rmSync(tmpDir, { recursive: true, force: true }); } catch (e) {}
  };

  chromeProcess.on("error", (err) => {
    console.error("[TestRunner] Failed to start Chrome:", err);
    cleanup();
    process.exit(1);
  });

  process.on("exit", cleanup);
  process.on("SIGINT", () => process.exit(0));

  const safetyTimeout = setTimeout(() => {
    console.error(`[TestRunner] Error: Safety timeout reached (${timeoutMs}ms). Aborting.`);
    cleanup();
    process.exit(1);
  }, timeoutMs);
  if (typeof safetyTimeout.unref === "function") safetyTimeout.unref();

  // 3. Connect to Chrome CDP
  let wsUrl = null;
  for (let i = 0; i < 20; i++) {
    try {
      const res = await fetch(`http://127.0.0.1:${cdpPort}/json/list`);
      const tabs = await res.json();
      const tab = tabs.find((t) => t.type === "page");
      if (tab && tab.webSocketDebuggerUrl) {
        wsUrl = tab.webSocketDebuggerUrl;
        break;
      }
    } catch (e) {}
    await sleep(500);
  }

  if (!wsUrl) {
    cleanup();
    throw new Error("Timeout waiting for Chrome Remote Debugging port");
  }

  const ws = new globalThis.WebSocket(wsUrl);
  let messageId = 1;
  const responsePromises = {};

  function sendCommand(method, params = {}) {
    const id = messageId++;
    const payload = JSON.stringify({ id, method, params });
    ws.send(payload);
    return new Promise((resolve, reject) => {
      responsePromises[id] = { resolve, reject };
    });
  }

  await new Promise((resolve, reject) => {
    ws.onopen = resolve;
    ws.onerror = reject;
  });

  console.log("[TestRunner] Connected to Chrome DevTools Protocol.");

  return {
    ws,
    sendCommand,
    responsePromises,
    cdpPort,
    serverPort,
    targetUrl,
    cleanup: () => {
      clearTimeout(safetyTimeout);
      try { ws.close(); } catch (e) {}
      cleanup();
    }
  };
}

module.exports = {
  launchWebTestHarness,
  findChrome,
  sleep,
  WEB_TEST_PORTS,
  SIDECAR_TEST_PORTS
};
