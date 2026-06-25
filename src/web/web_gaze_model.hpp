// Receives pre-calculated gaze values from browser JavaScript.
#pragma once

#include "gaze_model.hpp"

namespace Gaze {

class WebGazeModel : public GazeModel {
private:
    GazeVector3 fed_gaze_dir;
    bool has_new_data;

public:
    WebGazeModel();
    virtual ~WebGazeModel() = default;

    virtual bool initialize() override;
    virtual bool estimate_raw_gaze(const EyeCrops& crops, GazeVector3& out_gaze_dir_cv) override;

    // Interface method called by the JS Sidecar via Emscripten JS Bridge
    void feed_raw_gaze(const GazeVector3& raw_gaze);
};

} // namespace Gaze
