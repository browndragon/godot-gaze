#include "doctest.h"
#include "ort_eye_state_model.hpp"
#include <fstream>
#include <vector>
#include <cmath>

inline bool eye_state_file_exists(const std::string &filename) {
    std::ifstream f(filename.c_str());
    return f.good();
}

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TEST_CASE("ORT Eye State Classifier Boundary Invariants and Signal Separation")
{
    std::string model_path = "project/addons/godot-gaze/models/open_closed_eye.ort";
    if (!eye_state_file_exists(model_path)) {
        model_path = "../project/addons/godot-gaze/models/open_closed_eye.ort";
    }

    if (!eye_state_file_exists(model_path)) {
        MESSAGE("Skipping ORT Eye State Classifier test: Model file not found");
        return;
    }

    Gaze::ORTEyeStateModel model(model_path);
    REQUIRE(model.initialize() == true);

    auto load_crop = [](const std::string &path, int crop_x, int crop_y, int crop_s) -> std::vector<uint8_t> {
        int w = 0, h = 0, c = 0;
        unsigned char *data = stbi_load(path.c_str(), &w, &h, &c, 3);
        if (!data) {
            std::string fb = "../" + path;
            data = stbi_load(fb.c_str(), &w, &h, &c, 3);
        }
        std::vector<uint8_t> out(60 * 60 * 3, 128);
        if (!data) return out;

        for (int y = 0; y < 60; ++y) {
            int src_y = crop_y + (y * crop_s) / 60;
            src_y = std::max(0, std::min(src_y, h - 1));
            for (int x = 0; x < 60; ++x) {
                int src_x = crop_x + (x * crop_s) / 60;
                src_x = std::max(0, std::min(src_x, w - 1));
                int src_idx = (src_y * w + src_x) * 3;
                int dst_idx = (y * 60 + x) * 3;
                out[dst_idx + 0] = data[src_idx + 2]; // B
                out[dst_idx + 1] = data[src_idx + 1]; // G
                out[dst_idx + 2] = data[src_idx + 0]; // R
            }
        }
        stbi_image_free(data);
        return out;
    };

    // Load open eye from eyes_both_open.jpg and closed eye from eyes_both_wink.jpg
    auto open_crop = load_crop("tests/resources/eyes_both_open.jpg", 680, 420, 80);
    auto closed_crop = load_crop("tests/resources/eyes_both_wink.jpg", 680, 420, 80);
    auto r_wink_crop = load_crop("tests/resources/eyes_anatomical_right_wink.jpg", 680, 420, 80);
    auto l_wink_crop = load_crop("tests/resources/eyes_anatomical_left_wink.jpg", 760, 420, 80);

    float open_score = 0.0f;
    float closed_score = 0.0f;
    float r_wink_score = 0.0f;
    float l_wink_score = 0.0f;

    bool ok1 = model.estimate_openness(open_crop.data(), open_score);
    bool ok2 = model.estimate_openness(closed_crop.data(), closed_score);
    bool ok3 = model.estimate_openness(r_wink_crop.data(), r_wink_score);
    bool ok4 = model.estimate_openness(l_wink_crop.data(), l_wink_score);

    REQUIRE(ok1 == true);
    REQUIRE(ok2 == true);
    REQUIRE(ok3 == true);
    REQUIRE(ok4 == true);

    // Assert probability bounds [0.0, 1.0]
    CHECK(open_score >= 0.0f);
    CHECK(open_score <= 1.0f);
    CHECK(closed_score >= 0.0f);
    CHECK(closed_score <= 1.0f);

    // Assert strict domain bounds
    CHECK(open_score >= 0.70f);
    CHECK(closed_score <= 0.20f);
    CHECK(r_wink_score <= 0.25f);
    CHECK(l_wink_score <= 0.25f);
}
