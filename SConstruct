# SConstruct
# SCons build configuration for the godot-gaze GDExtension library.

import os
import sys
import subprocess

# Copy JavaScript sidecar file to the Godot Gaze addon bin directory
# TODO: Does this belong here? Can we just move it into src/web/... and make the js rule a part of the js target(s)?
def copy_js_sidecar():
    js_path = "src/web/gaze_sidecar.js"
    dest_path = "project/addons/godot-gaze/bin/gaze_sidecar.js"
    if not os.path.exists(js_path):
        print(f"Error: {js_path} does not exist. Cannot copy JS sidecar.")
        return
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    print(f"[SCons] Copying {js_path} to {dest_path}...")
    import shutil
    shutil.copy2(js_path, dest_path)

copy_js_sidecar()



# TODO: Replace with a real github dependency, or vendor it. I don't understand why this is searched and built, when it's in thirdparty.
def find_and_copy_stb():
    import glob
    import shutil
    dest = "thirdparty/stb/stb_image.h"
    if os.path.exists(dest):
        return True
    
    # Common search paths
    search_patterns = [
        "/opt/homebrew/Cellar/emscripten/*/libexec/third_party/stb_image.h",
        "/usr/local/Cellar/emscripten/*/libexec/third_party/stb_image.h",
        "/opt/homebrew/opt/emscripten/libexec/third_party/stb_image.h",
        "/usr/local/opt/emscripten/libexec/third_party/stb_image.h"
    ]
    for pattern in search_patterns:
        matches = glob.glob(pattern)
        if matches:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(matches[0], dest)
            print(f"[SCons] Copied stb_image.h from {matches[0]} to {dest}")
            return True
            
    print("[SCons] Warning: stb_image.h not found on system.")
    return False

find_and_copy_stb()

# Detect and prepend emsdk bin directory if using asdf
# TODO: Does this belong here? Another web dependency afaict? Is there a way to make this check exist in web and not in this root document, or would that be messier than this way of doing things?
if any("platform=javascript" in arg for arg in sys.argv) or any("platform=web" in arg for arg in sys.argv):
    try:
        emsdk_path = subprocess.check_output(["asdf", "where", "emsdk"], text=True).strip()
        emscripten_bin = os.path.join(emsdk_path, "upstream", "emscripten")
        if os.path.isdir(emscripten_bin):
            os.environ["PATH"] = emscripten_bin + os.pathsep + os.environ.get("PATH", "")
    except Exception:
        pass

# TODO: Do not add complexity unnecessarily. If godot says it's "web", we should be using "web". If godot says it's "macos", we should be using "macos". Does godot handle these synonyms (guessing not, or we wouldn't need a rewrite!)? No? Neither do we! Yes? So should we, with a doc about why and which!
# TODO: Are there scripts or other reasons that are currently passing deprecated synonyms like "javascript" or "darwin", etc? We should repair them!


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

opts.Add(BoolVariable("threads", "Enable threading support", False))
opts.Add("arch", "Target architecture", "")
opts.Add(BoolVariable("ios_simulator", "Build for iOS simulator", False))
opts.Add(BoolVariable("build_tests", "Build native unit test suite", False))
opts.Add("IOS_SDK_PATH", "Path to the iOS SDK", "")
opts.Add("IOS_TOOLCHAIN_PATH", "Path to iOS toolchain", "")

env = Environment(variables=opts)

# Resolve default architecture if not specified
if not env.get("arch") or env["arch"] == "":
    if env["platform"] == "macos":
        import platform as pyplatform
        host_arch = pyplatform.machine()
        target_arch = "arm64" if host_arch in ["arm64", "aarch64"] else "x86_64"
    elif env["platform"] == "android":
        target_arch = "arm64"
    elif env["platform"] == "ios":
        target_arch = "arm64"
    elif env["platform"] == "web":
        target_arch = "wasm32"
    else:
        import platform as pyplatform
        host_arch = pyplatform.machine()
        target_arch = "arm64" if host_arch in ["arm64", "aarch64"] else "x86_64"
    ARGUMENTS["arch"] = target_arch
    env["arch"] = target_arch
else:
    ARGUMENTS["arch"] = env["arch"]

# TODO: If they *specified* ios_simulator in ARGUMENTS, we overwrite it if it's in env? That seems wrong. In general, explicit args should beat env I would think? But this might be a wacky scons thing.
if "ios_simulator" in env:
    ARGUMENTS["ios_simulator"] = env["ios_simulator"]
if "PATH" in os.environ:
    env["ENV"]["PATH"] = os.environ["PATH"]

# Configure parallel building using available CPU cores if not specified by the user
# TODO: surely num_jobs == 1 means *one job*, so this flag is inappropriate?
if GetOption('num_jobs') <= 1:
    import os
    num_jobs = os.cpu_count() or 1
    SetOption('num_jobs', num_jobs)
    print(f"[SCons] Auto-detected parallel builds: setting -j to {num_jobs}")

# Normalize platform key in env if it defaults to javascript
# TODO: Didn't we already do this on line 68? Harmonize and, as commented there, maybe skip.
if env["platform"] == "javascript":
    env["platform"] = "web"

# ARCOM: The archivier commandline
# Wrap ARCOM with TEMPFILE for non-Windows platforms to avoid "Argument list too long" errors.
# We skip Windows because SCons natively manages long command lines for MSVC by generating
# MSVC-specific .rsp files. Overwriting ARCOM here would break MSVC's custom /OUT: syntax.
if env["platform"] != "windows":
    env["ARCOM"] = "${TEMPFILE('$AR $ARFLAGS $TARGET $SOURCES')}"

# Set build target directory
# TODO: checking this is still correct? I'd expect this was specific to current targets.
env.Append(CPPPATH=["#src/core", "#src/native", "#src/web", "#src/godot", "#thirdparty/one_euro_filter"])

# Detect compilers and setup platform-specific flags
if env["platform"] == "macos":
    env.Append(CCFLAGS=["-std=c++17", "-O2", "-fPIC", "-fobjc-arc"])
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
sim_suffix = "_simulator" if (env["platform"] == "ios" and env.get("ios_simulator", False)) else ""
threads_suffix = "_threads" if env.get("threads", False) else "_nothreads"
variant_path = f"build/godot-cpp/{env['platform']}_{env['target']}_{env['arch']}{sim_suffix}{threads_suffix}"

# Clean up custom arguments to prevent warnings from the godot-cpp build system
for custom_arg in ["build_tests"]:
    if custom_arg in ARGUMENTS:
        del ARGUMENTS[custom_arg]

Export("env")
SConscript(
    godot_cpp_dir + "/SConstruct",
    variant_dir=variant_path,
    duplicate=0
)

# Clone clean environment AFTER SConscript executes to preserve SCons builders (e.g. compilation_db)
core_env = env.Clone()

# Filter out any godot-cpp include paths to ensure strict layer isolation
if "CPPPATH" in core_env:
    core_env["CPPPATH"] = [p for p in core_env["CPPPATH"] if "godot" not in str(p)]

# Restore exceptions for our clean environment as well (since godot-cpp disables them by default)
if core_env["platform"] != "web":
    if "is_msvc" in core_env and core_env["is_msvc"]:
        core_env.Append(CXXFLAGS=["/EHsc"])
    else:
        for flag_key in ["CXXFLAGS", "CCFLAGS"]:
            if flag_key in core_env:
                if isinstance(core_env[flag_key], list):
                    while "-fno-exceptions" in core_env[flag_key]:
                        core_env[flag_key].remove("-fno-exceptions")
                elif isinstance(core_env[flag_key], str):
                    core_env[flag_key] = core_env[flag_key].replace("-fno-exceptions", "")
        core_env.Append(CXXFLAGS=["-fexceptions"])

env.Append(CPPPATH=[
    godot_cpp_dir + "/include",
    variant_path + "/gen/include",
    godot_cpp_dir + "/gdextension"
])

# Create SCons VariantDir for godot-gaze compilation
gaze_variant_dir = f"build/godot-gaze/{env['platform']}_{env['target']}_{env['arch']}{sim_suffix}{threads_suffix}"
VariantDir(gaze_variant_dir, "src", duplicate=0)
VariantDir(os.path.join(gaze_variant_dir, "thirdparty_objc"), "thirdparty/onnxruntime/objectivec", duplicate=0)

def setup_onnxruntime(env):
    import sys
    import os
    import subprocess
    import shutil

    platform = env["platform"]
    arch = env["arch"]
    
    # 1. Initialize Submodule if not present
    # TODO: Again, is this standard? I'd expect something in setup/etc requiring the submodule. Now that the project exists and has the submodule, checking out our project automatically checks out the submodule, right?
    # TODO: So we shoud just delete this?!
    ort_dir = "thirdparty/onnxruntime"
    if not os.path.exists(os.path.join(ort_dir, "CMakeLists.txt")):
        print("[SCons] Initializing onnxruntime submodule...")
        try:
            subprocess.run(["git", "submodule", "update", "--init", "--recursive", "--depth", "1", "thirdparty/onnxruntime"], check=True)
        except Exception as e:
            print(f"[SCons] git submodule update failed, performing manual clone & checkout: {e}")
            subprocess.run(["git", "submodule", "add", "--force", "--depth", "1", "https://github.com/microsoft/onnxruntime.git", "thirdparty/onnxruntime"], check=False)
            subprocess.run(["git", "fetch", "--depth", "1", "origin", "tag", "v1.17.1"], cwd="thirdparty/onnxruntime", check=True)
            subprocess.run(["git", "checkout", "v1.17.1"], cwd="thirdparty/onnxruntime", check=True)
            subprocess.run(["git", "submodule", "update", "--init", "--recursive", "--depth", "1"], check=True)

    # 2. Convert models to .ort format if .ort files don't exist
    # TODO: These .ort are actually checked in, right? I think maybe better to expect the .ort to exist in the proper place, and document (in CONTRIBUTING?) how to swap/upgrade models. So this, too, should be removed.
    # TODO: I'm sympathetic to leaving this tool available through scons. Perhaps as a custom target? But it seems just as easy to just make it a script (which I think it also is...?), so why repeat ourselves, esp in this specific, navigation-oriented build-oriented file?
    model_dir = "project/addons/godot-gaze/models"
    clean_models_marker = os.path.join(model_dir, ".clean_models_v2")
    if not os.path.exists(model_dir):
        os.makedirs(model_dir, exist_ok=True)

    config_dst = os.path.join(model_dir, "required_operators.config")
    if not os.path.exists(config_dst):
        config_src = "scripts/required_operators.config"
        if os.path.exists(config_src):
            print(f"[SCons] Copying fallback required operators config from {config_src} to {config_dst}...")
            import shutil
            shutil.copy2(config_src, config_dst)

    models_to_convert = [
        ("face_detection_yunet_2023mar.onnx", "face_detection_yunet_2023mar.ort"),
        ("gaze-estimation-adas-0002.onnx", "gaze-estimation-adas-0002.ort")
    ]

    if not os.path.exists(clean_models_marker):
        has_onnx = any(os.path.exists(os.path.join(model_dir, onnx_name)) for onnx_name, _ in models_to_convert)
        if has_onnx:
            print("[SCons] Deleting old ORT models and config to regenerate with basic optimization...")
            for f in os.listdir(model_dir):
                if f.endswith(".ort") or f == "required_operators.config":
                    try:
                        os.remove(os.path.join(model_dir, f))
                    except Exception as e:
                        pass
        with open(clean_models_marker, "w") as f:
            f.write("cleaned")
            
    needed_conversion = False
    for onnx_name, ort_name in models_to_convert:
        if not os.path.exists(os.path.join(model_dir, ort_name)):
            # Only trigger conversion if the source ONNX file actually exists.
            if os.path.exists(os.path.join(model_dir, onnx_name)):
                needed_conversion = True
                break
            
    if needed_conversion:
        print("[SCons] Converting ONNX models to ORT format...")
        try:
            import onnxruntime
            import onnx
        except ImportError:
            print("[SCons] onnx or onnxruntime python package not found. Installing...")
            subprocess.run([sys.executable, "-m", "pip", "install", "onnx", "onnxruntime"], check=False)
        
        conv_cmd = [
            sys.executable,
            "-m", "onnxruntime.tools.convert_onnx_models_to_ort",
            "--optimization_style=Runtime",
            model_dir
        ]
        print(f"[SCons] Running model conversion: {' '.join(conv_cmd)}")
        subprocess.run(conv_cmd, check=True)
        # Rename and filter config
        import shutil
        if os.path.exists(os.path.join(model_dir, "face_detection_yunet_2023mar.with_runtime_opt.ort")):
            shutil.move(os.path.join(model_dir, "face_detection_yunet_2023mar.with_runtime_opt.ort"), os.path.join(model_dir, "face_detection_yunet_2023mar.ort"))
        if os.path.exists(os.path.join(model_dir, "gaze-estimation-adas-0002.with_runtime_opt.ort")):
            shutil.move(os.path.join(model_dir, "gaze-estimation-adas-0002.with_runtime_opt.ort"), os.path.join(model_dir, "gaze-estimation-adas-0002.ort"))
        
        config_src = os.path.join(model_dir, "required_operators.with_runtime_opt.config")
        config_dst = os.path.join(model_dir, "required_operators.config")
        if os.path.exists(config_src):
            with open(config_src, "r") as f:
                lines = f.readlines()
            filtered_lines = [line for line in lines if "com.microsoft" not in line]
            with open(config_dst, "w") as f:
                f.writelines(filtered_lines)
            os.remove(config_src)

    # 3. Build ONNX Runtime if the library is not built yet
    # TODO: Ok! This I think we need, again, forked by plat etc. Thanks, this is reasonable. Is it reasonable to add a SConstruct file to third party to manage building onnx specifically, so remove this from here and delegate to there?
    #       One way to decide if we can put this code in a thirdparty/SConstruct file is if it's reasonable to have multiple exposed targets in such a file (one per further child dir), each of which will have its own requirements and not all of which are used in any one build.
    #       Also, they'd need to be able to inherit/be-injected-with some of these variables cleanly (platform, arch, etc).
    ort_build_dir = f"build/ort/{platform}_{arch}"
    clean_marker = os.path.join(ort_build_dir, ".clean_marker_v13")
    if not os.path.exists(clean_marker):
        print(f"[SCons] Cleaning old build directory {ort_build_dir} to clear CMakeCache...")
        import shutil
        if os.path.exists(ort_build_dir):
            try:
                shutil.rmtree(ort_build_dir)
            except Exception as e:
                print(f"[SCons] Failed to remove build dir: {e}")
        os.makedirs(ort_build_dir, exist_ok=True)
        with open(clean_marker, "w") as f:
            f.write("cleaned")
    ort_lib_name = "onnxruntime"
    ort_config = "MinSizeRel"
    
    # TODO: This can't be specific to onnx, we must use this elsewhere. Is there a way to share it?
    lib_suffix = ".so"
    if platform == "windows":
        lib_suffix = ".dll"
    elif platform == "macos":
        lib_suffix = ".dylib"
    elif platform == "ios":
        lib_suffix = ".a"
    
    # TODO: This can't be specific to onnx, we must use this elsewhere. Is there a way to share it?
    lib_prefix = "lib" if platform != "windows" else ""
    expected_lib_path = os.path.join(ort_build_dir, ort_config, f"{lib_prefix}{ort_lib_name}{lib_suffix}")
    
    # TODO: This can't be specific to onnx, we must use this elsewhere. Is there a way to share it?
    out_dir = "project/addons/godot-gaze/bin"
    target_lib_path = os.path.join(out_dir, f"{lib_prefix}{ort_lib_name}{lib_suffix}")
    if os.path.exists(target_lib_path) and not os.path.exists(expected_lib_path):
        print(f"[SCons] Restoring {expected_lib_path} from {target_lib_path} to avoid redundant build...")
        os.makedirs(os.path.dirname(expected_lib_path), exist_ok=True)
        import shutil
        shutil.copy2(target_lib_path, expected_lib_path)
        if platform == "windows":
            target_import_lib = os.path.join(out_dir, f"{ort_lib_name}.lib")
            expected_import_lib = os.path.join(ort_build_dir, ort_config, f"{ort_lib_name}.lib")
            if os.path.exists(target_import_lib):
                shutil.copy2(target_import_lib, expected_import_lib)

    # TODO: This + the above feels really weird. Is this the expected way for an scons file to run? I'd expect we'd just use a build if cached or do a build. The "restoring" step feels strange.
    if not os.path.exists(expected_lib_path):
        print(f"[SCons] {expected_lib_path} not found. Starting ONNX Runtime custom build...")
        
        ep_flags = ["--use_xnnpack"]
        if platform == "android":
            ep_flags.append("--use_nnapi")
            
        minimal_build_type = "extended"
        build_cmd = [
            sys.executable,
            "thirdparty/onnxruntime/tools/ci_build/build.py",
            "--build_dir", ort_build_dir,
            "--config", ort_config,
        ]
        if platform != "ios":
            build_cmd.append("--build_shared_lib")
        build_cmd += [
            "--minimal_build", minimal_build_type,
            "--include_ops_by_config", os.path.join(model_dir, "required_operators.config"),
            "--disable_ml_ops",
            "--disable_contrib_ops",
            "--enable_lto",
            "--enable_reduced_operator_type_support",
            "--parallel",
            "--skip_tests",
            "--cmake_extra_defines",
            "CMAKE_POLICY_VERSION_MINIMUM=3.5",
            "onnxruntime_BUILD_UNIT_TESTS=OFF",
            "onnxruntime_PREFER_SYSTEM_LIB=OFF",
            "onnxruntime_USE_SYSTEM_PROTOBUF=OFF",
            "CMAKE_DISABLE_FIND_PACKAGE_Protobuf=ON",
            "CMAKE_DISABLE_FIND_PACKAGE_protobuf=ON",
            "CMAKE_DISABLE_FIND_PACKAGE_ONNX=ON",
            "CMAKE_DISABLE_FIND_PACKAGE_onnx=ON",
            "CMAKE_DISABLE_FIND_PACKAGE_re2=ON"
        ]
        # I think I've seen this elsewhere in the file. Can this stuff be shared, so that only onnx specific overrides are provided? Same question for android below, macos below.
        if platform == "macos":
            build_cmd += [
                "CMAKE_CXX_COMPILER=/usr/bin/c++",
                "CMAKE_C_COMPILER=/usr/bin/cc"
            ]
        build_cmd += ep_flags
        
        if platform == "android":
            build_cmd += [
                "--android",
                "--android_sdk_path", os.environ.get("ANDROID_SDK_ROOT", os.environ.get("ANDROID_HOME", "")),
                "--android_ndk_path", os.environ.get("ANDROID_NDK_ROOT", ""),
                "--android_abi", "arm64-v8a" if arch == "arm64" else "x86_64",
                "--android_api", "29"
            ]
        elif platform == "ios":
            build_cmd += ["--ios", "--osx_arch", arch, "--apple_deploy_target", "12.0"]
            if env.get("ios_simulator", False):
                if env.get("IOS_SDK_PATH", "") != "":
                    build_cmd += ["--apple_sysroot", env["IOS_SDK_PATH"]]
                else:
                    build_cmd += ["--apple_sysroot", "iphonesimulator"]
            else:
                if env.get("IOS_SDK_PATH", "") != "":
                    build_cmd += ["--apple_sysroot", env["IOS_SDK_PATH"]]
                else:
                    build_cmd += ["--apple_sysroot", "iphoneos"]
        elif platform == "macos":
            # ONNX Runtime build.py detects native architecture automatically
            pass
            
        print(f"[SCons] Building ORT: {' '.join(build_cmd)}")
        my_env = os.environ.copy()
        for env_var in ["CPATH", "C_INCLUDE_PATH", "CXX_INCLUDE_PATH"]:
            if env_var in my_env:
                print(f"[SCons] Removing polluting environment variable: {env_var}={my_env[env_var]}")
                del my_env[env_var]
        if "PATH" in my_env:
            paths = my_env["PATH"].split(os.pathsep)
            filtered_paths = [p for p in paths if "Android/sdk" not in p]
            my_env["PATH"] = os.pathsep.join(filtered_paths)
        subprocess.run(build_cmd, check=True, env=my_env)

    # 4. Add include paths and link settings
    ort_dir_scons = "#thirdparty/onnxruntime"
    env.Append(CPPPATH=[
        ort_dir_scons,
        ort_dir_scons + "/include",
        ort_dir_scons + "/include/onnxruntime",
        ort_dir_scons + "/include/onnxruntime/core/session",
        ort_dir_scons + "/objectivec",
        ort_dir_scons + "/objectivec/include"
    ])
    
    env.Append(LIBPATH=["#" + os.path.join(ort_build_dir, ort_config)])
    env.Append(LIBS=[ort_lib_name])
    
    if platform == "macos":
        env.Append(LINKFLAGS=["-Wl,-rpath,@loader_path"])
        out_dir = "project/addons/godot-gaze/bin"
        os.makedirs(out_dir, exist_ok=True)
        dylib_path = os.path.join(out_dir, f"{lib_prefix}{ort_lib_name}{lib_suffix}")
        shutil.copy2(expected_lib_path, dylib_path)
        print(f"[SCons] Copied {expected_lib_path} to {out_dir}")
        subprocess.run(["codesign", "-s", "-", "--force", dylib_path], check=False)
        
        # Copy to build/tests for macOS SIP/Gatekeeper validation compatibility
        test_dir = "build/tests"
        os.makedirs(test_dir, exist_ok=True)
        test_dylib = os.path.join(test_dir, f"{lib_prefix}{ort_lib_name}{lib_suffix}")
        shutil.copy2(expected_lib_path, test_dylib)
        subprocess.run(["codesign", "-s", "-", "--force", test_dylib], check=False)
        print(f"[SCons] Copied and signed {expected_lib_path} in {test_dir} for macOS SIP compliance")
        
        if env["target"] != "template_debug":
            print(f"[SCons] Stripping local symbols from {dylib_path}...")
            subprocess.run(["strip", "-x", dylib_path], check=False)
    elif platform in ["linux", "android"]:
        if platform == "linux":
            env.Append(LINKFLAGS=["-Wl,-rpath,$ORIGIN"])
        out_dir = "project/addons/godot-gaze/bin"
        os.makedirs(out_dir, exist_ok=True)
        import shutil
        shutil.copy2(expected_lib_path, os.path.join(out_dir, f"{lib_prefix}{ort_lib_name}{lib_suffix}"))
    elif platform == "windows":
        out_dir = "project/addons/godot-gaze/bin"
        os.makedirs(out_dir, exist_ok=True)
        import shutil
        shutil.copy2(expected_lib_path, os.path.join(out_dir, f"{lib_prefix}{ort_lib_name}{lib_suffix}"))
        lib_path = os.path.join(ort_build_dir, ort_config, f"{ort_lib_name}.lib")
        if os.path.exists(lib_path):
            shutil.copy2(lib_path, os.path.join(out_dir, f"{ort_lib_name}.lib"))

# Conditional Source and Dependency mapping
# Compile core, native, and windows using core_env (no Godot includes)
# Compile godot and gen using env (with Godot includes)
if env["platform"] != "web":
    setup_onnxruntime(core_env)
    setup_onnxruntime(env)
    if env["platform"] == "windows":
        win_libs = ["mf", "mfplat", "mfreadwrite", "mfuuid", "ole32"]
        env.Append(LIBS=win_libs)
        core_env.Append(LIBS=win_libs)
    elif env["platform"] == "macos":
        mac_flags = ["-framework", "ApplicationServices", "-framework", "CoreFoundation", "-framework", "ImageIO", "-framework", "CoreGraphics", "-framework", "CoreVideo", "-framework", "CoreMedia", "-framework", "AVFoundation", "-framework", "Metal", "-framework", "Foundation"]
        env.Append(LINKFLAGS=mac_flags)
        core_env.Append(LINKFLAGS=mac_flags)

# Generate in-editor GDExtension class reference documentation
doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("project/docs/classref/*.xml"))

# ==============================================================================
# WARNING TO DEVELOPERS: Layer Separation Enforcement
# Core (`src/core/`) and Native (`src/native/`) layers must remain 100%
# independent of the Godot engine. DO NOT add `godot-cpp` includes or
# dependencies to these layers. They are compiled below using `core_env`
# which deliberately lacks any Godot includes to enforce this boundary.
# ==============================================================================

# Compile Core objects using core_env (no Godot dependencies allowed)
core_objs = []
for s in Glob("src/core/*.cpp"):
    rel_path = os.path.relpath(str(s), "src")
    obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".cpp", core_env["SHOBJSUFFIX"]))
    core_objs.append(core_env.SharedObject(target=obj_file, source=s))

# Compile Native objects using core_env (no Godot dependencies allowed)
native_objs = []
if env["platform"] != "web":
    native_excludes = [
        "godot_camera.cpp",
        "gaze_processors.cpp",
        "gaze_tracker_native.cpp",
        "vision_server.cpp",
        "wmf_camera.cpp"
    ]
    
    for s in Glob("src/native/*.cpp"):
        filename = os.path.basename(str(s))
        if filename in native_excludes:
            continue
        if filename.endswith(".fallback.cpp"):
            continue
        rel_path = os.path.relpath(str(s), "src")
        obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".cpp", core_env["SHOBJSUFFIX"]))
        native_objs.append(core_env.SharedObject(target=obj_file, source=s))

    if env["platform"] not in ["windows", "android"]:
        for s in Glob("src/native/*.fallback.cpp"):
            rel_path = os.path.relpath(str(s), "src")
            obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".fallback.cpp", core_env["SHOBJSUFFIX"]))
            native_objs.append(core_env.SharedObject(target=obj_file, source=s))
    # TODO: I'm ok retaining this "just in case", but do we still ahve this? In any case, would MUCH rather have a native_macos or native_apple (or both!) targeet allow .mm than include it in native arbitrarily. See windows below, same idea.
    if env["platform"] in ["macos", "ios"]:
        for s in Glob("src/native/*.mm"):
            rel_path = os.path.relpath(str(s), "src")
            obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".mm", core_env["SHOBJSUFFIX"]))
            native_objs.append(core_env.SharedObject(target=obj_file, source=s))

# Compile Windows objects using core_env (selective on platform)
windows_objs = []
if env["platform"] == "windows":
    for s in Glob("src/windows/*.cpp"):
        rel_path = os.path.relpath(str(s), "src")
        obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".cpp", core_env["SHOBJSUFFIX"]))
        windows_objs.append(core_env.SharedObject(target=obj_file, source=s))

# Compile Android objects using core_env (selective on platform)
android_objs = []
if env["platform"] == "android":
    for s in Glob("src/android/*.cpp"):
        rel_path = os.path.relpath(str(s), "src")
        obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".cpp", core_env["SHOBJSUFFIX"]))
        android_objs.append(core_env.SharedObject(target=obj_file, source=s))

# Compile Godot objects using env (contains godot-cpp)
godot_objs = []
for s in Glob("src/godot/*.cpp"):
    rel_path = os.path.relpath(str(s), "src")
    obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".cpp", env["SHOBJSUFFIX"]))
    godot_objs.append(env.SharedObject(target=obj_file, source=s))

# Compile Web objects using env (contains godot-cpp)
# TODO: Can you explain this? web should also not depend directly on godot ideally; similar to native, it should include code which runs on web and enforces our system requirements, while some other layer provides godot bindings.
#       Note this may require analysis; the web platform may _require_ special handling from godot in a way native doesn't. But I would like to understand that better if so!
web_objs = []
if env["platform"] == "web":
    for s in Glob("src/web/*.cpp"):
        rel_path = os.path.relpath(str(s), "src")
        obj_file = os.path.join(gaze_variant_dir, rel_path.replace(".cpp", env["SHOBJSUFFIX"]))
        web_objs.append(env.SharedObject(target=obj_file, source=s))
    env.Append(CPPDEFINES=["WEB_ENABLED"])

# Compile generated doc data using env
doc_obj = env.SharedObject(target=os.path.join(gaze_variant_dir, "gen/doc_data.gen" + env["SHOBJSUFFIX"]), source=doc_data)

# Compile thirdparty Objective-C files on macOS/iOS using core_env
# TODO: Can this be moved into a putative macos/, ios/, or apple/ ("both") SConstruct file and set of targets?
#       The sketch is that macos deps apple deps native, ios deps apple deps native.
thirdparty_objc_objs = []
if env["platform"] in ["macos", "ios"]:
    thirdparty_objc_sources = [
        os.path.join(gaze_variant_dir, "thirdparty_objc/ort_value.mm"),
        os.path.join(gaze_variant_dir, "thirdparty_objc/ort_enums.mm"),
        os.path.join(gaze_variant_dir, "thirdparty_objc/error_utils.mm")
    ]
    for s in thirdparty_objc_sources:
        thirdparty_objc_objs.append(core_env.SharedObject(source=s))

# Combine all library objects
library_objs = core_objs + native_objs + windows_objs + android_objs + godot_objs + web_objs + [doc_obj] + thirdparty_objc_objs

# Output library name mapping
if env["platform"] == "web":
    env.Replace(SHLIBPREFIX="")

# TODO: Ahah! yeah, duplicated in onnx. Can this be done earlier/better so that these conventions are just set in stone?
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

lib_threads_suffix = ".threads" if (env["platform"] == "web" and env.get("threads", False)) else ""
file_platform = "javascript" if env["platform"] == "web" else env["platform"]

if env["platform"] == "android":
    target_lib = f"project/addons/godot-gaze/bin/gaze.android.{env['target']}.{env['arch']}{lib_suffix}"
elif env["platform"] == "ios":
    sim_part = ".simulator" if env.get("ios_simulator", False) else ""
    target_lib = f"project/addons/godot-gaze/bin/gaze.ios.{env['target']}{sim_part}.{env['arch']}{lib_suffix}"
elif env["platform"] in ["windows", "linux"]:
    target_lib = f"project/addons/godot-gaze/bin/gaze.{file_platform}.{env['target']}.{env['arch']}{lib_threads_suffix}{lib_suffix}"
else:
    target_lib = f"project/addons/godot-gaze/bin/gaze.{file_platform}.{env['target']}{lib_threads_suffix}{lib_suffix}"

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

# Create GDExtension library builder (Static on iOS, Shared on other platforms)
if env["platform"] == "ios":
    library = env.StaticLibrary(target=target_lib, source=library_objs)
else:
    library = env.SharedLibrary(target=target_lib, source=library_objs)

# Ad-hoc codesigning on macOS to satisfy Gatekeeper and SIP
if env["platform"] == "macos":
    env.AddPostAction(library, "codesign -s - --force $TARGET")

env.Default(library)

# Load tests/SConscript unconditionally to expose testing targets
SConscript("tests/SConscript", exports="core_env env library target_lib gaze_variant_dir")
