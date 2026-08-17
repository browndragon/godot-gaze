#!/usr/bin/env python3
"""
tools/benchmark_analyzer.py

Rigorous benchmark delta analyzer for godot-gaze test benchmarks.
Computes line-by-line delta errors:
    Delta Error = Current_Error - Golden_Error
    - Delta Error < 0: IMPROVEMENT (Error reduced)
    - Delta Error > 0: REGRESSION  (Error increased)
    - Delta Error = 0: UNCHANGED
"""

import sys
import os
import argparse
import numpy as np

# Physical targets for the 9 benchmark image test cases
BENCHMARK_TARGETS = {
    "self_center.jpg": {"nose_target": np.array([0.0, 0.0]), "gaze_target": np.array([0.0, 0.0])},
    "self_left_left.jpg": {"nose_target": np.array([-150.75, 0.0]), "gaze_target": np.array([-150.75, 0.0])},
    "self_right_right.jpg": {"nose_target": np.array([150.75, 0.0]), "gaze_target": np.array([150.75, 0.0])},
    "self_top_top.jpg": {"nose_target": np.array([0.0, -94.25]), "gaze_target": np.array([0.0, -94.25])},
    "self_down_down.jpg": {"nose_target": np.array([0.0, 94.25]), "gaze_target": np.array([0.0, 94.25])},
    "self_nosedown_eyesup.jpg": {"nose_target": np.array([0.0, 94.25]), "gaze_target": np.array([0.0, -94.25])},
    "self_noseleft_eyesright.jpg": {"nose_target": np.array([-150.75, 0.0]), "gaze_target": np.array([150.75, 0.0])},
    "self_noseright_eyesleft.jpg": {"nose_target": np.array([150.75, 0.0]), "gaze_target": np.array([-150.75, 0.0])},
    "self_nosetop_eyesdown.jpg": {"nose_target": np.array([0.0, -94.25]), "gaze_target": np.array([0.0, 94.25])}
}

def parse_report(filepath):
    data = {}
    if not os.path.exists(filepath):
        return data

    with open(filepath, 'r') as f:
        for line in f:
            if line.startswith('|') and 'Image File' not in line and '---' not in line:
                parts = [p.strip() for p in line.split('|')[1:-1]]
                if len(parts) >= 6:
                    img, prop, val_str, err_str, prev_err_str, delta_str = parts[:6]
                    data[(img, prop)] = {
                        'val_str': val_str,
                        'err_str': err_str,
                        'prev_err_str': prev_err_str,
                        'delta_str': delta_str
                    }
    return data

def parse_vector(val_str):
    if 'N/A' in val_str or '-9999' in val_str:
        return None
    cleaned = val_str.replace('(', '').replace(')', '').replace('mm', '').replace('deg', '').strip()
    parts = [float(x) for x in cleaned.split(',') if x.strip()]
    return np.array(parts)

def compute_metric_error(img, prop, val_str, err_str):
    vec = parse_vector(val_str)
    if vec is None:
        return 9999.0  # Sentinel for invalid/failed estimation

    if prop in ('gaze_mm', 'nose_mm'):
        target_key = 'gaze_target' if prop == 'gaze_mm' else 'nose_target'
        target = BENCHMARK_TARGETS.get(img, {}).get(target_key)
        if target is not None and len(vec) >= 2:
            return float(np.linalg.norm(vec[:2] - target))
        return 9999.0
    elif prop == 'head_rot_deg':
        err_vec = parse_vector(err_str)
        if err_vec is not None:
            return float(np.linalg.norm(err_vec))
        return 0.0
    elif prop == 'head_pos_mm':
        return 0.0

    return 0.0

def analyze_benchmark(golden_path, current_path):
    # Fallback to project/ build path if top-level path does not exist
    if not os.path.exists(current_path) and os.path.exists(os.path.join("project", current_path)):
        current_path = os.path.join("project", current_path)

    golden_data = parse_report(golden_path)
    current_data = parse_report(current_path)

    if not golden_data:
        print(f"Error: Golden baseline file not found at '{golden_path}'")
        return 1
    if not current_data:
        print(f"Error: Current report file not found at '{current_path}'")
        return 1

    print("========================================================================================================================")
    print("                                      BENCHMARK ERROR DELTA ANALYSIS REPORT                                            ")
    print("========================================================================================================================")
    print(f"Golden Baseline : {golden_path}")
    print(f"Current Output  : {current_path}")
    print("Delta Convention : Negative (-) = Error Reduction (IMPROVEMENT) | Positive (+) = Error Increase (REGRESSION)")
    print("------------------------------------------------------------------------------------------------------------------------")
    print(f"{'Image File':<28} | {'Property':<12} | {'Golden Error':<14} | {'Current Error':<14} | {'Delta Error':<16} | {'Status'}")
    print("------------------------------------------------------------------------------------------------------------------------")

    improvements = []
    regressions = []
    unchanged = []

    for key in sorted(golden_data.keys()):
        img, prop = key
        g_row = golden_data[key]
        c_row = current_data.get(key)

        if not c_row:
            continue

        g_err = compute_metric_error(img, prop, g_row['val_str'], g_row['err_str'])
        c_err = compute_metric_error(img, prop, c_row['val_str'], c_row['err_str'])

        delta_err = c_err - g_err

        if abs(delta_err) < 1e-2:
            status = "UNCHANGED"
            delta_fmt = "  0.00"
            unchanged.append(key)
        elif delta_err < 0:
            status = "IMPROVED (-)"
            delta_fmt = f"{delta_err:-.2f} mm/deg"
            improvements.append((key, delta_err))
        else:
            status = "REGRESSED (+)"
            delta_fmt = f"+{delta_err:.2f} mm/deg"
            regressions.append((key, delta_err))

        g_err_fmt = "FAILED" if g_err >= 9900 else f"{g_err:.2f}"
        c_err_fmt = "FAILED" if c_err >= 9900 else f"{c_err:.2f}"

        print(f"{img:<28} | {prop:<12} | {g_err_fmt:<14} | {c_err_fmt:<14} | {delta_fmt:<16} | {status}")

    print("------------------------------------------------------------------------------------------------------------------------")
    print(f"SUMMARY: {len(improvements)} Metrics Improved, {len(regressions)} Metrics Regressed, {len(unchanged)} Metrics Unchanged.")
    print("========================================================================================================================")

    return 0

def main():
    parser = argparse.ArgumentParser(description="Analyze benchmark error deltas between golden and current run.")
    parser.add_argument("--golden", default="test_assets/gaze_benchmark_report.md", help="Path to golden baseline report")
    parser.add_argument("--current", default="build/tests/artifacts/gaze_benchmark_report.md", help="Path to current benchmark report")
    args = parser.parse_args()

    sys.exit(analyze_benchmark(args.golden, args.current))

if __name__ == "__main__":
    main()
