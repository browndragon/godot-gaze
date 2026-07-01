/**
 * @dir src/windows
 * @brief Windows-specific capture implementations and dependencies.
 *
 * This directory contains Windows-specific camera capture implementations (e.g. WMFCamera) using
 * Windows Media Foundation APIs. It is compiled selectively only when targeting Windows.
 */

# Windows Native Platform Layer

This directory isolates the Windows Media Foundation (WMF) camera capture logic (`wmf_camera.cpp/hpp`) from the rest of the cross-platform codebase.
