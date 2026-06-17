#!/usr/bin/env bash
# scripts/download_opencv.sh
# Automates the retrieval and setup of OpenCV dependencies for godot-gaze compilation.

set -euo pipefail

OPENCV_VERSION="4.10.0" # Target stable version
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRDPARTY_DIR="${BASE_DIR}/thirdparty/opencv"

echo "=== Gaze Tracker OpenCV Dependency Downloader ==="
echo "Targeting OpenCV version: ${OPENCV_VERSION}"
mkdir -p "${THIRDPARTY_DIR}"

# Detect OS
OS_NAME="$(uname -s)"
case "${OS_NAME}" in
    Darwin)
        echo "Detected macOS host."
        # Check if brew is installed and has opencv
        if command -v brew &> /dev/null; then
            BREW_PREFIX="$(brew --prefix)"
            OPENCV_BREW_DIR="${BREW_PREFIX}/opt/opencv"
            if [ -d "${OPENCV_BREW_DIR}" ]; then
                echo "Found Homebrew OpenCV at: ${OPENCV_BREW_DIR}"
                echo "Creating local symlink under thirdparty/opencv/macos..."
                rm -f "${THIRDPARTY_DIR}/macos"
                ln -s "${OPENCV_BREW_DIR}" "${THIRDPARTY_DIR}/macos"
                echo "macOS OpenCV setup completed."
            else
                echo "Warning: OpenCV not found in Homebrew. Run 'brew install opencv' or set OPENCV_DIR manually."
            fi
        else
            echo "Warning: Homebrew not detected. Please install OpenCV manually and point OPENCV_DIR to its root."
        fi
        ;;
    Linux)
        echo "Detected Linux host."
        echo "Please install OpenCV via your package manager (e.g., sudo apt install libopencv-dev)."
        echo "Or build it from source and link via OPENCV_DIR."
        ;;
    *)
        echo "Unsupported host platform for auto-symlinking: ${OS_NAME}"
        ;;
esac

# Function to download and extract Android SDK
setup_android() {
    local sdk_url="https://github.com/opencv/opencv/releases/download/${OPENCV_VERSION}/opencv-${OPENCV_VERSION}-android-sdk.zip"
    local dest_zip="${THIRDPARTY_DIR}/opencv-android.zip"
    local extract_dir="${THIRDPARTY_DIR}/android_temp"

    echo "Downloading OpenCV Android SDK..."
    curl -L -o "${dest_zip}" "${sdk_url}"
    echo "Extracting Android SDK..."
    unzip -q -o "${dest_zip}" -d "${extract_dir}"
    
    # Move out of inner folder
    rm -rf "${THIRDPARTY_DIR}/android"
    mv "${extract_dir}/OpenCV-android-sdk" "${THIRDPARTY_DIR}/android"
    
    # Cleanup
    rm -f "${dest_zip}"
    rm -rf "${extract_dir}"
    echo "Android SDK setup completed at thirdparty/opencv/android."
}

# Function to download and extract iOS Framework
setup_ios() {
    local framework_url="https://github.com/opencv/opencv/releases/download/${OPENCV_VERSION}/opencv-${OPENCV_VERSION}-ios-framework.zip"
    local dest_zip="${THIRDPARTY_DIR}/opencv-ios.zip"
    local extract_dir="${THIRDPARTY_DIR}/ios_temp"

    echo "Downloading OpenCV iOS Framework..."
    curl -L -o "${dest_zip}" "${framework_url}"
    echo "Extracting iOS Framework..."
    unzip -q -o "${dest_zip}" -d "${extract_dir}"
    
    # Move out of inner folder
    rm -rf "${THIRDPARTY_DIR}/ios"
    mv "${extract_dir}" "${THIRDPARTY_DIR}/ios"
    
    # Cleanup
    rm -f "${dest_zip}"
    echo "iOS SDK setup completed at thirdparty/opencv/ios."
}

# Check command line arguments for mobile SDK setup
if [ $# -gt 0 ]; then
    for arg in "$@"; do
        if [ "${arg}" == "--android" ]; then
            setup_android
        elif [ "${arg}" == "--ios" ]; then
            setup_ios
        else
            echo "Unknown argument: ${arg}"
            echo "Usage: $0 [--android] [--ios]"
            exit 1
        fi
    done
else
    echo "Run with '--android' or '--ios' if you need to fetch prebuilt mobile libraries."
fi

echo "Done!"
