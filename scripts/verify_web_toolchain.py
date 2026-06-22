#!/usr/bin/env python3
import subprocess
import os
import sys
import re

# Static mapping fallback for known official Godot version -> Emscripten version
GODOT_EMSDK_MAP = {
    "4.3": "3.1.64",
    "4.4": "3.1.64",
    "4.5": "3.1.64",
    "4.7": "4.0.20"
}

def get_active_emcc_version():
    # Prepend emsdk bin directory if using asdf to bypass shim resolution issues
    try:
        emsdk_path = subprocess.check_output(["asdf", "where", "emsdk"], text=True).strip()
        emscripten_bin = os.path.join(emsdk_path, "upstream", "emscripten")
        if os.path.isdir(emscripten_bin):
            os.environ["PATH"] = emscripten_bin + os.pathsep + os.environ.get("PATH", "")
    except Exception:
        pass

    try:
        output = subprocess.check_output(["emcc", "--version"]).decode("utf-8")
        match = re.search(r"emcc .*? ([0-9\.]+)", output)
        if match:
            return match.group(1)
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return None

def get_templates_dir():
    if sys.platform == "win32":
        return os.path.join(os.environ.get("APPDATA", ""), "Godot", "export_templates")
    elif sys.platform == "darwin":
        return os.path.expanduser("~/Library/Application Support/Godot/export_templates")
    else: # Linux/BSD
        return os.path.expanduser("~/.local/share/godot/export_templates")

def get_godot_version():
    # 1. Try running godot from PATH
    for cmd in ["godot", "godot4"]:
        try:
            output = subprocess.check_output([cmd, "--version"]).decode("utf-8").strip()
            match = re.match(r"^([0-9]+\.[0-9]+)", output)
            if match:
                return match.group(1), f"from active '{cmd}' command"
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
            
    # 2. Try scanning local export templates folder
    templates_dir = get_templates_dir()
    if os.path.isdir(templates_dir):
        versions = []
        for d in os.listdir(templates_dir):
            match = re.match(r"^([0-9]+\.[0-9]+)", d)
            if match:
                versions.append(match.group(1))
        if versions:
            versions.sort(key=lambda s: [int(x) for x in s.split('.')])
            return versions[-1], "detected in Godot export_templates folder"
            
    return None, None

def get_emscripten_version_from_template(godot_version_str):
    templates_dir = get_templates_dir()
    if not os.path.isdir(templates_dir):
        return None, None
        
    subdirs = [d for d in os.listdir(templates_dir) if godot_version_str in d or d.startswith(godot_version_str)]
    # Sort in reverse order (e.g. '4.7.stable' before '4.7.rc1')
    subdirs.sort(reverse=True)
        
    for subdir in subdirs:
        # Check both dlink (GDExtension-enabled) and standard templates
        zip_names = ["web_dlink_debug.zip", "web_debug.zip", "web_dlink_release.zip", "web_release.zip"]
        for zip_name in zip_names:
            zip_path = os.path.join(templates_dir, subdir, zip_name)
            if os.path.isfile(zip_path):
                try:
                    import zipfile
                    with zipfile.ZipFile(zip_path, 'r') as z:
                        if "godot.js" in z.namelist():
                            content = z.read("godot.js").decode("utf-8", errors="ignore")
                            # Emscripten dynamic get_version function holds the version literal
                            m = re.search(r'_godot_js_emscripten_get_version.*?allocString\(\x22([0-9\.]+)\x22\)', content, re.DOTALL)
                            if m:
                                return m.group(1), zip_path
                except Exception:
                    pass
    return None, None

def main():
    print("=== Godot Web GDExtension Toolchain Verification ===")
    
    emcc_ver = get_active_emcc_version()
    godot_ver, source = get_godot_version()
    
    if not emcc_ver:
        print("\033[91m[ERROR] Emscripten compiler (emcc) not found in PATH!\033[0m")
        print("Please install the Emscripten SDK and add it to your environment/asdf.")
        sys.exit(1)
        
    print(f"Active Emscripten (emcc): {emcc_ver}")
    
    if not godot_ver:
        print("\033[93m[WARNING] Could not automatically detect Godot Engine version.\033[0m")
        print("Defaulting check to Godot 4.7 target...")
        godot_ver = "4.7"
    else:
        print(f"Target Godot Version:     {godot_ver} ({source})")
        
    # Attempt dynamic version check from the actual templates first
    expected_emcc, template_file = get_emscripten_version_from_template(godot_ver)
    if expected_emcc:
        print(f"Expected Emscripten:      {expected_emcc} (extracted from {os.path.basename(template_file)})")
    else:
        # Fallback to static mapping
        expected_emcc = GODOT_EMSDK_MAP.get(godot_ver)
        if not expected_emcc:
            expected_emcc = "4.0.20"
            print(f"[INFO] No explicit Emscripten version mapped. Defaulting check to {expected_emcc}")
        else:
            print(f"Expected Emscripten:      {expected_emcc} (static fallback mapping)")
            
    if emcc_ver != expected_emcc:
        print("\n\033[91m[WARNING] Emscripten Version Mismatch Detected!\033[0m")
        print(f"Your active emcc version is {emcc_ver}, but Godot {godot_ver} templates require {expected_emcc}.")
        print("Building with a mismatched version WILL cause memory corruption (like missing/shifted string names) inside Web/HTML5 exports.")
        print("\nFix using asdf:")
        print("  1. Update '.tool-versions' with the correct version:")
        print(f"     emsdk {expected_emcc}")
        print("  2. Run: asdf install")
        print("  3. Rebuild both godot-cpp and Gaze GDExtension clean:")
        print("     # Clean previous builds:")
        print("     scons platform=javascript target=template_debug --clean")
        print("     scons platform=javascript target=template_release --clean")
        print("     cd thirdparty/godot-cpp")
        print("     scons platform=web target=template_debug --clean")
        print("     scons platform=web target=template_release --clean")
        print("     # Recompile bindings:")
        print("     scons platform=web target=template_debug -j4")
        print("     scons platform=web target=template_release -j4")
        print("     cd ../..")
        print("     # Recompile extension:")
        print("     scons platform=javascript target=template_debug -j4")
        print("     scons platform=javascript target=template_release -j4")
        sys.exit(2)
    else:
        print("\n\033[92m[SUCCESS] Toolchain version matches! You are ready to build for Web.\033[0m")
        sys.exit(0)

if __name__ == '__main__':
    main()
