#include "web_gaze_model.hpp"

namespace Gaze {

WebGazeModel::WebGazeModel() : fed_gaze_dir(0.0, 0.0, -1.0), has_new_data(false) {}

bool WebGazeModel::initialize() {
    // Web initialization is handled browser-side in JavaScript (ONNX Runtime Web)
    return true;
}

bool WebGazeModel::estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_gaze_dir_cv) {
    // Simply returns the latest vector fed by the JavaScript sidecar
    out_gaze_dir_cv = fed_gaze_dir;
    return true;
}

void WebGazeModel::feed_raw_gaze(const GazeVector3& raw_gaze) {
    fed_gaze_dir = raw_gaze.normalized();
    has_new_data = true;
}

} // namespace Gaze
