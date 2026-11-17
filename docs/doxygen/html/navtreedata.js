/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Godot Gaze", "index.html", [
    [ "Godot Gaze (<span class=\"tt\">v0.1.0</span>)", "index.html", "index" ],
    [ "System Architecture &amp; Technical Specification", "md_docs_2architecture.html", [
      [ "1. Architectural Topology", "md_docs_2architecture.html#autotoc_md34", null ],
      [ "2. Layer Isolation &amp; Purity Rules", "md_docs_2architecture.html#autotoc_md36", null ],
      [ "3. Concurrency &amp; Threading Model", "md_docs_2architecture.html#autotoc_md38", null ],
      [ "4. Hardware &amp; Memory Model: Why CPU RAM is the Common Denominator", "md_docs_2architecture.html#autotoc_md40", [
        [ "Why Zero-Copy GPU Roundtrips Were Rejected", "md_docs_2architecture.html#autotoc_md41", null ]
      ] ],
      [ "5. WebAssembly &amp; JavaScript Boundary Architecture", "md_docs_2architecture.html#autotoc_md43", null ],
      [ "6. Input Subsystem &amp; Event Architecture", "md_docs_2architecture.html#autotoc_md45", [
        [ "6.1. First-Class Events: <span class=\"tt\">InputEventGazeBase</span>, <span class=\"tt\">InputEventGaze</span>, <span class=\"tt\">InputEventGazeMissing</span>", "md_docs_2architecture.html#autotoc_md46", null ],
        [ "6.2. Pointer Arbitration &amp; Mouse Emulation (<span class=\"tt\">MouseGazeEmulation</span>)", "md_docs_2architecture.html#autotoc_md47", null ],
        [ "6.3. Inertial Dwell Mechanics (<span class=\"tt\">FillAccumulator</span>)", "md_docs_2architecture.html#autotoc_md48", null ]
      ] ],
      [ "7. GazeDisplayServer &amp; Platform Windowing Abstraction", "md_docs_2architecture.html#autotoc_md50", [
        [ "7.1. Architectural Rationale: Why Not Rely on Godot's <span class=\"tt\">DisplayServer</span>?", "md_docs_2architecture.html#autotoc_md51", null ],
        [ "7.2. The Unified Coordinate Contract: Logical Pixels (<span class=\"tt\">lpix</span>)", "md_docs_2architecture.html#autotoc_md52", null ],
        [ "7.3. Platform Geometry Mapping Matrix", "md_docs_2architecture.html#autotoc_md53", null ]
      ] ]
    ] ],
    [ "Gaze Tracking Lifecycle &amp; Reference-Counted Architecture", "md_docs_2lifecycle__tracking.html", [
      [ "1. Overview: The Reference-Counted Lifecycle Model", "md_docs_2lifecycle__tracking.html#autotoc_md56", [
        [ "Why Reference Counting?", "md_docs_2lifecycle__tracking.html#autotoc_md57", null ]
      ] ],
      [ "2. Integration Patterns", "md_docs_2lifecycle__tracking.html#autotoc_md59", [
        [ "Pattern A: Declarative Scene Node (<span class=\"tt\">GazeTracker</span>)", "md_docs_2lifecycle__tracking.html#autotoc_md60", [
          [ "Invariants &amp; Behavior", "md_docs_2lifecycle__tracking.html#autotoc_md61", null ]
        ] ],
        [ "Pattern B: Programmatic GDScript Attachment (<span class=\"tt\">GazeTracker.track_node</span>)", "md_docs_2lifecycle__tracking.html#autotoc_md63", [
          [ "How <span class=\"tt\">track_node()</span> Works", "md_docs_2lifecycle__tracking.html#autotoc_md64", null ]
        ] ]
      ] ],
      [ "3. Direct Server APIs (Advanced)", "md_docs_2lifecycle__tracking.html#autotoc_md66", null ]
    ] ],
    [ "Gaze Tracking Mathematical &amp; Physical Model", "md_docs_2mathematical__model.html", [
      [ "1. Coordinate Systems &amp; Facial Geometry", "md_docs_2mathematical__model.html#autotoc_md69", [
        [ "1.1. Inference Camera Space (OpenCV Standard)", "md_docs_2mathematical__model.html#autotoc_md70", null ],
        [ "1.2. Canonical 35-Point Anthropometric 3D Face Model", "md_docs_2mathematical__model.html#autotoc_md71", null ],
        [ "1.3. Godot Camera Space (Standard Graphics Camera Space)", "md_docs_2mathematical__model.html#autotoc_md72", null ],
        [ "1.4. Head Pose PnP Solvers (SQPnP &amp; Levenberg-Marquardt)", "md_docs_2mathematical__model.html#autotoc_md73", null ]
      ] ]
    ] ],
    [ "Vision Models &amp; Camera Ingestion Specification", "md_docs_2vision__models.html", [
      [ "1. Camera Ingestion Architecture &amp; Color Formats", "md_docs_2vision__models.html#autotoc_md96", [
        [ "1.1. Zero-Allocation Memory Model", "md_docs_2vision__models.html#autotoc_md97", null ],
        [ "1.2. Platform-Specific Camera Ingestion Details", "md_docs_2vision__models.html#autotoc_md98", null ]
      ] ],
      [ "2. Model Catalog &amp; Neural Preprocessing", "md_docs_2vision__models.html#autotoc_md100", [
        [ "2.1. Model Specification Matrix", "md_docs_2vision__models.html#autotoc_md101", null ]
      ] ],
      [ "3. Detailed Model Specifications", "md_docs_2vision__models.html#autotoc_md103", [
        [ "3.1. YuNet Face Detector (<span class=\"tt\">ORTYuNetDetector</span>)", "md_docs_2vision__models.html#autotoc_md104", null ],
        [ "3.2. Intel ADAS 35-Point Facial Landmarker (<span class=\"tt\">ORTLandmarkModel</span>)", "md_docs_2vision__models.html#autotoc_md106", null ],
        [ "3.3. Intel ADAS Gaze Estimation (<span class=\"tt\">ORTGazeModel</span>)", "md_docs_2vision__models.html#autotoc_md108", null ],
        [ "3.4. Open-Closed Eye Classifier (<span class=\"tt\">ORTEyeStateModel</span>)", "md_docs_2vision__models.html#autotoc_md110", null ]
      ] ]
    ] ],
    [ "Gaze Estimation Model Evaluation &amp; Architectural Comparison", "md_docs_2gaze__model__evaluation.html", [
      [ "1. Executive Summary &amp; Benchmark Matrix", "md_docs_2gaze__model__evaluation.html#autotoc_md113", null ],
      [ "2. Deconstructing the 800 FPS vs 12–150 ms Discrepancy", "md_docs_2gaze__model__evaluation.html#autotoc_md115", [
        [ "A. The \"Baking / Upstream Offload\" Illusion", "md_docs_2gaze__model__evaluation.html#autotoc_md116", null ],
        [ "B. Input Spatial Area &amp; Computational Complexity (FLOPs)", "md_docs_2gaze__model__evaluation.html#autotoc_md117", null ]
      ] ],
      [ "3. Peripheral Dynamic Range vs. Automotive Bias", "md_docs_2gaze__model__evaluation.html#autotoc_md119", null ],
      [ "4. Resolution Scaling &amp; Optimization Opportunities", "md_docs_2gaze__model__evaluation.html#autotoc_md121", null ],
      [ "5. Architectural Roadmap: Pluggable Gaze Estimator Strategy", "md_docs_2gaze__model__evaluation.html#autotoc_md123", null ],
      [ "5. Architectural Decision &amp; Future Swappable Strategy", "md_docs_2gaze__model__evaluation.html#autotoc_md124", [
        [ "A. Production Decision: OpenVINO ADAS-0002 Remains Default", "md_docs_2gaze__model__evaluation.html#autotoc_md125", null ],
        [ "B. Future Readiness: Blueprint for <span class=\"tt\">DirectFaceGazeEstimator</span>", "md_docs_2gaze__model__evaluation.html#autotoc_md126", [
          [ "Blueprint for Future Implementation:", "md_docs_2gaze__model__evaluation.html#autotoc_md127", null ]
        ] ]
      ] ]
    ] ],
    [ "Rejected Designs &amp; Anti-Patterns", "md_docs_2rejected__designs.html", [
      [ "1. Frequently Asserted Quibbles &amp; Technical Traps", "md_docs_2rejected__designs.html#autotoc_md130", null ],
      [ "2. Code Style &amp; Integration Guidelines", "md_docs_2rejected__designs.html#autotoc_md132", null ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Functions", "functions_func.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"index.html#autotoc_md14"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';