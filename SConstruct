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
if "PATH" in os.environ:
    env["ENV"]["PATH"] = os.environ["PATH"]

# Set build target directory
env.Append(CPPPATH=["src/core", "src/native", "src/web", "src/godot", "thirdparty/one_euro_filter"])

# Detect compilers and setup platform-specific flags
if env["platform"] == "macos":
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC"])
    if env["use_llvm"]:
        env.Replace(CC="clang", CXX="clang++")
elif env["platform"] == "windows":
    env.Append(CCFLAGS=["/std:c++17", "/O2", "/EHsc"])
elif env["platform"] == "javascript":
    env["CC"] = "emcc"
    env["CXX"] = "em++"
    env["AR"] = "emar"
    env["RANLIB"] = "emranlib"
    env["OBJSUFFIX"] = ".o"
    env["SHOBJSUFFIX"] = ".o"
    env.Replace(SHLIBSUFFIX=".wasm")
    env.Replace(SHLIBPREFIX="")
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-sSIDE_MODULE=1", "-sSUPPORT_LONGJMP='wasm'"])
    env.Append(LINKFLAGS=["-sSIDE_MODULE=1", "-sWASM_BIGINT", "-sSUPPORT_LONGJMP='wasm'"])
else:  # Linux/Android/iOS
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC"])

# Godot cpp bindings setup (assuming godot-cpp is present as submodule/directory)
godot_cpp_dir = os.environ.get("GODOT_CPP_DIR", "godot-cpp")
if not os.path.isdir(godot_cpp_dir) and os.path.isdir("thirdparty/godot-cpp"):
    godot_cpp_dir = "thirdparty/godot-cpp"

env.Append(CPPPATH=[
    godot_cpp_dir + "/include",
    godot_cpp_dir + "/gen/include",
    godot_cpp_dir + "/gdextension"
])

# Setup platform-specific library paths
env.Append(LIBPATH=[godot_cpp_dir + "/bin"])

godot_cpp_platform = "web" if env["platform"] == "javascript" else env["platform"]
lib_name = f"godot-cpp.{godot_cpp_platform}.{env['target']}"  # Default fallback
bin_dir = godot_cpp_dir + "/bin"
if os.path.isdir(bin_dir):
    for f in os.listdir(bin_dir):
        if f.startswith(f"libgodot-cpp.{godot_cpp_platform}.{env['target']}") and f.endswith(".a"):
            lib_name = f[3:-2] # Strip 'lib' prefix and '.a' suffix
            break
env.Append(LIBS=[lib_name])

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
        elif env["platform"] == "macos":
            # Check standard Homebrew paths on macOS
            for hb_path in ["/opt/homebrew/opt/opencv", "/usr/local/opt/opencv"]:
                if os.path.isdir(hb_path):
                    opencv_sdk = hb_path
                    break

    if opencv_sdk and os.path.isdir(opencv_sdk):
        print("Using OpenCV SDK located at: " + opencv_sdk)
        env.Append(CPPPATH=[opencv_sdk + "/include", opencv_sdk + "/include/opencv4"])
        env.Append(LIBPATH=[opencv_sdk + "/lib"])
        # Standard OpenCV libraries we require
        env.Append(LIBS=["opencv_core", "opencv_imgproc", "opencv_imgcodecs", "opencv_objdetect", "opencv_videoio", "opencv_dnn", "opencv_calib3d"])
    else:
        print("WARNING: OpenCV SDK was not found! Compilation will fail on native targets unless OPENCV_DIR is set.")

# Output library name mapping
# We let SCons use default prefixes for native desktop platforms:
# - macOS: libgaze.macos.template_debug.dylib
# - Linux: libgaze.linux.template_debug.so
# - Windows: gaze.windows.template_debug.dll
# For Web (Javascript), we customize prefix for WASM.
if env["platform"] == "javascript":
    env.Replace(SHLIBPREFIX="")

# We explicitly determine and append the suffix to prevent SCons from treating 
# the dot in ".platform.target" as a file extension and omitting the actual suffix.
lib_suffix = ".so"
if env["platform"] == "windows":
    lib_suffix = ".dll"
elif env["platform"] == "macos":
    lib_suffix = ".dylib"
elif env["platform"] == "javascript":
    lib_suffix = ".wasm"

target_lib = "project/addons/godot-gaze/bin/gaze." + env["platform"] + "." + env["target"] + lib_suffix

# Create shared library builder call
env.SharedLibrary(target=target_lib, source=sources)
