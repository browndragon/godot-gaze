# SConstruct
# SCons build configuration for the godot-gaze GDExtension library.

import os
import sys

# Define target options
opts = Variables()
opts.Add(EnumVariable("platform", "Target platform", sys.platform, allowed_values=["windows", "linux", "macos", "android", "ios", "javascript"]))
opts.Add(EnumVariable("target", "Compilation target", "template_debug", allowed_values=["template_debug", "template_release", "editor"]))
opts.Add(BoolVariable("use_llvm", "Use LLVM/Clang compiler", False))
opts.Add(PathVariable("opencv_dir", "Path to OpenCV SDK root", os.environ.get("OPENCV_DIR", ""), PathVariable.PathAccept))

env = Environment(variables=opts)

# Set build target directory
env.Append(CPPPATH=["src/core", "thirdparty/one_euro_filter"])

# Detect compilers and setup platform-specific flags
if env["platform"] == "macos":
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC"])
    if env["use_llvm"]:
        env.Replace(CC="clang", CXX="clang++")
elif env["platform"] == "windows":
    env.Append(CCFLAGS=["/std:c++17", "/O2", "/EHsc"])
elif env["platform"] == "javascript":
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-s", "SIDE_MODULE=1"])
else:  # Linux/Android/iOS
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC"])

# Godot cpp bindings setup (assuming godot-cpp is present as submodule/directory)
godot_cpp_dir = os.environ.get("GODOT_CPP_DIR", "godot-cpp")
env.Append(CPPPATH=[
    godot_cpp_dir + "/include",
    godot_cpp_dir + "/gen/include",
    godot_cpp_dir + "/gdextension"
])

# Setup platform-specific library paths
if env["platform"] == "macos":
    env.Append(LIBPATH=[godot_cpp_dir + "/bin"])
    env.Append(LIBS=["godot-cpp.macos.template_debug.arm64"]) # Default, SCons will expand based on target

# Conditional Source and OpenCV dependency mapping
sources = Glob("src/core/*.cpp") + Glob("src/godot/*.cpp")

if env["platform"] == "javascript":
    # Web target: exclude OpenCV native, include web stub
    print("Building for Web/WASM target. Excluding native OpenCV and compiling web sidecar bridge.")
    sources += Glob("src/web/*.cpp")
    env.Append(CPPDEFINES=["WEB_ENABLED"])
else:
    # Native target: include native implementations and link OpenCV
    print("Building for Native platform. Configuring OpenCV linking.")
    sources += Glob("src/native/*.cpp")

    # Locate OpenCV headers and libs
    opencv_sdk = env["opencv_dir"]
    if not opencv_sdk:
        # Check local thirdparty folder
        local_opencv = "thirdparty/opencv/" + env["platform"]
        if os.path.isdir(local_opencv):
            opencv_sdk = local_opencv
        elif env["platform"] == "macos" and os.path.isdir("thirdparty/opencv/macos"):
            opencv_sdk = "thirdparty/opencv/macos"

    if opencv_sdk and os.path.isdir(opencv_sdk):
        print("Using OpenCV SDK located at: " + opencv_sdk)
        env.Append(CPPPATH=[opencv_sdk + "/include", opencv_sdk + "/include/opencv4"])
        env.Append(LIBPATH=[opencv_sdk + "/lib"])
        # Standard OpenCV libraries we require
        env.Append(LIBS=["opencv_core", "opencv_imgproc", "opencv_objdetect", "opencv_videoio", "opencv_dnn", "opencv_calib3d"])
    else:
        print("WARNING: OpenCV SDK was not found! Compilation will fail on native targets unless OPENCV_DIR is set.")

# Output library name mapping
lib_prefix = "lib"
lib_suffix = ".so"
if env["platform"] == "windows":
    lib_prefix = ""
    lib_suffix = ".dll"
elif env["platform"] == "macos":
    lib_suffix = ".dylib"
elif env["platform"] == "javascript":
    lib_prefix = "lib"
    lib_suffix = ".wasm"

target_lib = "project/addons/godot-gaze/bin/gaze." + env["platform"] + "." + env["target"]

# Create shared library builder call
env.SharedLibrary(target=target_lib, source=sources)
