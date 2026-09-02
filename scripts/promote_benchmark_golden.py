#!/usr/bin/env python3
"""
scripts/promote_benchmark_golden.py

Safely promotes candidate benchmark results (build/tests/artifacts/gaze_benchmark_current.json)
to the tracked baseline goldenfile (test_assets/gaze_benchmark_golden.json).

NOTE: This script must ONLY be executed when benchmark changes/regressions have been
explicitly reviewed and approved by human project leadership.
"""

import os
import sys
import shutil
import json

def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    candidate_path = os.path.join(repo_root, "build", "tests", "artifacts", "gaze_benchmark_current.json")
    target_golden = os.path.join(repo_root, "test_assets", "gaze_benchmark_golden.json")

    if not os.path.exists(candidate_path):
        print(f"Error: Candidate benchmark results file not found at: {candidate_path}")
        print("Please run 'scons tests/benchmark' first to produce current candidate results.")
        sys.exit(1)

    with open(candidate_path, "r") as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError as e:
            print(f"Error: Candidate file {candidate_path} is not valid JSON: {e}")
            sys.exit(1)

    os.makedirs(os.path.dirname(target_golden), exist_ok=True)
    with open(target_golden, "w") as f:
        json.dump(data, f, indent=2)

    print(f"Successfully promoted candidate results to golden baseline:\n  -> {target_golden}")
    print(f"Total fixtures recorded: {len(data.keys())}")

if __name__ == "__main__":
    main()
