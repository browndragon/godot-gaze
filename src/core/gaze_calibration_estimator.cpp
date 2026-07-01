/**
 * @file gaze_calibration_estimator.cpp
 * @brief Implement Nelder-Mead simplex optimization for screen geometry calibration in millimeter space (Layer 4)
 */
#include "gaze_calibration_estimator.hpp"
#include <cmath>
#include <algorithm>

namespace Gaze {

struct Vertex {
    double x[6];
    double fx;
};

static double compute_loss(
    const std::vector<CalibrationSample>& samples,
    const GazeVector2& screen_size_mm,
    const GazeVector3& initial_camera_offset,
    double initial_camera_tilt_deg,
    bool freeze_camera_params,
    const double params[6],
    const CalibrationWeights& weights
) {
    GazeVector3 camera_offset;
    double camera_tilt;
    if (freeze_camera_params) {
        camera_offset = initial_camera_offset;
        camera_tilt = initial_camera_tilt_deg;
    } else {
        camera_offset = GazeVector3(params[0], params[1], params[2]);
        camera_tilt = params[3];
    }
    double bias_pitch = params[4];
    double bias_yaw = params[5];

    // Penalty for out of bounds parameters
    double penalty = 0.0;
    double pm = weights.penalty_multiplier;

    if (!freeze_camera_params) {
        if (camera_offset.x < -weights.max_camera_offset_x) { penalty += pm * std::pow(-weights.max_camera_offset_x - camera_offset.x, 2); camera_offset.x = -weights.max_camera_offset_x; }
        if (camera_offset.x > weights.max_camera_offset_x)  { penalty += pm * std::pow(camera_offset.x - weights.max_camera_offset_x, 2);  camera_offset.x = weights.max_camera_offset_x; }
        if (camera_offset.y < weights.min_camera_offset_y) { penalty += pm * std::pow(weights.min_camera_offset_y - camera_offset.y, 2); camera_offset.y = weights.min_camera_offset_y; }
        if (camera_offset.y > weights.max_camera_offset_y)  { penalty += pm * std::pow(camera_offset.y - weights.max_camera_offset_y, 2);  camera_offset.y = weights.max_camera_offset_y; }
        if (camera_offset.z < weights.min_camera_offset_z) { penalty += pm * std::pow(weights.min_camera_offset_z - camera_offset.z, 2); camera_offset.z = weights.min_camera_offset_z; }
        if (camera_offset.z > weights.max_camera_offset_z)  { penalty += pm * std::pow(camera_offset.z - weights.max_camera_offset_z, 2);  camera_offset.z = weights.max_camera_offset_z; }

        if (camera_tilt < -weights.max_camera_tilt) { penalty += pm * std::pow(-weights.max_camera_tilt - camera_tilt, 2); camera_tilt = -weights.max_camera_tilt; }
        if (camera_tilt > weights.max_camera_tilt)  { penalty += pm * std::pow(camera_tilt - weights.max_camera_tilt, 2);  camera_tilt = weights.max_camera_tilt; }
    }

    if (bias_pitch < -weights.max_bias) { penalty += pm * std::pow(-weights.max_bias - bias_pitch, 2); bias_pitch = -weights.max_bias; }
    if (bias_pitch > weights.max_bias)  { penalty += pm * std::pow(bias_pitch - weights.max_bias, 2);  bias_pitch = weights.max_bias; }
    if (bias_yaw < -weights.max_bias)   { penalty += pm * std::pow(-weights.max_bias - bias_yaw, 2);   bias_yaw = -weights.max_bias; }
    if (bias_yaw > weights.max_bias)    { penalty += pm * std::pow(bias_yaw - weights.max_bias, 2);    bias_yaw = weights.max_bias; }

    double total_err_sq = 0.0;
    double theta_rad = camera_tilt * DEG_TO_RAD;
    double cos_t = std::cos(theta_rad);
    double sin_t = std::sin(theta_rad);

    double W_half = screen_size_mm.x * 0.5;
    double H_half = screen_size_mm.y * 0.5;

    for (const auto& sample : samples) {
        GazeVector3 v = sample.gaze_direction.normalized();

        double vy = v.y;
        if (vy > 1.0) vy = 1.0;
        else if (vy < -1.0) vy = -1.0;

        double yaw = std::atan2(v.x, v.z);
        double pitch = std::asin(vy);

        double calib_yaw = yaw + bias_yaw;
        double calib_pitch = pitch + bias_pitch;

        double cos_pitch = std::cos(calib_pitch);
        GazeVector3 biased_dir = GazeVector3(
            std::sin(calib_yaw) * cos_pitch,
            std::sin(calib_pitch),
            std::cos(calib_yaw) * cos_pitch
        ).normalized();

        // Project ray onto tilted screen plane (Display Space Z = 0)
        double O_disp_z = sin_t * sample.gaze_origin.y - cos_t * sample.gaze_origin.z + camera_offset.z;
        double v_disp_z = sin_t * biased_dir.y - cos_t * biased_dir.z;

        if (std::abs(v_disp_z) < 1e-6) {
            total_err_sq += weights.penalty_multiplier;
            continue;
        }

        double t = -O_disp_z / v_disp_z;
        if (t < 0.0) {
            total_err_sq += weights.penalty_multiplier;
            continue;
        }

        double O_disp_x = -sample.gaze_origin.x + camera_offset.x + W_half;
        double O_disp_y = -(cos_t * sample.gaze_origin.y + sin_t * sample.gaze_origin.z + camera_offset.y) + H_half;

        double v_disp_x = -biased_dir.x;
        double v_disp_y = -(cos_t * biased_dir.y + sin_t * biased_dir.z);

        double projected_x = O_disp_x + v_disp_x * t;
        double projected_y = O_disp_y + v_disp_y * t;

        double dx = projected_x - sample.target_pos_mm.x;
        double dy = projected_y - sample.target_pos_mm.y;
        total_err_sq += dx * dx + dy * dy;
    }

    // Regularization / Priors (soft penalties for deviating too far)
    double offset_prior_x = camera_offset.x - initial_camera_offset.x;
    double offset_prior_y = camera_offset.y - initial_camera_offset.y;
    double offset_prior_z = camera_offset.z - initial_camera_offset.z;
    double tilt_prior = camera_tilt - initial_camera_tilt_deg;

    double reg_loss = 0.0;
    if (!freeze_camera_params) {
        reg_loss += weights.offset_x * std::pow(offset_prior_x, 2);
        reg_loss += weights.offset_y * std::pow(offset_prior_y, 2);
        reg_loss += weights.offset_z * std::pow(offset_prior_z, 2);
        reg_loss += weights.tilt * std::pow(tilt_prior, 2);
    }

    // Small bias regularization
    reg_loss += weights.bias * (std::pow(bias_pitch, 2) + std::pow(bias_yaw, 2));

    return total_err_sq + reg_loss + penalty;
}

bool CalibrationEstimator::estimate(
    const std::vector<CalibrationSample>& samples,
    const GazeVector2& screen_size_mm,
    const GazeVector3& initial_camera_offset,
    double initial_camera_tilt_deg,
    bool freeze_camera_params,
    GazeVector3& out_camera_offset,
    double& out_camera_tilt_deg,
    double& out_bias_pitch,
    double& out_bias_yaw,
    const CalibrationWeights& weights
) {
    if (samples.empty()) {
        return false;
    }

    constexpr int D = 6;
    Vertex simplex[D + 1];

    // Initial vertex (x0)
    simplex[0].x[0] = initial_camera_offset.x;
    simplex[0].x[1] = initial_camera_offset.y;
    simplex[0].x[2] = initial_camera_offset.z;
    simplex[0].x[3] = initial_camera_tilt_deg;
    simplex[0].x[4] = 0.0; // initial bias pitch
    simplex[0].x[5] = 0.0; // initial bias yaw

    simplex[0].fx = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, simplex[0].x, weights);

    // Step size for each parameter
    double steps[D] = {
        freeze_camera_params ? 0.0 : weights.step_camera_offset, // camera offset x (mm)
        freeze_camera_params ? 0.0 : weights.step_camera_offset, // camera offset y (mm)
        freeze_camera_params ? 0.0 : weights.step_camera_offset, // camera offset z (mm)
        freeze_camera_params ? 0.0 : weights.step_camera_tilt,   // camera tilt (deg)
        weights.step_bias,                                       // bias pitch (rad)
        weights.step_bias                                        // bias yaw (rad)
    };

    // Initialize the other D vertices
    for (int i = 1; i <= D; ++i) {
        for (int j = 0; j < D; ++j) {
            simplex[i].x[j] = simplex[0].x[j];
        }
        simplex[i].x[i - 1] += steps[i - 1];
        simplex[i].fx = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, simplex[i].x, weights);
    }

    // Optimization loop
    for (int iter = 0; iter < weights.max_iterations; ++iter) {
        // Sort simplex vertices so fx is ascending
        for (int i = 0; i < D; ++i) {
            for (int j = i + 1; j <= D; ++j) {
                if (simplex[i].fx > simplex[j].fx) {
                    std::swap(simplex[i], simplex[j]);
                }
            }
        }

        // Convergence check: if simplex range is very small
        if (simplex[D].fx - simplex[0].fx < weights.convergence_threshold) {
            break;
        }

        // Compute centroid of best D vertices
        double centroid[D] = {0.0};
        for (int j = 0; j < D; ++j) {
            for (int i = 0; i < D; ++i) {
                centroid[j] += simplex[i].x[j];
            }
            centroid[j] /= D;
        }

        // Reflection
        double reflected[D];
        for (int j = 0; j < D; ++j) {
            reflected[j] = centroid[j] + 1.0 * (centroid[j] - simplex[D].x[j]);
        }
        double f_reflected = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, reflected, weights);

        if (simplex[0].fx <= f_reflected && f_reflected < simplex[D - 1].fx) {
            for (int j = 0; j < D; ++j) simplex[D].x[j] = reflected[j];
            simplex[D].fx = f_reflected;
            continue;
        }

        // Expansion
        if (f_reflected < simplex[0].fx) {
            double expanded[D];
            for (int j = 0; j < D; ++j) {
                expanded[j] = centroid[j] + 2.0 * (reflected[j] - centroid[j]);
            }
            double f_expanded = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, expanded, weights);

            if (f_expanded < f_reflected) {
                for (int j = 0; j < D; ++j) simplex[D].x[j] = expanded[j];
                simplex[D].fx = f_expanded;
            } else {
                for (int j = 0; j < D; ++j) simplex[D].x[j] = reflected[j];
                simplex[D].fx = f_reflected;
            }
            continue;
        }

        // Contraction
        if (f_reflected >= simplex[D - 1].fx) {
            if (simplex[D - 1].fx <= f_reflected && f_reflected < simplex[D].fx) {
                double contracted[D];
                for (int j = 0; j < D; ++j) {
                    contracted[j] = centroid[j] + 0.5 * (reflected[j] - centroid[j]);
                }
                double f_contracted = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, contracted, weights);
                if (f_contracted <= f_reflected) {
                    for (int j = 0; j < D; ++j) simplex[D].x[j] = contracted[j];
                    simplex[D].fx = f_contracted;
                    continue;
                }
            } else {
                double contracted[D];
                for (int j = 0; j < D; ++j) {
                    contracted[j] = centroid[j] - 0.5 * (centroid[j] - simplex[D].x[j]);
                }
                double f_contracted = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, contracted, weights);
                if (f_contracted < simplex[D].fx) {
                    for (int j = 0; j < D; ++j) simplex[D].x[j] = contracted[j];
                    simplex[D].fx = f_contracted;
                    continue;
                }
            }
        }

        // Shrink simplex
        for (int i = 1; i <= D; ++i) {
            for (int j = 0; j < D; ++j) {
                simplex[i].x[j] = simplex[0].x[j] + 0.5 * (simplex[i].x[j] - simplex[0].x[j]);
            }
            simplex[i].fx = compute_loss(samples, screen_size_mm, initial_camera_offset, initial_camera_tilt_deg, freeze_camera_params, simplex[i].x, weights);
        }
    }

    // Assign optimal values from the best vertex
    if (freeze_camera_params) {
        out_camera_offset = initial_camera_offset;
        out_camera_tilt_deg = initial_camera_tilt_deg;
    } else {
        out_camera_offset.x = simplex[0].x[0];
        out_camera_offset.y = simplex[0].x[1];
        out_camera_offset.z = simplex[0].x[2];
        out_camera_tilt_deg = simplex[0].x[3];
    }
    out_bias_pitch = simplex[0].x[4];
    out_bias_yaw = simplex[0].x[5];

    return true;
}

} // namespace Gaze
