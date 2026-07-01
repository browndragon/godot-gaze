#pragma once

#include "math_defs.hpp"
#include <vector>

namespace Gaze {

/**
 * @brief Solves the Perspective-n-Point (PnP) problem for 5-point face landmarks using
 * Levenberg-Marquardt optimization.
 * 
 * @param model_points The 3D canonical landmarks (size 5).
 * @param image_points The 2D image landmarks (size 5).
 * @param fx Focal length along X.
 * @param fy Focal length along Y.
 * @param cx Principal point X.
 * @param cy Principal point Y.
 * @param rvec Input/Output rotation vector.
 * @param tvec Input/Output translation vector.
 * @param use_extrinsic_guess If true, uses the input values of rvec/tvec as initial guess.
 * @return true if the solver converged successfully, false otherwise.
 */
bool solve_pnp_lm(
    const std::vector<GazeVector3>& model_points,
    const std::vector<GazeVector2>& image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3& rvec, GazeVector3& tvec,
    bool use_extrinsic_guess = true
);

} // namespace Gaze
