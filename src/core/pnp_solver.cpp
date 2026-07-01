#include "pnp_solver.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Gaze {

// Helper to project a single 3D point using Rodrigues rotation vector and translation vector
static GazeVector2 project_point(const GazeVector3& P, const GazeVector3& r, const GazeVector3& t, double fx, double fy, double cx, double cy) {
    GazeBasis3D R = rodrigues_to_basis(r);
    GazeVector3 P_cam = R.multiply_vector(P) + t;
    if (std::abs(P_cam.z) < 1e-6) {
        return GazeVector2(cx, cy);
    }
    return GazeVector2(
        fx * (P_cam.x / P_cam.z) + cx,
        fy * (P_cam.y / P_cam.z) + cy
    );
}

// Computes residuals (image_points - projected_points) and returns Sum of Squared Errors (SSE)
static double compute_residuals(
    const std::vector<GazeVector3>& model_points,
    const std::vector<GazeVector2>& image_points,
    const double beta[6],
    double fx, double fy, double cx, double cy,
    double residuals[10]
) {
    GazeVector3 r(beta[0], beta[1], beta[2]);
    GazeVector3 t(beta[3], beta[4], beta[5]);
    double sse = 0.0;
    for (size_t i = 0; i < 5; ++i) {
        GazeVector2 p_proj = project_point(model_points[i], r, t, fx, fy, cx, cy);
        residuals[2 * i] = image_points[i].x - p_proj.x;
        residuals[2 * i + 1] = image_points[i].y - p_proj.y;
        sse += residuals[2 * i] * residuals[2 * i] + residuals[2 * i + 1] * residuals[2 * i + 1];
    }
    return sse;
}

// Computes Jacobian of the projection function with respect to beta parameters using numerical differentiation
static void compute_jacobian(
    const std::vector<GazeVector3>& model_points,
    const double beta[6],
    double fx, double fy, double cx, double cy,
    double J[10][6]
) {
    const double eps = 1e-4;
    double perturbed_beta[6];
    for (int i = 0; i < 6; ++i) {
        perturbed_beta[i] = beta[i];
    }
    for (int j = 0; j < 6; ++j) {
        // Perturb positively
        perturbed_beta[j] = beta[j] + eps;
        GazeVector3 r_pos(perturbed_beta[0], perturbed_beta[1], perturbed_beta[2]);
        GazeVector3 t_pos(perturbed_beta[3], perturbed_beta[4], perturbed_beta[5]);

        // Perturb negatively
        perturbed_beta[j] = beta[j] - eps;
        GazeVector3 r_neg(perturbed_beta[0], perturbed_beta[1], perturbed_beta[2]);
        GazeVector3 t_neg(perturbed_beta[3], perturbed_beta[4], perturbed_beta[5]);

        perturbed_beta[j] = beta[j]; // restore

        for (size_t i = 0; i < 5; ++i) {
            GazeVector2 proj_pos = project_point(model_points[i], r_pos, t_pos, fx, fy, cx, cy);
            GazeVector2 proj_neg = project_point(model_points[i], r_neg, t_neg, fx, fy, cx, cy);
            
            // Central difference for higher precision
            J[2 * i][j] = (proj_pos.x - proj_neg.x) / (2.0 * eps);
            J[2 * i + 1][j] = (proj_pos.y - proj_neg.y) / (2.0 * eps);
        }
    }
}

// Solves a 6x6 linear system A * x = b using Gaussian Elimination with partial pivoting
static bool solve6x6(double A[6][6], const double b[6], double x[6]) {
    double temp[6][7];
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            temp[i][j] = A[i][j];
        }
        temp[i][6] = b[i];
    }
    for (int i = 0; i < 6; ++i) {
        int pivot = i;
        for (int r = i + 1; r < 6; ++r) {
            if (std::abs(temp[r][i]) > std::abs(temp[pivot][i])) {
                pivot = r;
            }
        }
        if (std::abs(temp[pivot][i]) < 1e-12) {
            return false;
        }
        if (pivot != i) {
            for (int c = i; c <= 6; ++c) {
                std::swap(temp[i][c], temp[pivot][c]);
            }
        }
        for (int r = i + 1; r < 6; ++r) {
            double factor = temp[r][i] / temp[i][i];
            for (int c = i; c <= 6; ++c) {
                temp[r][c] -= factor * temp[i][c];
            }
        }
    }
    for (int i = 5; i >= 0; --i) {
        double sum = temp[i][6];
        for (int j = i + 1; j < 6; ++j) {
            sum -= temp[i][j] * x[j];
        }
        x[i] = sum / temp[i][i];
    }
    return true;
}

bool solve_pnp_lm(
    const std::vector<GazeVector3>& model_points,
    const std::vector<GazeVector2>& image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3& rvec, GazeVector3& tvec,
    bool use_extrinsic_guess
) {
    if (model_points.size() != 5 || image_points.size() != 5) {
        return false;
    }

    double beta[6];
    if (use_extrinsic_guess) {
        beta[0] = rvec.x;
        beta[1] = rvec.y;
        beta[2] = rvec.z;
        beta[3] = tvec.x;
        beta[4] = tvec.y;
        beta[5] = tvec.z;
    } else {
        beta[0] = 0.0;
        beta[1] = 0.0;
        beta[2] = 0.0;
        beta[3] = 0.0;
        beta[4] = 0.0;
        beta[5] = 700.0;
    }

    double residuals[10];
    double sse = compute_residuals(model_points, image_points, beta, fx, fy, cx, cy, residuals);

    bool has_converged = (sse < 1.0);
    double lambda = 0.001;
    const int max_iter = 100;

    for (int iter = 0; iter < max_iter; ++iter) {
        double J[10][6];
        compute_jacobian(model_points, beta, fx, fy, cx, cy, J);

        double JTJ[6][6];
        double JTe[6];
        for (int i = 0; i < 6; ++i) {
            JTe[i] = 0.0;
            for (int r = 0; r < 10; ++r) {
                JTe[i] += J[r][i] * residuals[r];
            }
            for (int j = 0; j < 6; ++j) {
                JTJ[i][j] = 0.0;
                for (int r = 0; r < 10; ++r) {
                    JTJ[i][j] += J[r][i] * J[r][j];
                }
            }
        }

        bool solved = false;
        double beta_new[6];
        double delta[6];

        // LM Inner loop: try steps with increasing lambda until SSE decreases
        while (!solved && lambda < 1e10) {
            double A[6][6];
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 6; ++j) {
                    A[i][j] = JTJ[i][j];
                }
                A[i][i] += lambda * (JTJ[i][i] > 1e-6 ? JTJ[i][i] : 1.0);
            }

            if (!solve6x6(A, JTe, delta)) {
                lambda *= 10.0;
                continue;
            }

            for (int i = 0; i < 6; ++i) {
                beta_new[i] = beta[i] + delta[i];
            }

            double residuals_new[10];
            double sse_new = compute_residuals(model_points, image_points, beta_new, fx, fy, cx, cy, residuals_new);

            if (sse_new < sse) {
                // Step accepted
                sse = sse_new;
                std::copy(residuals_new, residuals_new + 10, residuals);
                std::copy(beta_new, beta_new + 6, beta);
                lambda = std::max(1e-7, lambda / 10.0);
                solved = true;
                has_converged = true;
            } else {
                // Step rejected, increase damping
                lambda *= 10.0;
            }
        }

        // Convergence criteria
        double delta_norm = 0.0;
        for (int i = 0; i < 6; ++i) {
            delta_norm += delta[i] * delta[i];
        }
        delta_norm = std::sqrt(delta_norm);

        if (!solved || delta_norm < 1e-8) {
            break;
        }
    }

    if (has_converged && !std::isnan(beta[0]) && !std::isnan(beta[3]) && !std::isinf(beta[0]) && !std::isinf(beta[3])) {
        rvec = GazeVector3(beta[0], beta[1], beta[2]);
        tvec = GazeVector3(beta[3], beta[4], beta[5]);
        return true;
    }
    return false;
}

} // namespace Gaze
