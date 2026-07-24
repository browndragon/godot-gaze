#!/usr/bin/env node
// tools/run_web_tests.js
// Runs Godot Web export in a headless browser via CDP and checks console logs for test results.

const path = require("path");
const { launchWebTestHarness, sleep, WEB_TEST_PORTS } = require("./web_test_harness");

const TARGET_URL = `http://localhost:${WEB_TEST_PORTS.serverPort}/index.html?run-tests=true`;

async function run() {
  let harness;
  try {
    harness = await launchWebTestHarness({
      cdpPort: WEB_TEST_PORTS.cdpPort,
      serverPort: WEB_TEST_PORTS.serverPort,
      targetUrl: TARGET_URL,
      timeoutMs: 30000,
      serveFile: (urlPath) => {
        let cleanPath = urlPath === "/" ? "/index.html" : urlPath;
        const filePath = path.join(__dirname, "../project/exports/web", cleanPath);
        let contentType = "application/octet-stream";
        if (cleanPath.endsWith(".html")) contentType = "text/html";
        else if (cleanPath.endsWith(".js")) contentType = "application/javascript";
        else if (cleanPath.endsWith(".wasm")) contentType = "application/wasm";
        return { filePath, contentType };
      }
    });

    const { ws, sendCommand, responsePromises } = harness;
    let testCompleted = false;
    let testPassed = false;

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
          args.includes("ALL Headless Integration & E2E tests have passed successfully!") ||
          args.includes("ALL Windowed GPU integration tests have passed successfully!") ||
          args.includes("ALL Headless Integration & E2E tests have passed successfully")
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
      }
    };

    await sendCommand("Runtime.enable");
    await sendCommand("Page.enable");
    await sendCommand("Network.enable");

    console.log(`[TestRunner] Navigating to ${TARGET_URL}...`);
    await sendCommand("Page.navigate", { url: TARGET_URL });

    while (!testCompleted) {
      await sleep(500);
    }

    harness.cleanup();

    if (testPassed) {
      console.log("[TestRunner] Godot Web E2E tests PASSED successfully!");
      process.exit(0);
    } else {
      console.error("[TestRunner] Godot Web E2E tests FAILED!");
      process.exit(1);
    }
  } catch (err) {
    console.error("[TestRunner] Test run failed with error:", err);
    if (harness) harness.cleanup();
    process.exit(1);
  }
}

run();
