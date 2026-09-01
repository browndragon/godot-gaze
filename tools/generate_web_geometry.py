#!/usr/bin/env python3
"""
tools/generate_web_geometry.py
Generates src/web/face_model_geometry.js from canonical C++ definitions in src/core/math_defs.hpp.
Ensures zero coordinate drift between Native C++ and Web JS runtime.
"""

import os
import re

def parse_canonical_35pt_model(header_path):
    with open(header_path, "r", encoding="utf-8") as f:
        content = f.read()

    match = re.search(r"get_canonical_35pt_face_model\(\)\s*\{([\s\S]*?)\n\s*return pts;", content)
    if not match:
        raise ValueError("Could not find get_canonical_35pt_face_model in " + header_path)

    body = match.group(1)
    pts = []
    
    point_re = re.compile(r"pts\[\s*(\d+)\s*\]\s*=\s*GazeVector3\(\s*([-\d\.\+eEf]+)f?\s*,\s*([-\d\.\+eEf]+)f?\s*,\s*([-\d\.\+eEf]+)f?\s*\)")
    for line in body.split("\n"):
        pm = point_re.search(line)
        if pm:
            idx = int(pm.group(1))
            x = float(pm.group(2).rstrip("f"))
            y = float(pm.group(3).rstrip("f"))
            z = float(pm.group(4).rstrip("f"))
            pts.append((idx, x, y, z))

    jaw_x_m = re.search(r"float jaw_x\[\]\s*=\s*\{([^}]+)\};", body)
    jaw_y_m = re.search(r"float jaw_y\[\]\s*=\s*\{([^}]+)\};", body)
    jaw_z_m = re.search(r"float jaw_z\[\]\s*=\s*\{([^}]+)\};", body)

    if jaw_x_m and jaw_y_m and jaw_z_m:
        jx = [float(v.strip().rstrip("f")) for v in jaw_x_m.group(1).split(",")]
        jy = [float(v.strip().rstrip("f")) for v in jaw_y_m.group(1).split(",")]
        jz = [float(v.strip().rstrip("f")) for v in jaw_z_m.group(1).split(",")]
        for i in range(17):
            pts.append((18 + i, jx[i], jy[i], jz[i]))

    pts.sort(key=lambda p: p[0])
    if len(pts) != 35:
        raise ValueError(f"Expected 35 points, but extracted {len(pts)}")

    return [(p[1], p[2], p[3]) for p in pts]

def generate_js_geometry(pts_35, output_path):
    p0 = pts_35[5]
    p1 = ((pts_35[0][0] + pts_35[1][0]) * 0.5, (pts_35[0][1] + pts_35[1][1]) * 0.5, (pts_35[0][2] + pts_35[1][2]) * 0.5)
    p2 = ((pts_35[2][0] + pts_35[3][0]) * 0.5, (pts_35[2][1] + pts_35[3][1]) * 0.5, (pts_35[2][2] + pts_35[3][2]) * 0.5)
    p3 = pts_35[8]
    p4 = pts_35[9]
    pts_5 = [p0, p1, p2, p3, p4]

    lines = [
        "// AUTO-GENERATED from src/core/math_defs.hpp by tools/generate_web_geometry.py",
        "// DO NOT EDIT MANUALLY. Single Source of Truth is src/core/face_model_geometry.hpp / math_defs.hpp.",
        "",
        "(function (root, factory) {",
        "  if (typeof module === 'object' && module.exports) {",
        "    module.exports = factory();",
        "  } else {",
        "    root.FaceModelGeometry = factory();",
        "  }",
        "}(typeof self !== 'undefined' ? self : this, function () {",
        "  return {",
        "    DEFAULT_HFOV_DEG: 65.0,",
        "    DEFAULT_FOCAL_TAN_HALF: 0.6370702608, // tan(65 deg / 2)",
        "    calculateDefaultFocalLength: function (width) {",
        "      return width / (2.0 * 0.6370702608);",
        "    },",
        "    // Canonical 35-point OpenVINO ADAS face model (Nose Tip Pt 5 is Origin (0,0,0))",
        "    MODEL_POINTS_35: ["
    ]

    for i, (x, y, z) in enumerate(pts_35):
        lines.append(f"      [{x:7.1f}, {y:7.1f}, {z:7.1f}], // Pt {i}")

    lines.extend([
        "    ],",
        "    // Canonical 5-point YuNet alignment model (Pt 0 is Nose Tip Origin (0,0,0))",
        "    MODEL_POINTS_5: ["
    ])

    for i, (x, y, z) in enumerate(pts_5):
        lines.append(f"      [{x:7.1f}, {y:7.1f}, {z:7.1f}], // Pt {i}")

    lines.extend([
        "    ]",
        "  };",
        "});",
        ""
    ])

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"[generate_web_geometry] Generated {output_path} with 35 points from {pts_35[5]} origin.")

def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    header_path = os.path.join(root, "src", "core", "math_defs.hpp")
    output_path = os.path.join(root, "src", "web", "face_model_geometry.js")
    pts_35 = parse_canonical_35pt_model(header_path)
    generate_js_geometry(pts_35, output_path)

if __name__ == "__main__":
    main()
