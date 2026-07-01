# scripts/conftest.py
# Pytest configurations and options for godot-gaze test suite.

def pytest_addoption(parser):
    parser.addoption("--no-web", action="store_true", help="Skip Node.js Web Sidecar tests")
    parser.addoption("--no-godot", action="store_true", help="Skip Godot headless/windowed tests")
