# SConstruct
# SCons build configuration for the godot-gaze GDExtension library.

import os
import sys
import subprocess

# Detect and prepend emsdk bin directory if using asdf
if any("platform=javascript" in arg for arg in sys.argv) or any("platform=web" in arg for arg in sys.argv):
    try:
        emsdk_path = subprocess.check_output(["asdf", "where", "emsdk"], text=True).strip()
        emscripten_bin = os.path.join(emsdk_path, "upstream", "emscripten")
        if os.path.isdir(emscripten_bin):
            os.environ["PATH"] = emscripten_bin + os.pathsep + os.environ.get("PATH", "")
    except Exception:
        pass

# Normalize platform argument to web internally before SCons variables update
if ARGUMENTS.get("platform", "") == "javascript":
    ARGUMENTS["platform"] = "web"

default_platform = sys.platform
if default_platform == "darwin":
    default_platform = "macos"
elif default_platform.startswith("linux"):
    default_platform = "linux"
elif default_platform in ["win32", "cygwin"]:
    default_platform = "windows"

# Define target options
opts = Variables()
opts.Add(EnumVariable("platform", "Target platform", default_platform, allowed_values=["windows", "linux", "macos", "android", "ios", "javascript", "web"]))
opts.Add(EnumVariable("target", "Compilation target", "template_debug", allowed_values=["template_debug", "template_release", "editor"]))
opts.Add(BoolVariable("use_llvm", "Use LLVM/Clang compiler", False))
opts.Add(PathVariable("opencv_dir", "Path to OpenCV SDK root", os.environ.get("OPENCV_DIR", ""), PathVariable.PathAccept))
opts.Add(BoolVariable("threads", "Enable threading support", False))

env = Environment(variables=opts)
if "PATH" in os.environ:
    env["ENV"]["PATH"] = os.environ["PATH"]

# Normalize platform key in env if it defaults to javascript
if env["platform"] == "javascript":
    env["platform"] = "web"

# Set build target directory
env.Append(CPPPATH=["src/core", "src/native", "src/web", "src/godot", "thirdparty/one_euro_filter"])

# Detect compilers and setup platform-specific flags
if env["platform"] == "macos":
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC"])
    if env["use_llvm"]:
        env.Replace(CC="clang", CXX="clang++")
elif env["platform"] == "windows":
    env.Append(CCFLAGS=["/std:c++17", "/O2", "/EHsc"])
elif env["platform"] == "web":
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
    if env["threads"]:
        env.Append(CCFLAGS=["-sUSE_PTHREADS=1"])
        env.Append(LINKFLAGS=["-sUSE_PTHREADS=1"])
else:  # Linux/Android/iOS
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC"])

# Godot cpp bindings setup
godot_cpp_dir = "thirdparty/godot-cpp"
threads_suffix = "_threads" if env.get("threads", False) else "_nothreads"
variant_path = f"build/godot-cpp/{env['platform']}_{env['target']}{threads_suffix}"

# If target is macos and arch is not specified, default to host architecture (arm64 or x86_64)
# to avoid slow and failing universal builds.
if env["platform"] == "macos" and not "arch" in ARGUMENTS:
    import platform as pyplatform
    host_arch = pyplatform.machine()
    target_arch = "arm64" if host_arch in ["arm64", "aarch64"] else "x86_64"
    ARGUMENTS["arch"] = target_arch
    env["arch"] = target_arch
    print(f"Arch not specified. Defaulting to host architecture: {target_arch}")

Export("env")
SConscript(
    godot_cpp_dir + "/SConstruct",
    variant_dir=variant_path,
    duplicate=0
)

env.Append(CPPPATH=[
    godot_cpp_dir + "/include",
    variant_path + "/gen/include",
    godot_cpp_dir + "/gdextension"
])

# Conditional Source and OpenCV dependency mapping
sources = Glob("src/core/*.cpp") + Glob("src/godot/*.cpp")

if env["platform"] == "web":
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

    if env["platform"] == "android":
        if opencv_sdk and os.path.isdir(opencv_sdk):
            print("Using OpenCV Android SDK located at: " + opencv_sdk)
            env.Append(CPPPATH=[
                opencv_sdk + "/sdk/native/jni/include",
                opencv_sdk + "/sdk/native/jni/include/opencv4"
            ])
            env.Append(LIBPATH=[opencv_sdk + "/sdk/native/staticlibs/arm64-v8a"])
            env.Append(LIBS=["opencv_core", "opencv_imgproc", "opencv_imgcodecs", "opencv_objdetect", "opencv_videoio", "opencv_dnn", "opencv_calib3d"])
        else:
            print("WARNING: OpenCV Android SDK was not found! Compilation will fail for android unless OPENCV_DIR is set.")
    elif env["platform"] == "ios":
        if opencv_sdk and os.path.isdir(opencv_sdk):
            print("Using OpenCV iOS Framework located at: " + opencv_sdk)
            env.Append(CPPPATH=[opencv_sdk + "/opencv2.framework/Headers"])
            env.Append(FRAMEWORKPATH=[opencv_sdk])
            env.Append(LINKFLAGS=["-framework", "opencv2"])
        else:
            print("WARNING: OpenCV iOS Framework was not found! Compilation will fail for ios unless OPENCV_DIR is set.")
    elif env["platform"] == "windows":
        if opencv_sdk and os.path.isdir(opencv_sdk):
            print("Using OpenCV Windows SDK located at: " + opencv_sdk)
            env.Append(CPPPATH=[opencv_sdk + "/build/include"])
            lib_dir = ""
            for vc_ver in ["vc16", "vc15"]:
                check_path = os.path.join(opencv_sdk, "build", "x64", vc_ver, "lib")
                if os.path.isdir(check_path):
                    lib_dir = check_path
                    break
            if lib_dir:
                env.Append(LIBPATH=[lib_dir])
                libs = []
                for f in os.listdir(lib_dir):
                    if f.endswith(".lib") and not f.endswith("d.lib"):
                        libs.append(f.removesuffix(".lib"))
                if libs:
                    print("Linking OpenCV libraries: " + str(libs))
                    env.Append(LIBS=libs)
                else:
                    env.Append(LIBS=["opencv_world4100"])
            else:
                if os.path.isdir(opencv_sdk + "/lib"):
                    env.Append(LIBPATH=[opencv_sdk + "/lib"])
                    env.Append(LIBS=["opencv_core", "opencv_imgproc", "opencv_imgcodecs", "opencv_objdetect", "opencv_videoio", "opencv_dnn", "opencv_calib3d"])
        else:
            print("WARNING: OpenCV Windows SDK was not found! Compilation will fail for windows unless OPENCV_DIR is set.")
    else:
        # macOS / Linux / Default
        if not opencv_sdk and env["platform"] == "macos":
            for hb_path in ["/opt/homebrew/opt/opencv", "/usr/local/opt/opencv"]:
                if os.path.isdir(hb_path):
                    opencv_sdk = hb_path
                    break
        if opencv_sdk and os.path.isdir(opencv_sdk):
            print("Using OpenCV SDK located at: " + opencv_sdk)
            env.Append(CPPPATH=[opencv_sdk + "/include", opencv_sdk + "/include/opencv4"])
            env.Append(LIBPATH=[opencv_sdk + "/lib"])
            env.Append(LIBS=["opencv_core", "opencv_imgproc", "opencv_imgcodecs", "opencv_objdetect", "opencv_videoio", "opencv_dnn", "opencv_calib3d"])
        else:
            print("WARNING: OpenCV SDK was not found! Compilation will fail on native targets unless OPENCV_DIR is set.")

# Output library name mapping
if env["platform"] == "web":
    env.Replace(SHLIBPREFIX="")

# Explicitly determine suffix and target builder
lib_suffix = ".so"
if env["platform"] == "windows":
    lib_suffix = ".dll"
elif env["platform"] == "macos":
    lib_suffix = ".dylib"
elif env["platform"] == "web":
    lib_suffix = ".wasm"
elif env["platform"] == "ios":
    lib_suffix = ".a"

lib_threads_suffix = ".threads" if (env["platform"] == "web" and env["threads"]) else ""
file_platform = "javascript" if env["platform"] == "web" else env["platform"]
target_lib = "project/addons/godot-gaze/bin/gaze." + file_platform + "." + env["target"] + lib_threads_suffix + lib_suffix

# Force enable exceptions for godot-gaze library compilation on native platforms
# Since godot-cpp disables exceptions by default, we restore them for our library (except on Web)
if env["platform"] != "web":
    if "is_msvc" in env and env["is_msvc"]:
        if "CPPDEFINES" in env:
            if isinstance(env["CPPDEFINES"], list) and ("_HAS_EXCEPTIONS", 0) in env["CPPDEFINES"]:
                env["CPPDEFINES"].remove(("_HAS_EXCEPTIONS", 0))
        env.Append(CXXFLAGS=["/EHsc"])
    else:
        for flag_key in ["CXXFLAGS", "CCFLAGS"]:
            if flag_key in env:
                if isinstance(env[flag_key], list):
                    while "-fno-exceptions" in env[flag_key]:
                        env[flag_key].remove("-fno-exceptions")
                elif isinstance(env[flag_key], str):
                    env[flag_key] = env[flag_key].replace("-fno-exceptions", "")
        env.Append(CXXFLAGS=["-fexceptions"])

# Create library builder call (Static on iOS, Shared on other platforms)
if env["platform"] == "ios":
    library = env.StaticLibrary(target=target_lib, source=sources)
else:
    library = env.SharedLibrary(target=target_lib, source=sources)

env.Default(library)
