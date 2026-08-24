#include "pnp_solver.hpp"
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
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
    std::vector<double>& residuals
) {
    GazeVector3 r(beta[0], beta[1], beta[2]);
    GazeVector3 t(beta[3], beta[4], beta[5]);
    double sse = 0.0;
    size_t n = model_points.size();
    residuals.resize(2 * n);
    for (size_t i = 0; i < n; ++i) {
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
    std::vector<std::vector<double>>& J
) {
    const double eps = 1e-4;
    size_t n = model_points.size();
    J.assign(2 * n, std::vector<double>(6, 0.0));

    double perturbed_beta[6];
    for (int i = 0; i < 6; ++i) {
        perturbed_beta[i] = beta[i];
    }
    for (int j = 0; j < 6; ++j) {
        perturbed_beta[j] = beta[j] + eps;
        GazeVector3 r_pos(perturbed_beta[0], perturbed_beta[1], perturbed_beta[2]);
        GazeVector3 t_pos(perturbed_beta[3], perturbed_beta[4], perturbed_beta[5]);

        perturbed_beta[j] = beta[j] - eps;
        GazeVector3 r_neg(perturbed_beta[0], perturbed_beta[1], perturbed_beta[2]);
        GazeVector3 t_neg(perturbed_beta[3], perturbed_beta[4], perturbed_beta[5]);

        perturbed_beta[j] = beta[j];

        for (size_t i = 0; i < n; ++i) {
            GazeVector2 proj_pos = project_point(model_points[i], r_pos, t_pos, fx, fy, cx, cy);
            GazeVector2 proj_neg = project_point(model_points[i], r_neg, t_neg, fx, fy, cx, cy);
            
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

bool solve_pnp_dlt(
    const std::vector<GazeVector3>& model_points,
    const std::vector<GazeVector2>& image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3& rvec, GazeVector3& tvec
) {
    if (model_points.size() < 4 || image_points.size() != model_points.size()) {
        return false;
    }

    GazeVector2 r_mid_2d, l_mid_2d;
    GazeVector3 r_mid_3d, l_mid_3d;

    if (model_points.size() == 35) {
        // 35-point model: Right Eye Inner (0), Outer (1); Left Eye Inner (2), Outer (3)
        r_mid_2d = (image_points[0] + image_points[1]) * 0.5;
        l_mid_2d = (image_points[2] + image_points[3]) * 0.5;
        r_mid_3d = (model_points[0] + model_points[1]) * 0.5;
        l_mid_3d = (model_points[2] + model_points[3]) * 0.5;
    } else {
        // Find the index of the point with minimum X (right in camera space) and maximum X (left in camera space)
        size_t min_x_idx = 0;
        size_t max_x_idx = 0;
        for (size_t i = 1; i < model_points.size(); ++i) {
            if (model_points[i].x < model_points[min_x_idx].x) min_x_idx = i;
            if (model_points[i].x > model_points[max_x_idx].x) max_x_idx = i;
        }
        r_mid_2d = image_points[min_x_idx];
        l_mid_2d = image_points[max_x_idx];
        r_mid_3d = model_points[min_x_idx];
        l_mid_3d = model_points[max_x_idx];
    }

    double dx_2d = l_mid_2d.x - r_mid_2d.x;
    double dy_2d = l_mid_2d.y - r_mid_2d.y;
    double eye_dist_2d = std::hypot(dx_2d, dy_2d);
    double eye_dist_3d = std::hypot(l_mid_3d.x - r_mid_3d.x, l_mid_3d.y - r_mid_3d.y);

    if (eye_dist_2d < 1e-4 || eye_dist_3d < 1e-4) {
        return false;
    }

    double z_est = (eye_dist_3d * fx) / eye_dist_2d;
    double eye_mid_x_2d = (r_mid_2d.x + l_mid_2d.x) * 0.5;
    double eye_mid_y_2d = (r_mid_2d.y + l_mid_2d.y) * 0.5;
    double eye_mid_x_3d = (r_mid_3d.x + l_mid_3d.x) * 0.5;
    double eye_mid_y_3d = (r_mid_3d.y + l_mid_3d.y) * 0.5;

    double tx_est = ((eye_mid_x_2d - cx) * z_est / fx) - eye_mid_x_3d;
    double ty_est = ((eye_mid_y_2d - cy) * z_est / fy) - eye_mid_y_3d;
    double roll_angle = std::atan2(dy_2d, dx_2d);

    rvec = GazeVector3(0.0, 0.0, roll_angle);
    tvec = GazeVector3(tx_est, ty_est, z_est);
    return true;
}

bool solve_pnp_opencv(
    const std::vector<GazeVector3>& model_points,
    const std::vector<GazeVector2>& image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3& rvec, GazeVector3& tvec,
    PnPSolverMethod method,
    bool use_extrinsic_guess
) {
    if (model_points.size() < 4 || image_points.size() != model_points.size()) {
        return false;
    }

    std::vector<cv::Point3d> cv_obj_pts(model_points.size());
    for (size_t i = 0; i < model_points.size(); ++i) {
        cv_obj_pts[i] = cv::Point3d(model_points[i].x, model_points[i].y, model_points[i].z);
    }

    std::vector<cv::Point2d> cv_img_pts(image_points.size());
    for (size_t i = 0; i < image_points.size(); ++i) {
        cv_img_pts[i] = cv::Point2d(image_points[i].x, image_points[i].y);
    }

    cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) <<
        fx, 0.0, cx,
        0.0, fy, cy,
        0.0, 0.0, 1.0);
    cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, CV_64F);

    cv::Mat cv_rvec = cv::Mat::zeros(3, 1, CV_64F);
    cv::Mat cv_tvec = cv::Mat::zeros(3, 1, CV_64F);

    if (use_extrinsic_guess) {
        cv_rvec.at<double>(0) = rvec.x;
        cv_rvec.at<double>(1) = rvec.y;
        cv_rvec.at<double>(2) = rvec.z;
        cv_tvec.at<double>(0) = tvec.x;
        cv_tvec.at<double>(1) = tvec.y;
        cv_tvec.at<double>(2) = tvec.z;
    }

    int flag = cv::SOLVEPNP_SQPNP;
    if (method == PnPSolverMethod::ITERATIVE) {
        flag = cv::SOLVEPNP_ITERATIVE;
    } else if (method == PnPSolverMethod::EPNP) {
        flag = cv::SOLVEPNP_EPNP;
    }

    try {
        bool ok = cv::solvePnP(
            cv_obj_pts,
            cv_img_pts,
            camera_matrix,
            dist_coeffs,
            cv_rvec,
            cv_tvec,
            use_extrinsic_guess,
            flag
        );

        if (!ok) {
            return false;
        }

        rvec = GazeVector3(cv_rvec.at<double>(0), cv_rvec.at<double>(1), cv_rvec.at<double>(2));
        tvec = GazeVector3(cv_tvec.at<double>(0), cv_tvec.at<double>(1), cv_tvec.at<double>(2));
        return true;
    } catch (...) {
        return false;
    }
}

bool solve_pnp_lm(
    const std::vector<GazeVector3>& model_points,
    const std::vector<GazeVector2>& image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3& rvec, GazeVector3& tvec,
    bool use_extrinsic_guess
) {
    return solve_pnp_opencv(model_points, image_points, fx, fy, cx, cy, rvec, tvec, PnPSolverMethod::SQPNP, use_extrinsic_guess);
}

} // namespace Gaze
