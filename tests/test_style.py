#!/usr/bin/env python3
# tests/test_style.py
# Static compliance check to verify no direct calls to UtilityFunctions::print / printerr.

import os
import sys
import re

def assert_c_style(filepath, filename, violations):
    """
    Asserts style and compliance rules on a single source file.
    Appends any found violations to the violations list.
    """
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()
    
    for idx, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*") or stripped.startswith("/*"):
            continue

        # Rule 1: No raw UtilityFunctions::print or UtilityFunctions::printerr
        if "UtilityFunctions::print" in line or "UtilityFunctions::printerr" in line:
            is_allowed_redirection = (
                filename == "register_types.cpp" and 
                (stripped == "UtilityFunctions::printerr(godot_msg);" or 
                 stripped == "UtilityFunctions::print(godot_msg);")
            )
            if not is_allowed_redirection:
                violations.append((filepath, idx, f"Direct print call: {stripped}"))

        # Rule 2: No direct DisplayServer usage in gaze-math critical code.
        # Only gaze_display_server, register_types, and mock_gaze_display_server may interact with Godot's DisplayServer.
        if re.search(r"(?<!Gaze)DisplayServer", line) or "<godot_cpp/classes/display_server.hpp>" in line:
            allowed_files = (
                "gaze_display_server.cpp", "gaze_display_server.hpp",
                "register_types.cpp", "register_types.hpp",
                "mock_gaze_display_server.cpp", "mock_gaze_display_server.hpp"
            )
            if filename not in allowed_files:
                violations.append((filepath, idx, f"Direct DisplayServer reference (use GazeDisplayServer instead): {stripped}"))

        # Rule 3: Prohibit multiplying window/mouse coordinates by scale in src/godot
        if ("* scale" in line or "* screen_scale" in line) and any(kw in line for kw in ("win_pos", "mouse_pos", "local_pos")):
            violations.append((filepath, idx, f"Coordinate multiplication by scale anti-pattern: {stripped}"))

def main():
    src_dirs = ["src/godot", "src/web"]
    violations = []

    for src_dir in src_dirs:
        if not os.path.exists(src_dir):
            continue
        for root, _, files in os.walk(src_dir):
            for file in files:
                if file.endswith((".cpp", ".hpp", ".h", ".mm")):
                    filepath = os.path.join(root, file)
                    assert_c_style(filepath, file, violations)

    if violations:
        print("Style compliance check FAILED! Code style violations found:")
        for filepath, line_num, message in violations:
            print(f"  {filepath}:{line_num} -> {message}")
        print("\nPlease fix the above style and architectural violations before committing.")
        sys.exit(1)
    
    print("Style compliance check PASSED. No unauthorized raw print statements found.")
    sys.exit(0)

if __name__ == "__main__":
    main()

