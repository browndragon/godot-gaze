/**
 * @file pnp_solver.hpp
 * @brief Perspective-n-Point Pose Estimation Interface
 *
 * Provides high-level PnP solver dispatch over 3D model points and 2D image coordinates.
 */
#pragma once

#include "math_defs.hpp"
#include "sqpnp_solver.hpp"
#include <vector>

namespace Gaze
{

/**
 * @brief Solves the Perspective-n-Point pose using the globally optimal SQPnP algorithm.
 * @param model_points 3D points in model space.
 * @param image_points 2D points in pixel coordinates.
 * @param fx Focal length X.
 * @param fy Focal length Y.
 * @param cx Principal point X.
 * @param cy Principal point Y.
 * @param rvec Output/Input Rodrigues rotation vector in camera space.
 * @param tvec Output/Input Translation vector in camera space.
 * @param use_extrinsic_guess Whether to seed with initial extrinsic guess (optional in SQPnP).
 * @return true if convergence succeeded.
 */
inline bool solve_pnp_lm(
    const std::vector<GazeVector3> &model_points,
    const std::vector<GazeVector2> &image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3 &rvec, GazeVector3 &tvec,
    bool use_extrinsic_guess = false)
{
    (void)use_extrinsic_guess;
    return SQPnPSolver::solve_rvec(model_points, image_points, fx, fy, cx, cy, rvec, tvec);
}

inline bool solve_pnp(
    const std::vector<GazeVector3> &model_points,
    const std::vector<GazeVector2> &image_points,
    double fx, double fy, double cx, double cy,
    GazeBasis3D &rotation, GazeVector3 &translation)
{
    return SQPnPSolver::solve(model_points, image_points, fx, fy, cx, cy, rotation, translation);
}

} // namespace Gaze
