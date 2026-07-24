#!/usr/bin/env node
// tools/run_sidecar_gaze_tests.js
// Runs Sidecar Web coordinate extraction and window movement/resizing integration tests via CDP.

const path = require("path");
const { launchWebTestHarness, sleep, SIDECAR_TEST_PORTS } = require("./web_test_harness");

const TARGET_URL = `http://localhost:${SIDECAR_TEST_PORTS.serverPort}/tests/test_sidecar_gaze.html`;

async function run() {
  let harness;
  try {
    harness = await launchWebTestHarness({
      cdpPort: SIDECAR_TEST_PORTS.cdpPort,
      serverPort: SIDECAR_TEST_PORTS.serverPort,
      targetUrl: TARGET_URL,
      timeoutMs: 25000,
      serveFile: (urlPath) => {
        let filePath = "";
        let contentType = "application/octet-stream";

        if (urlPath === "/tests/test_sidecar_gaze.html") {
          filePath = path.join(__dirname, "../tests/test_sidecar_gaze.html");
          contentType = "text/html";
        } else if (urlPath === "/src/web/gaze_sidecar.js") {
          filePath = path.join(__dirname, "../src/web/gaze_sidecar.js");
          contentType = "application/javascript";
        } else if (urlPath.startsWith("/project/")) {
          filePath = path.join(__dirname, "..", urlPath);
          if (urlPath.endsWith(".js")) contentType = "application/javascript";
          else if (urlPath.endsWith(".wasm")) contentType = "application/wasm";
        } else if (urlPath.startsWith("/tests/resources/")) {
          filePath = path.join(__dirname, "..", urlPath);
          contentType = "image/jpeg";
        }

        return { filePath, contentType };
      }
    });

    const { ws, sendCommand, responsePromises } = harness;

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
      } else if (msg.method === "Runtime.exceptionThrown") {
        console.error(
          `[BROWSER EXCEPTION]`,
          msg.params.exceptionDetails.exception.description ||
            msg.params.exceptionDetails.text,
        );
      }
    };

    await sendCommand("Runtime.enable");
    await sendCommand("Page.enable");

    console.log(`[TestRunner] Navigating to ${TARGET_URL}...`);
    await sendCommand("Page.navigate", { url: TARGET_URL });

    console.log("[TestRunner] Waiting for page and models to initialize...");
    await sleep(4000);

    await sendCommand("Runtime.evaluate", {
      expression: "window.initPromise",
      awaitPromise: true,
    });

    const testImages = [
      "self_center.jpg",
      "self_left_left.jpg",
      "self_right_right.jpg",
      "self_top_top.jpg",
      "self_down_down.jpg",
    ];

    const results = {};

    console.log("\n--- Running Sidecar Coordinate Extraction Tests ---");
    for (const imgName of testImages) {
      console.log(`[TestRunner] Testing image: ${imgName}...`);
      const res = await sendCommand("Runtime.evaluate", {
        expression: `window.runGazeTest("${imgName}")`,
        awaitPromise: true,
        returnByValue: true,
      });

      if (res.exceptionDetails) {
        throw new Error(
          `Test image ${imgName} failed with exception: ${res.exceptionDetails.exception.description}`,
        );
      }

      const data = res.result.value;
      results[imgName] = data;

      console.log(`  Face Tracked: ${data.face_detected}`);
      console.log(`  Left Eye Center:  (${data.lex.toFixed(2)}, ${data.ley.toFixed(2)}, ${data.lez.toFixed(2)})`);
      console.log(`  Right Eye Center: (${data.rex.toFixed(2)}, ${data.rey.toFixed(2)}, ${data.rez.toFixed(2)})`);
      console.log(`  Gaze Dir:         (${data.dx.toFixed(4)}, ${data.dy.toFixed(4)}, ${data.dz.toFixed(4)})`);
      console.log(`  Head Translation: (${data.tx.toFixed(2)}, ${data.ty.toFixed(2)}, ${data.tz.toFixed(2)})`);
      console.log(`  Head Rotation:    (${data.rx.toFixed(4)}, ${data.ry.toFixed(4)}, ${data.rz.toFixed(4)})`);

      const expected = await sendCommand("Runtime.evaluate", {
        expression: `window.cppTestData["${imgName}"]`,
        returnByValue: true,
      });
      const exp = expected.result.value;
      const tol = 0.05;

      const diffLex = Math.abs(data.lex - exp.lex);
      const diffLey = Math.abs(data.ley - exp.ley);
      const diffLez = Math.abs(data.lez - exp.lez);
      const diffRex = Math.abs(data.rex - exp.rex);
      const diffRey = Math.abs(data.rey - exp.rey);
      const diffRez = Math.abs(data.rez - exp.rez);
      const diffDx = Math.abs(data.dx - exp.gx);
      const diffDy = Math.abs(data.dy - -exp.gy);
      const diffDz = Math.abs(data.dz - exp.gz);

      console.log(`  [Parity Check] Left Eye Diff:  (${diffLex.toFixed(4)}, ${diffLey.toFixed(4)}, ${diffLez.toFixed(4)})`);
      console.log(`  [Parity Check] Right Eye Diff: (${diffRex.toFixed(4)}, ${diffRey.toFixed(4)}, ${diffRez.toFixed(4)})`);
      console.log(`  [Parity Check] Gaze Dir Diff:  (${diffDx.toFixed(4)}, ${diffDy.toFixed(4)}, ${diffDz.toFixed(4)})`);
      console.log();

      if (
        diffLex > tol || diffLey > tol || diffLez > tol ||
        diffRex > tol || diffRey > tol || diffRez > tol ||
        diffDx > tol || diffDy > tol || diffDz > tol
      ) {
        throw new Error(`Math Parity Assertion Failed for ${imgName}!`);
      }
    }

    console.log("\n--- Running Window Relocation & Resizing Tests ---");
    const listRes = await fetch(`http://127.0.0.1:${SIDECAR_TEST_PORTS.cdpPort}/json/list`);
    const tabsList = await listRes.json();
    const activeTab = tabsList.find((t) => t.type === "page");
    const targetId = activeTab.id;
    const { windowId } = await sendCommand("Browser.getWindowForTarget", { targetId });
    console.log(`[TestRunner] Got Browser windowId: ${windowId}`);

    async function getCanvasGazeCoords() {
      const res = await sendCommand("Runtime.evaluate", {
        expression: `window.runGazeTest("self_center.jpg")`,
        awaitPromise: true,
        returnByValue: true,
      });
      if (res.exceptionDetails) {
        throw new Error(`Window test frame failed: ${res.exceptionDetails.exception.description}`);
      }
      return { canvasX: res.result.value.canvasX, canvasY: res.result.value.canvasY };
    }

    console.log("[TestRunner] Test Case 3: Window Displacement Monotonicity");
    await sendCommand("Browser.setWindowBounds", {
      windowId,
      bounds: { left: 100, top: 200, width: 800, height: 600, windowState: "normal" },
    });
    await sleep(1000);
    const coords1 = await getCanvasGazeCoords();

    await sendCommand("Browser.setWindowBounds", {
      windowId,
      bounds: { left: 250, top: 100, width: 800, height: 600, windowState: "normal" },
    });
    await sleep(1000);
    const coords2 = await getCanvasGazeCoords();

    const deltaWinLeft = (250 - 100) * 2.0;
    const deltaWinTop = (100 - 200) * 2.0;
    const deltaCanvasX = coords2.canvasX - coords1.canvasX;
    const deltaCanvasY = coords2.canvasY - coords1.canvasY;

    if (Math.abs(deltaCanvasX - deltaWinLeft) > 2 || Math.abs(deltaCanvasY - deltaWinTop) > 2) {
      throw new Error(`Assertion Failed: canvas delta does not match window displacement.`);
    }

    console.log("[TestRunner] Test Case 4: Window Resizing Stability");
    await sendCommand("Browser.setWindowBounds", {
      windowId,
      bounds: { left: 250, top: 100, width: 1024, height: 768, windowState: "normal" },
    });
    await sleep(1000);
    const coords3 = await getCanvasGazeCoords();

    if (Math.abs(coords3.canvasX - coords2.canvasX) > 2 || Math.abs(coords3.canvasY - coords2.canvasY) > 2) {
      throw new Error(`Assertion Failed: Canvas screen coordinates shifted during window resize.`);
    }

    console.log("\n--- Running Dynamic DPR Coordinate Scaling Tests ---");
    const dprValues = [1.0, 2.0, 3.0];
    const logicalX = 250;
    const logicalY = 187;

    for (const dprVal of dprValues) {
      await sendCommand("Runtime.evaluate", {
        expression: `Object.defineProperty(window, 'devicePixelRatio', { get: function() { return ${dprVal}; }, configurable: true });`,
      });
      const coords = await getCanvasGazeCoords();
      const expectedX = logicalX * dprVal;
      const expectedY = logicalY * dprVal;

      if (Math.abs(coords.canvasX - expectedX) > 2 || Math.abs(coords.canvasY - expectedY) > 2) {
        throw new Error(`Assertion Failed: Coordinates at DPR=${dprVal} do not match expected scaling.`);
      }
    }

    await sendCommand("Runtime.evaluate", {
      expression: `Object.defineProperty(window, 'devicePixelRatio', { get: function() { return 2.0; }, configurable: true });`,
    });

    console.log("[TestRunner] Test Case 5: Sandbox and Fullscreen Fallbacks");
    await sendCommand("Runtime.evaluate", {
      expression: `
        Object.defineProperty(window, 'screenX', { get: function() { throw new DOMException("SecurityError", "SecurityError"); } });
        Object.defineProperty(window, 'screenLeft', { get: function() { throw new DOMException("SecurityError", "SecurityError"); } });
        Object.defineProperty(window, 'screenY', { get: function() { throw new DOMException("SecurityError", "SecurityError"); } });
        Object.defineProperty(window, 'screenTop', { get: function() { throw new DOMException("SecurityError", "SecurityError"); } });
      `,
    });

    const sandboxCoords = await getCanvasGazeCoords();
    if (isNaN(sandboxCoords.canvasX) || isNaN(sandboxCoords.canvasY)) {
      throw new Error(`Assertion Failed: Sandbox coordinates are NaN`);
    }

    await sendCommand("Runtime.evaluate", {
      expression: `Object.defineProperty(document, 'fullscreenElement', { get: function() { return document.getElementById('testCanvas') || true; }, configurable: true });`,
    });
    const fullscreenCoords = await getCanvasGazeCoords();
    if (isNaN(fullscreenCoords.canvasX) || isNaN(fullscreenCoords.canvasY)) {
      throw new Error(`Assertion Failed: Fullscreen coordinates are NaN`);
    }

    for (const imgName of testImages) {
      if (!results[imgName].face_detected) {
        throw new Error(`Assertion Failed: Face should be detected in ${imgName}`);
      }
    }

    console.log("\n=== ALL SIDECAR COORDINATE TESTS PASSED SUCCESSFULLY! ===");
    harness.cleanup();
    process.exit(0);
  } catch (err) {
    console.error("[TestRunner] Test run failed with error:", err);
    if (harness) harness.cleanup();
    process.exit(1);
  }
}

run();
