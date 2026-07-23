#!/usr/bin/env python3
# tests/test_style.py
# Static compliance check to verify no direct calls to UtilityFunctions::print / printerr.

import os
import sys

def assert_c_style(filepath, filename, violations):
    """
    Asserts style and compliance rules on a single source file.
    Appends any found violations to the violations list.
    """
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()
    
    for idx, line in enumerate(lines, 1):
        # Rule 1: No raw UtilityFunctions::print or UtilityFunctions::printerr
        if "UtilityFunctions::print" in line or "UtilityFunctions::printerr" in line:
            stripped = line.strip()
            is_allowed_redirection = (
                filename == "register_types.cpp" and 
                (stripped == "UtilityFunctions::printerr(godot_msg);" or 
                 stripped == "UtilityFunctions::print(godot_msg);")
            )
            if not is_allowed_redirection:
                violations.append((filepath, idx, f"Direct print call: {stripped}"))

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
        print("Style compliance check FAILED! Raw UtilityFunctions print statements found:")
        for filepath, line_num, message in violations:
            print(f"  {filepath}:{line_num} -> {message}")
        print("\nPlease route all logging through Gaze::log_info, Gaze::log_warning, or Gaze::log_error.")
        sys.exit(1)
    
    print("Style compliance check PASSED. No unauthorized raw print statements found.")
    sys.exit(0)

if __name__ == "__main__":
    main()

