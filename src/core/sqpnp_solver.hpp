/**
 * @file sqpnp_solver.hpp
 * @brief Self-Contained Globally Optimal Sequential Quadratic Programming PnP (SQPnP) Solver
 *
 * Implements the SQPnP algorithm from:
 * "A Consistently Fast and Globally Optimal Solution to the Perspective-n-Point Problem"
 * by G. Terzakis and M. Lourakis (ECCV 2020).
 *
 * Licensed under the BSD 3-Clause License (Copyright 2020 George Terzakis).
 */
#pragma once

#include "math_defs.hpp"
#include <vector>

namespace Gaze
{

class SQPnPSolver
{
public:
    struct Solution
    {
        GazeBasis3D rotation;
        GazeVector3 translation;
        double squared_error = 0.0;
    };

    /**
     * @brief Solves the PnP problem given 3D object points and corresponding 2D image points.
     * @param object_points 3D points in model space (minimum 4 points).
     * @param image_points 2D image points in pixels.
     * @param fx Focal length X.
     * @param fy Focal length Y.
     * @param cx Principal point X.
     * @param cy Principal point Y.
     * @param out_rotation Output optimal rotation basis.
     * @param out_translation Output optimal translation vector in camera space (+X right, +Y down, +Z forward).
     * @return true if a valid pose solution was found.
     */
    static bool solve(
        const std::vector<GazeVector3> &object_points,
        const std::vector<GazeVector2> &image_points,
        double fx, double fy, double cx, double cy,
        GazeBasis3D &out_rotation,
        GazeVector3 &out_translation);

    static bool solve_rvec(
        const std::vector<GazeVector3> &object_points,
        const std::vector<GazeVector2> &image_points,
        double fx, double fy, double cx, double cy,
        GazeVector3 &out_rvec,
        GazeVector3 &out_translation);
};

} // namespace Gaze
