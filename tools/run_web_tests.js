#!/usr/bin/env node
// tools/run_web_tests.js
// Runs Godot Web export in a headless browser via CDP and checks console logs for test results.

// TODO: Similar comments to run_sidecar_gaze_tests.js

const { spawn } = require("child_process");
const http = require("http");
const fs = require("fs");
const path = require("path");

const PORT = 9224; // Chrome remote debugging port
const SERVER_PORT = 8209;
const CHROME_PATH =
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const TARGET_URL = `http://localhost:${SERVER_PORT}/index.html?run-tests=true`;

// 1. Start Web Server serving exports/web/
const server = http.createServer((req, res) => {
  let urlPath = decodeURIComponent(req.url.split("?")[0]);
  if (urlPath === "/") {
    urlPath = "/index.html";
  }

  const filePath = path.join(__dirname, "../project/exports/web", urlPath);

  // Enable cross-origin isolation headers for WebAssembly SharedArrayBuffer / threads
  res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
  res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");

  try {
    if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
      if (urlPath.endsWith(".html")) {
        res.setHeader("Content-Type", "text/html");
        let html = fs.readFileSync(filePath, "utf8");
        if (req.url.includes("run-tests=true")) {
          // Do nothing
        }
        console.log(
          `[TestServer] Serving HTML file: ${filePath} (${html.length} chars)`,
        );
        res.end(html);
      } else {
        if (urlPath.endsWith(".js")) {
          res.setHeader("Content-Type", "application/javascript");
        } else if (urlPath.endsWith(".wasm")) {
          res.setHeader("Content-Type", "application/wasm");
        } else {
          res.setHeader("Content-Type", "application/octet-stream");
        }
        const bytes = fs.readFileSync(filePath);
        console.log(
          `[TestServer] Serving binary file: ${filePath} (${bytes.length} bytes)`,
        );
        res.end(bytes);
      }
    } else {
      res.statusCode = 404;
      res.end("404 Not Found");
    }
  } catch (err) {
    res.statusCode = 500;
    res.end("500 Internal Error");
  }
});

server.listen(SERVER_PORT, () => {
  console.log(
    `[TestServer] Serving project/exports/web/ at http://localhost:${SERVER_PORT}/`,
  );
});

// 2. Launch Chrome
console.log("[TestRunner] Launching Google Chrome headlessly...");
const chromeProcess = spawn(CHROME_PATH, [
  "--headless=new",
  `--remote-debugging-port=${PORT}`,
  "--enable-features=SharedArrayBuffer",
  "--js-flags=--experimental-wasm-threads",
  "about:blank",
]);

chromeProcess.on("error", (err) => {
  console.error("[TestRunner] Failed to start Chrome:", err);
  server.close();
  process.exit(1);
});

// Cleanup processes on exit
const cleanup = () => {
  try {
    chromeProcess.kill();
  } catch (e) {}
  try {
    server.close();
  } catch (e) {}
};
process.on("exit", cleanup);
process.on("SIGINT", () => {
  process.exit(0);
});

// Safety timeout: abort tests if they hang
const safetyTimeout = setTimeout(() => {
  console.error(
    "[TestRunner] Error: Safety timeout reached. Web E2E tests took too long and were aborted.",
  );
  cleanup();
  process.exit(1);
}, 30000);
if (typeof safetyTimeout.unref === "function") {
  safetyTimeout.unref();
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function getWsUrl() {
  for (let i = 0; i < 20; i++) {
    try {
      const res = await fetch(`http://127.0.0.1:${PORT}/json/list`);
      const tabs = await res.json();
      const tab = tabs.find((t) => t.type === "page");
      if (tab && tab.webSocketDebuggerUrl) {
        return tab.webSocketDebuggerUrl;
      }
    } catch (e) {}
    await sleep(500);
  }
  throw new Error("Timeout waiting for Chrome Remote Debugging port");
}

async function run() {
  let ws;
  try {
    const wsUrl = await getWsUrl();
    console.log("[TestRunner] Connected to Chrome DevTools Protocol.");

    ws = new globalThis.WebSocket(wsUrl);

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

    let testCompleted = false;
    let testPassed = false;

    ws.onopen = async () => {
      try {
        await sendCommand("Runtime.enable");
        await sendCommand("Page.enable");
        await sendCommand("Network.enable");

        console.log(`[TestRunner] Navigating to ${TARGET_URL}...`);
        await sendCommand("Page.navigate", { url: TARGET_URL });
      } catch (err) {
        console.error("[TestRunner] Failed to initialize page:", err);
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
      } else if (msg.method === "Runtime.consoleAPICalled") {
        const args = msg.params.args
          .map((arg) => arg.value || arg.description || "")
          .join(" ");
        console.log(`[BROWSER CONSOLE] ${args}`);

        if (
          args.includes(
            "ALL Headless Integration & E2E tests have passed successfully!",
          ) ||
          args.includes(
            "ALL Windowed GPU integration tests have passed successfully!",
          ) ||
          args.includes(
            "ALL Headless Integration & E2E tests have passed successfully",
          )
        ) {
          testCompleted = true;
          testPassed = true;
        } else if (args.includes("FAIL:")) {
          testCompleted = true;
          testPassed = false;
        }
      } else if (msg.method === "Runtime.exceptionThrown") {
        const desc =
          msg.params.exceptionDetails.exception.description ||
          msg.params.exceptionDetails.text ||
          "";
        console.error(`[BROWSER EXCEPTION]`, desc);
        if (!desc.includes("AudioWorklet") && !desc.includes("AudioContext")) {
          testCompleted = true;
          testPassed = false;
        }
      } else if (msg.method === "Network.requestWillBeSent") {
        console.log(`[NETWORK REQ] ${msg.params.request.url}`);
      } else if (msg.method === "Network.loadingFailed") {
        console.error(
          `[NETWORK FAIL] ${msg.params.requestId}: ${msg.params.errorText}`,
        );
      }
    };

    // Poll for test completion status
    while (!testCompleted) {
      await sleep(500);
    }

    clearTimeout(safetyTimeout);
    ws.close();
    cleanup();

    if (testPassed) {
      console.log("[TestRunner] Godot Web E2E tests PASSED successfully!");
      process.exit(0);
    } else {
      console.error("[TestRunner] Godot Web E2E tests FAILED!");
      process.exit(1);
    }
  } catch (err) {
    console.error("[TestRunner] Test run failed with error:", err);
    if (ws) ws.close();
    cleanup();
    process.exit(1);
  }
}

run();
