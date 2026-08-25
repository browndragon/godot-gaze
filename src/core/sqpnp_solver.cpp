/**
 * @file sqpnp_solver.cpp
 * @brief Self-Contained Globally Optimal Sequential Quadratic Programming PnP (SQPnP) Solver
 *
 * Implements the SQPnP algorithm from:
 * "A Consistently Fast and Globally Optimal Solution to the Perspective-n-Point Problem"
 * by G. Terzakis and M. Lourakis (ECCV 2020).
 *
 * Licensed under the BSD 3-Clause License (Copyright 2020 George Terzakis).
 */
#include "sqpnp_solver.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <array>

namespace Gaze
{

namespace
{

constexpr double RANK_TOLERANCE = 1e-7;
constexpr double SQP_SQUARED_TOLERANCE = 1e-10;
constexpr double SQP_DET_THRESHOLD = 1.001;
constexpr double ORTHOGONALITY_SQUARED_ERROR_THRESHOLD = 1e-8;
constexpr double POINT_VARIANCE_THRESHOLD = 1e-5;
constexpr double SQRT3 = 1.73205080756887729352744634150587;
constexpr int SQP_MAX_ITERATION = 15;

using Vec9 = std::array<double, 9>;
using Mat9x9 = std::array<std::array<double, 9>, 9>;
using Mat3x3 = std::array<std::array<double, 3>, 3>;
using Mat3x9 = std::array<std::array<double, 9>, 3>;
using Mat9x6 = std::array<std::array<double, 6>, 9>;
using Mat9x3 = std::array<std::array<double, 3>, 9>;
using Mat6x6 = std::array<std::array<double, 6>, 6>;
using Vec6 = std::array<double, 6>;
using Vec3 = std::array<double, 3>;

inline double det3x3(const Vec9 &r)
{
    return (r[0] * r[4] * r[8] - r[0] * r[5] * r[7] - r[1] * r[3] * r[8]) +
           (r[2] * r[3] * r[7] + r[1] * r[6] * r[5] - r[2] * r[6] * r[4]);
}

inline double dot9(const Vec9 &a, const Vec9 &b)
{
    double s = 0.0;
    for (int i = 0; i < 9; ++i) s += a[i] * b[i];
    return s;
}

inline double norm9(const Vec9 &a)
{
    return std::sqrt(dot9(a, a));
}

inline double orthogonalityError(const Vec9 &e)
{
    double sq_norm_e1 = e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    double sq_norm_e2 = e[3] * e[3] + e[4] * e[4] + e[5] * e[5];
    double sq_norm_e3 = e[6] * e[6] + e[7] * e[7] + e[8] * e[8];
    double dot_e1e2 = e[0] * e[3] + e[1] * e[4] + e[2] * e[5];
    double dot_e1e3 = e[0] * e[6] + e[1] * e[7] + e[2] * e[8];
    double dot_e2e3 = e[3] * e[6] + e[4] * e[7] + e[5] * e[8];

    double err = (sq_norm_e1 - 1.0) * (sq_norm_e1 - 1.0) +
                 (sq_norm_e2 - 1.0) * (sq_norm_e2 - 1.0) +
                 (sq_norm_e3 - 1.0) * (sq_norm_e3 - 1.0) +
                 2.0 * (dot_e1e2 * dot_e1e2 + dot_e1e3 * dot_e1e3 + dot_e2e3 * dot_e2e3);
    return err;
}

// Analytic inverse of 3x3 symmetric matrix
bool invertSPD3x3(const Mat3x3 &A, Mat3x3 &A1)
{
    double L[9] = {0}, D[3] = {0}, v[2] = {0}, x[3] = {0};

    v[0] = D[0] = A[0][0];
    if (v[0] <= 1e-10) return false;
    v[1] = 1.0 / v[0];
    L[3] = A[1][0] * v[1];
    L[6] = A[2][0] * v[1];

    v[0] = L[3] * D[0];
    v[1] = D[1] = A[1][1] - L[3] * v[0];
    if (v[1] <= 1e-10) return false;
    L[7] = (A[2][1] - L[6] * v[0]) / v[1];

    v[0] = L[6] * D[0];
    v[1] = L[7] * D[1];
    D[2] = A[2][2] - L[6] * v[0] - L[7] * v[1];
    if (D[2] <= 1e-10) return false;

    D[0] = 1.0 / D[0];
    D[1] = 1.0 / D[1];
    D[2] = 1.0 / D[2];

    x[1] = -L[3];
    x[2] = -L[6] + L[7] * L[3];
    A1[0][2] = x[2] = x[2] * D[2];
    A1[0][1] = x[1] = x[1] * D[1] - L[7] * x[2];
    A1[0][0] = D[0] - L[3] * x[1] - L[6] * x[2];

    x[2] = -L[7];
    A1[1][2] = x[2] = x[2] * D[2];
    A1[1][1] = x[1] = D[1] - L[7] * x[2];
    A1[1][0] = -L[3] * x[1] - L[6] * x[2];

    A1[2][2] = x[2] = D[2];
    A1[2][1] = x[1] = -L[7] * x[2];
    A1[2][0] = -L[3] * x[1] - L[6] * x[2];

    return true;
}

bool analyticalInverse3x3Symm(const Mat3x3 &Q, Mat3x3 &Qinv)
{
    double a = Q[0][0], b = Q[1][0], d = Q[1][1], c = Q[2][0], e = Q[2][1], f = Q[2][2];
    double t2 = e * e, t4 = a * d, t7 = b * b, t9 = b * c, t12 = c * c;
    double det = -t4 * f + a * t2 + t7 * f - 2.0 * t9 * e + t12 * d;

    if (std::abs(det) < 1e-12) return false;

    double t15 = 1.0 / det;
    double t20 = (-b * f + c * e) * t15;
    double t24 = (b * e - c * d) * t15;
    double t30 = (a * e - t9) * t15;

    Qinv[0][0] = (-d * f + t2) * t15;
    Qinv[0][1] = Qinv[1][0] = -t20;
    Qinv[0][2] = Qinv[2][0] = -t24;
    Qinv[1][1] = -(a * f - t12) * t15;
    Qinv[1][2] = Qinv[2][1] = t30;
    Qinv[2][2] = -(t4 - t7) * t15;
    return true;
}

// 3D SVD for 3x3 matrix using Jacobi rotations to compute nearest rotation in SO(3)
void nearestRotationMatrixSVD(const Vec9 &e, Vec9 &r)
{
    // Copy e into 3x3
    double A[3][3] = {
        {e[0], e[1], e[2]},
        {e[3], e[4], e[5]},
        {e[6], e[7], e[8]}
    };
    // Compute A^T * A
    double ATA[3][3] = {0};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                ATA[i][j] += A[k][i] * A[k][j];

    // Eigenvalues and eigenvectors of ATA via Jacobi
    double V[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    double d[3] = {ATA[0][0], ATA[1][1], ATA[2][2]};

    for (int iter = 0; iter < 50; ++iter)
    {
        double sm = std::abs(ATA[0][1]) + std::abs(ATA[0][2]) + std::abs(ATA[1][2]);
        if (sm < 1e-15) break;

        for (int p = 0; p < 2; ++p)
        {
            for (int q = p + 1; q < 3; ++q)
            {
                double app = ATA[p][p], aqq = ATA[q][q], apq = ATA[p][q];
                if (std::abs(apq) < 1e-15) continue;

                double theta = 0.5 * (aqq - app) / apq;
                double t = 1.0 / (std::abs(theta) + std::sqrt(1.0 + theta * theta));
                if (theta < 0.0) t = -t;
                double c = 1.0 / std::sqrt(1.0 + t * t);
                double s = t * c;
                double tau = s / (1.0 + c);

                ATA[p][p] -= t * apq;
                ATA[q][q] += t * apq;
                ATA[p][q] = 0.0;

                for (int j = 0; j < 3; ++j)
                {
                    if (j != p && j != q)
                    {
                        double ajp = ATA[std::min(j, p)][std::max(j, p)];
                        double ajq = ATA[std::min(j, q)][std::max(j, q)];
                        ATA[std::min(j, p)][std::max(j, p)] = ajp - s * (ajq + tau * ajp);
                        ATA[std::min(j, q)][std::max(j, q)] = ajq + s * (ajp - tau * ajq);
                    }
                    double vjp = V[j][p], vjq = V[j][q];
                    V[j][p] = vjp - s * (vjq + tau * vjp);
                    V[j][q] = vjq + s * (vjp - tau * vjq);
                }
            }
        }
    }

    // Singular values
    double sigma[3];
    for (int i = 0; i < 3; ++i) sigma[i] = std::sqrt(std::max(0.0, ATA[i][i]));

    // U = A * V * diag(1/sigma)
    double U[3][3] = {0};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) sum += A[i][k] * V[k][j];
            U[i][j] = (sigma[j] > 1e-10) ? sum / sigma[j] : 0.0;
        }
    }

    // Ensure orthonormal U
    double detU = U[0][0]*(U[1][1]*U[2][2]-U[1][2]*U[2][1]) - U[0][1]*(U[1][0]*U[2][2]-U[1][2]*U[2][0]) + U[0][2]*(U[1][0]*U[2][1]-U[1][1]*U[2][0]);
    double detV = V[0][0]*(V[1][1]*V[2][2]-V[1][2]*V[2][1]) - V[0][1]*(V[1][0]*V[2][2]-V[1][2]*V[2][0]) + V[0][2]*(V[1][0]*V[2][1]-V[1][1]*V[2][0]);
    double detUV = detU * detV;

    // R = U * diag(1, 1, detUV) * V^T
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            r[i * 3 + j] = U[i][0] * V[j][0] + U[i][1] * V[j][1] + (U[i][2] * detUV) * V[j][2];
        }
    }
}

// Fast Optimal Attitude Matrix (FOAM) nearest rotation matrix
void nearestRotationMatrixFOAM(const Vec9 &e, Vec9 &r)
{
    double det_e = (e[0] * e[4] * e[8] - e[0] * e[5] * e[7] - e[1] * e[3] * e[8]) +
                   (e[2] * e[3] * e[7] + e[1] * e[6] * e[5] - e[2] * e[6] * e[4]);
    if (std::abs(det_e) < 1e-4)
    {
        nearestRotationMatrixSVD(e, r);
        return;
    }

    double adj_e[9];
    adj_e[0] = e[4] * e[8] - e[5] * e[7]; adj_e[1] = e[2] * e[7] - e[1] * e[8]; adj_e[2] = e[1] * e[5] - e[2] * e[4];
    adj_e[3] = e[5] * e[6] - e[3] * e[8]; adj_e[4] = e[0] * e[8] - e[2] * e[6]; adj_e[5] = e[2] * e[3] - e[0] * e[5];
    adj_e[6] = e[3] * e[7] - e[4] * e[6]; adj_e[7] = e[1] * e[6] - e[0] * e[7]; adj_e[8] = e[0] * e[4] - e[1] * e[3];

    double e_sq = dot9(e, e);
    double adj_e_sq = dot9(*(const Vec9*)adj_e, *(const Vec9*)adj_e);

    double l = 0.5 * (e_sq + 3.0);
    if (det_e < 0.0) l = -l;

    for (int i = 15; i > 0; --i)
    {
        double lprev = l;
        double tmp = (l * l - e_sq);
        double p = (tmp * tmp - 8.0 * l * det_e - 4.0 * adj_e_sq);
        double pp = 8.0 * (0.5 * tmp * l - det_e);
        if (std::abs(pp) < 1e-14) break;
        l -= p / pp;
        if (std::abs(l - lprev) <= 1e-12 * std::abs(lprev)) break;
    }

    double e_et[9];
    e_et[0] = e[0]*e[0] + e[1]*e[1] + e[2]*e[2];
    e_et[1] = e[0]*e[3] + e[1]*e[4] + e[2]*e[5];
    e_et[2] = e[0]*e[6] + e[1]*e[7] + e[2]*e[8];
    e_et[3] = e_et[1];
    e_et[4] = e[3]*e[3] + e[4]*e[4] + e[5]*e[5];
    e_et[5] = e[3]*e[6] + e[4]*e[7] + e[5]*e[8];
    e_et[6] = e_et[2];
    e_et[7] = e_et[5];
    e_et[8] = e[6]*e[6] + e[7]*e[7] + e[8]*e[8];

    double tmp[9];
    tmp[0] = e_et[0]*e[0] + e_et[1]*e[3] + e_et[2]*e[6];
    tmp[1] = e_et[0]*e[1] + e_et[1]*e[4] + e_et[2]*e[7];
    tmp[2] = e_et[0]*e[2] + e_et[1]*e[5] + e_et[2]*e[8];
    tmp[3] = e_et[3]*e[0] + e_et[4]*e[3] + e_et[5]*e[6];
    tmp[4] = e_et[3]*e[1] + e_et[4]*e[4] + e_et[5]*e[7];
    tmp[5] = e_et[3]*e[2] + e_et[4]*e[5] + e_et[5]*e[8];
    tmp[6] = e_et[6]*e[0] + e_et[7]*e[3] + e_et[8]*e[6];
    tmp[7] = e_et[6]*e[1] + e_et[7]*e[4] + e_et[8]*e[7];
    tmp[8] = e_et[6]*e[2] + e_et[7]*e[5] + e_et[8]*e[8];

    const double a = l * l + e_sq;
    double denom = 1.0 / (l * (l * l - e_sq) - 2.0 * det_e);

    r[0] = (a * e[0] + 2.0 * l * adj_e[0] - 2.0 * tmp[0]) * denom;
    r[1] = (a * e[1] + 2.0 * l * adj_e[3] - 2.0 * tmp[1]) * denom;
    r[2] = (a * e[2] + 2.0 * l * adj_e[6] - 2.0 * tmp[2]) * denom;
    r[3] = (a * e[3] + 2.0 * l * adj_e[1] - 2.0 * tmp[3]) * denom;
    r[4] = (a * e[4] + 2.0 * l * adj_e[4] - 2.0 * tmp[4]) * denom;
    r[5] = (a * e[5] + 2.0 * l * adj_e[7] - 2.0 * tmp[5]) * denom;
    r[6] = (a * e[6] + 2.0 * l * adj_e[2] - 2.0 * tmp[6]) * denom;
    r[7] = (a * e[7] + 2.0 * l * adj_e[5] - 2.0 * tmp[7]) * denom;
    r[8] = (a * e[8] + 2.0 * l * adj_e[8] - 2.0 * tmp[8]) * denom;
}

// Unconditionally stable Jacobi Eigenvalue decomposition for 9x9 real symmetric matrices
void jacobiSymmetric9x9(const Mat9x9 &A_in, Vec9 &eigenvalues, Mat9x9 &V)
{
    Mat9x9 A = A_in;
    for (int i = 0; i < 9; ++i)
    {
        for (int j = 0; j < 9; ++j) V[i][j] = (i == j) ? 1.0 : 0.0;
        eigenvalues[i] = A[i][i];
    }

    for (int iter = 0; iter < 50; ++iter)
    {
        double sm = 0.0;
        for (int i = 0; i < 8; ++i)
            for (int j = i + 1; j < 9; ++j)
                sm += std::abs(A[i][j]);

        if (sm < 1e-15) break;

        double thresh = (iter < 3) ? 0.2 * sm / 81.0 : 0.0;

        for (int p = 0; p < 8; ++p)
        {
            for (int q = p + 1; q < 9; ++q)
            {
                double g = 100.0 * std::abs(A[p][q]);
                if (iter > 4 && std::abs(eigenvalues[p]) + g == std::abs(eigenvalues[p])
                             && std::abs(eigenvalues[q]) + g == std::abs(eigenvalues[q]))
                {
                    A[p][q] = 0.0;
                }
                else if (std::abs(A[p][q]) > thresh)
                {
                    double h = eigenvalues[q] - eigenvalues[p];
                    double t;
                    if (std::abs(h) + g == std::abs(h))
                    {
                        t = A[p][q] / h;
                    }
                    else
                    {
                        double theta = 0.5 * h / A[p][q];
                        t = 1.0 / (std::abs(theta) + std::sqrt(1.0 + theta * theta));
                        if (theta < 0.0) t = -t;
                    }
                    double c = 1.0 / std::sqrt(1.0 + t * t);
                    double s = t * c;
                    double tau = s / (1.0 + c);
                    h = t * A[p][q];
                    eigenvalues[p] -= h;
                    eigenvalues[q] += h;
                    A[p][p] = eigenvalues[p];
                    A[q][q] = eigenvalues[q];
                    A[p][q] = 0.0;

                    for (int j = 0; j < p; ++j)
                    {
                        double g1 = A[j][p], h1 = A[j][q];
                        A[j][p] = g1 - s * (h1 + g1 * tau);
                        A[j][q] = h1 + s * (g1 - h1 * tau);
                    }
                    for (int j = p + 1; j < q; ++j)
                    {
                        double g1 = A[p][j], h1 = A[j][q];
                        A[p][j] = g1 - s * (h1 + g1 * tau);
                        A[j][q] = h1 + s * (g1 - h1 * tau);
                    }
                    for (int j = q + 1; j < 9; ++j)
                    {
                        double g1 = A[p][j], h1 = A[q][j];
                        A[p][j] = g1 - s * (h1 + g1 * tau);
                        A[q][j] = h1 + s * (g1 - h1 * tau);
                    }
                    for (int j = 0; j < 9; ++j)
                    {
                        double g1 = V[j][p], h1 = V[j][q];
                        V[j][p] = g1 - s * (h1 + g1 * tau);
                        V[j][q] = h1 + s * (g1 - h1 * tau);
                    }
                }
            }
        }
    }

    // Sort eigenvalues and corresponding eigenvector columns in descending order
    for (int i = 0; i < 8; ++i)
    {
        int max_idx = i;
        for (int j = i + 1; j < 9; ++j)
        {
            if (eigenvalues[j] > eigenvalues[max_idx]) max_idx = j;
        }
        if (max_idx != i)
        {
            std::swap(eigenvalues[i], eigenvalues[max_idx]);
            for (int k = 0; k < 9; ++k) std::swap(V[k][i], V[k][max_idx]);
        }
    }
}

void computeRowAndNullspace(
    const Vec9 &r, Mat9x6 &H, Mat9x3 &N, Mat6x6 &K, double norm_threshold = 0.1)
{
    H = {};

    double norm_r1 = std::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
    double inv_norm_r1 = norm_r1 > 1e-5 ? 1.0 / norm_r1 : 0.0;
    H[0][0] = r[0] * inv_norm_r1; H[1][0] = r[1] * inv_norm_r1; H[2][0] = r[2] * inv_norm_r1;
    K[0][0] = 2.0 * norm_r1;

    double norm_r2 = std::sqrt(r[3]*r[3] + r[4]*r[4] + r[5]*r[5]);
    double inv_norm_r2 = norm_r2 > 1e-5 ? 1.0 / norm_r2 : 0.0;
    H[3][1] = r[3] * inv_norm_r2; H[4][1] = r[4] * inv_norm_r2; H[5][1] = r[5] * inv_norm_r2;
    K[1][0] = 0.0; K[1][1] = 2.0 * norm_r2;

    double norm_r3 = std::sqrt(r[6]*r[6] + r[7]*r[7] + r[8]*r[8]);
    double inv_norm_r3 = norm_r3 > 1e-5 ? 1.0 / norm_r3 : 0.0;
    H[6][2] = r[6] * inv_norm_r3; H[7][2] = r[7] * inv_norm_r3; H[8][2] = r[8] * inv_norm_r3;
    K[2][0] = K[2][1] = 0.0; K[2][2] = 2.0 * norm_r3;

    double dot_j4q1 = r[3] * H[0][0] + r[4] * H[1][0] + r[5] * H[2][0];
    double dot_j4q2 = r[0] * H[3][1] + r[1] * H[4][1] + r[2] * H[5][1];
    H[0][3] = r[3] - dot_j4q1 * H[0][0];
    H[1][3] = r[4] - dot_j4q1 * H[1][0];
    H[2][3] = r[5] - dot_j4q1 * H[2][0];
    H[3][3] = r[0] - dot_j4q2 * H[3][1];
    H[4][3] = r[1] - dot_j4q2 * H[4][1];
    H[5][3] = r[2] - dot_j4q2 * H[5][1];
    double norm_j4 = std::sqrt(H[0][3]*H[0][3] + H[1][3]*H[1][3] + H[2][3]*H[2][3] +
                               H[3][3]*H[3][3] + H[4][3]*H[4][3] + H[5][3]*H[5][3]);
    double inv_norm_j4 = norm_j4 > 1e-5 ? 1.0 / norm_j4 : 0.0;
    for (int i = 0; i < 6; ++i) H[i][3] *= inv_norm_j4;

    K[3][0] = dot_j4q1; K[3][1] = dot_j4q2; K[3][2] = 0.0;
    K[3][3] = (r[3]*H[0][3] + r[4]*H[1][3] + r[5]*H[2][3]) + (r[0]*H[3][3] + r[1]*H[4][3] + r[2]*H[5][3]);

    double dot_j5q2 = r[6] * H[3][1] + r[7] * H[4][1] + r[8] * H[5][1];
    double dot_j5q3 = r[3] * H[6][2] + r[4] * H[7][2] + r[5] * H[8][2];
    double dot_j5q4 = r[6] * H[0][3] + r[7] * H[1][3] + r[8] * H[2][3];

    H[0][4] = -dot_j5q4 * H[0][3];
    H[1][4] = -dot_j5q4 * H[1][3];
    H[2][4] = -dot_j5q4 * H[2][3];
    H[3][4] = r[6] - dot_j5q2 * H[3][1] - dot_j5q4 * H[3][3];
    H[4][4] = r[7] - dot_j5q2 * H[4][1] - dot_j5q4 * H[4][3];
    H[5][4] = r[8] - dot_j5q2 * H[5][1] - dot_j5q4 * H[5][3];
    H[6][4] = r[3] - dot_j5q3 * H[6][2];
    H[7][4] = r[4] - dot_j5q3 * H[7][2];
    H[8][4] = r[5] - dot_j5q3 * H[8][2];

    double norm_q4 = 0.0;
    for (int i = 0; i < 9; ++i) norm_q4 += H[i][4] * H[i][4];
    norm_q4 = std::sqrt(norm_q4);
    double inv_norm_q4 = norm_q4 > 1e-5 ? 1.0 / norm_q4 : 0.0;
    for (int i = 0; i < 9; ++i) H[i][4] *= inv_norm_q4;

    K[4][0] = 0.0; K[4][1] = dot_j5q2; K[4][2] = dot_j5q3; K[4][3] = dot_j5q4;
    K[4][4] = (r[6]*H[3][4] + r[7]*H[4][4] + r[8]*H[5][4]) + (r[3]*H[6][4] + r[4]*H[7][4] + r[5]*H[8][4]);

    double dot_j6q1 = r[6] * H[0][0] + r[7] * H[1][0] + r[8] * H[2][0];
    double dot_j6q3 = r[0] * H[6][2] + r[1] * H[7][2] + r[2] * H[8][2];
    double dot_j6q4 = r[6] * H[0][3] + r[7] * H[1][3] + r[8] * H[2][3];
    double dot_j6q5 = (r[0]*H[6][4] + r[1]*H[7][4] + r[2]*H[8][4]) + (r[6]*H[0][4] + r[7]*H[1][4] + r[8]*H[2][4]);

    H[0][5] = r[6] - dot_j6q1 * H[0][0] - dot_j6q4 * H[0][3] - dot_j6q5 * H[0][4];
    H[1][5] = r[7] - dot_j6q1 * H[1][0] - dot_j6q4 * H[1][3] - dot_j6q5 * H[1][4];
    H[2][5] = r[8] - dot_j6q1 * H[2][0] - dot_j6q4 * H[2][3] - dot_j6q5 * H[2][4];
    H[3][5] = -dot_j6q5 * H[3][4] - dot_j6q4 * H[3][3];
    H[4][5] = -dot_j6q5 * H[4][4] - dot_j6q4 * H[4][3];
    H[5][5] = -dot_j6q5 * H[5][4] - dot_j6q4 * H[5][3];
    H[6][5] = r[0] - dot_j6q3 * H[6][2] - dot_j6q5 * H[6][4];
    H[7][5] = r[1] - dot_j6q3 * H[7][2] - dot_j6q5 * H[7][4];
    H[8][5] = r[2] - dot_j6q3 * H[8][2] - dot_j6q5 * H[8][4];

    double norm_q5 = 0.0;
    for (int i = 0; i < 9; ++i) norm_q5 += H[i][5] * H[i][5];
    norm_q5 = std::sqrt(norm_q5);
    double inv_norm_q5 = norm_q5 > 1e-5 ? 1.0 / norm_q5 : 0.0;
    for (int i = 0; i < 9; ++i) H[i][5] *= inv_norm_q5;

    K[5][0] = dot_j6q1; K[5][1] = 0.0; K[5][2] = dot_j6q3; K[5][3] = dot_j6q4; K[5][4] = dot_j6q5;
    K[5][5] = (r[6]*H[0][5] + r[7]*H[1][5] + r[8]*H[2][5]) + (r[0]*H[6][5] + r[1]*H[7][5] + r[2]*H[8][5]);

    // Null space projector Pn = I - H * H^T
    Mat9x9 Pn = {};
    for (int i = 0; i < 9; ++i)
    {
        Pn[i][i] = 1.0;
        for (int j = 0; j < 9; ++j)
        {
            double hht = 0.0;
            for (int k = 0; k < 6; ++k) hht += H[i][k] * H[j][k];
            Pn[i][j] -= hht;
        }
    }

    // Pick 3 most orthogonal columns of Pn with norm >= norm_threshold
    double col_norms[9];
    int idx1 = 0, idx2 = 0, idx3 = 0;
    double max_norm1 = -1.0;

    for (int i = 0; i < 9; ++i)
    {
        double s = 0.0;
        for (int k = 0; k < 9; ++k) s += Pn[k][i] * Pn[k][i];
        col_norms[i] = std::sqrt(s);
        if (col_norms[i] >= norm_threshold && col_norms[i] > max_norm1)
        {
            max_norm1 = col_norms[i];
            idx1 = i;
        }
    }

    double inv_max1 = max_norm1 > 1e-5 ? 1.0 / max_norm1 : 0.0;
    for (int i = 0; i < 9; ++i) N[i][0] = Pn[i][idx1] * inv_max1;
    col_norms[idx1] = -1.0;

    double min_dot12 = 1e9;
    for (int i = 0; i < 9; ++i)
    {
        if (col_norms[i] >= norm_threshold)
        {
            double dot = 0.0;
            for (int k = 0; k < 9; ++k) dot += Pn[k][i] * N[k][0];
            double cos_val = std::abs(dot / col_norms[i]);
            if (cos_val <= min_dot12)
            {
                min_dot12 = cos_val;
                idx2 = i;
            }
        }
    }

    Vec9 u2;
    double dot_v2n0 = 0.0;
    for (int i = 0; i < 9; ++i) dot_v2n0 += Pn[i][idx2] * N[i][0];
    double norm_u2 = 0.0;
    for (int i = 0; i < 9; ++i)
    {
        u2[i] = Pn[i][idx2] - dot_v2n0 * N[i][0];
        norm_u2 += u2[i] * u2[i];
    }
    norm_u2 = std::sqrt(norm_u2);
    double inv_u2 = norm_u2 > 1e-5 ? 1.0 / norm_u2 : 0.0;
    for (int i = 0; i < 9; ++i) N[i][1] = u2[i] * inv_u2;
    col_norms[idx2] = -1.0;

    double min_dot1323 = 1e9;
    for (int i = 0; i < 9; ++i)
    {
        if (col_norms[i] >= norm_threshold)
        {
            double dot1 = 0.0, dot2 = 0.0;
            for (int k = 0; k < 9; ++k)
            {
                dot1 += Pn[k][i] * N[k][0];
                dot2 += Pn[k][i] * N[k][1];
            }
            double cos1 = std::abs(dot1 / col_norms[i]);
            double cos2 = std::abs(dot2 / col_norms[i]);
            if (cos1 + cos2 <= min_dot1323)
            {
                min_dot1323 = cos1 + cos2;
                idx3 = i;
            }
        }
    }

    Vec9 u3;
    double dot_v3n0 = 0.0, dot_v3n1 = 0.0;
    for (int i = 0; i < 9; ++i)
    {
        dot_v3n0 += Pn[i][idx3] * N[i][0];
        dot_v3n1 += Pn[i][idx3] * N[i][1];
    }
    double norm_u3 = 0.0;
    for (int i = 0; i < 9; ++i)
    {
        u3[i] = Pn[i][idx3] - dot_v3n0 * N[i][0] - dot_v3n1 * N[i][1];
        norm_u3 += u3[i] * u3[i];
    }
    norm_u3 = std::sqrt(norm_u3);
    double inv_u3 = norm_u3 > 1e-5 ? 1.0 / norm_u3 : 0.0;
    for (int i = 0; i < 9; ++i) N[i][2] = u3[i] * inv_u3;
}

void solveSQPSystem(const Mat9x9 &omega, const Vec9 &r, Vec9 &delta)
{
    double sqnorm_r1 = r[0]*r[0] + r[1]*r[1] + r[2]*r[2];
    double sqnorm_r2 = r[3]*r[3] + r[4]*r[4] + r[5]*r[5];
    double sqnorm_r3 = r[6]*r[6] + r[7]*r[7] + r[8]*r[8];
    double dot_r1r2 = r[0]*r[3] + r[1]*r[4] + r[2]*r[5];
    double dot_r1r3 = r[0]*r[6] + r[1]*r[7] + r[2]*r[8];
    double dot_r2r3 = r[3]*r[6] + r[4]*r[7] + r[5]*r[8];

    Mat9x3 N;
    Mat9x6 H;
    Mat6x6 JH;

    computeRowAndNullspace(r, H, N, JH);

    Vec6 g;
    g[0] = 1.0 - sqnorm_r1; g[1] = 1.0 - sqnorm_r2; g[2] = 1.0 - sqnorm_r3;
    g[3] = -dot_r1r2;       g[4] = -dot_r2r3;       g[5] = -dot_r1r3;

    Vec6 x;
    x[0] = (std::abs(JH[0][0]) > 1e-8) ? g[0] / JH[0][0] : 0.0;
    x[1] = (std::abs(JH[1][1]) > 1e-8) ? g[1] / JH[1][1] : 0.0;
    x[2] = (std::abs(JH[2][2]) > 1e-8) ? g[2] / JH[2][2] : 0.0;
    x[3] = (std::abs(JH[3][3]) > 1e-8) ? (g[3] - JH[3][0]*x[0] - JH[3][1]*x[1]) / JH[3][3] : 0.0;
    x[4] = (std::abs(JH[4][4]) > 1e-8) ? (g[4] - JH[4][1]*x[1] - JH[4][2]*x[2] - JH[4][3]*x[3]) / JH[4][4] : 0.0;
    x[5] = (std::abs(JH[5][5]) > 1e-8) ? (g[5] - JH[5][0]*x[0] - JH[5][2]*x[2] - JH[5][3]*x[3] - JH[5][4]*x[4]) / JH[5][5] : 0.0;

    for (int i = 0; i < 9; ++i)
    {
        delta[i] = 0.0;
        for (int k = 0; k < 6; ++k) delta[i] += H[i][k] * x[k];
    }

    Mat3x9 nt_omega = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 9; ++j)
            for (int k = 0; k < 9; ++k)
                nt_omega[i][j] += N[k][i] * omega[k][j];

    Mat3x3 W = {}, W_inv = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 9; ++k)
                W[i][j] += nt_omega[i][k] * N[k][j];

    if (!analyticalInverse3x3Symm(W, W_inv))
    {
        invertSPD3x3(W, W_inv);
    }

    Vec9 delta_plus_r;
    for (int i = 0; i < 9; ++i) delta_plus_r[i] = delta[i] + r[i];

    Vec3 nt_omega_dr = {};
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 9; ++k)
            nt_omega_dr[i] += nt_omega[i][k] * delta_plus_r[k];

    Vec3 y = {};
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 3; ++k)
            y[i] -= W_inv[i][k] * nt_omega_dr[k];

    for (int i = 0; i < 9; ++i)
        for (int k = 0; k < 3; ++k)
            delta[i] += N[i][k] * y[k];
}

Vec9 runSQP(const Mat9x9 &omega, const Vec9 &r0)
{
    Vec9 r = r0;
    for (int step = 0; step < SQP_MAX_ITERATION; ++step)
    {
        Vec9 delta;
        solveSQPSystem(omega, r, delta);
        for (int i = 0; i < 9; ++i) r[i] += delta[i];
        if (dot9(delta, delta) <= SQP_SQUARED_TOLERANCE) break;
    }

    double det_r = det3x3(r);
    if (det_r < 0.0)
    {
        for (int i = 0; i < 9; ++i) r[i] = -r[i];
        det_r = -det_r;
    }

    Vec9 r_hat;
    if (det_r > SQP_DET_THRESHOLD)
    {
        nearestRotationMatrixFOAM(r, r_hat);
    }
    else
    {
        r_hat = r;
    }
    return r_hat;
}

double computeReprojectionError(
    const std::vector<GazeVector3> &object_points,
    const std::vector<GazeVector2> &image_points,
    const GazeBasis3D &R, const GazeVector3 &t,
    double fx, double fy, double cx, double cy)
{
    double sse = 0.0;
    for (size_t i = 0; i < object_points.size(); ++i)
    {
        GazeVector3 p_cam = R.multiply_vector(object_points[i]) + t;
        if (p_cam.z <= 0.001) return 1e9;
        double u_proj = fx * (p_cam.x / p_cam.z) + cx;
        double v_proj = fy * (p_cam.y / p_cam.z) + cy;
        double du = image_points[i].x - u_proj;
        double dv = image_points[i].y - v_proj;
        sse += du * du + dv * dv;
    }
    return sse / object_points.size();
}

} // namespace

bool SQPnPSolver::solve(
    const std::vector<GazeVector3> &object_points,
    const std::vector<GazeVector2> &image_points,
    double fx, double fy, double cx, double cy,
    GazeBasis3D &out_rotation,
    GazeVector3 &out_translation)
{
    size_t n = object_points.size();
    if (n < 4 || image_points.size() != n) return false;

    // Convert 2D image points to normalized camera rays (x, y)
    std::vector<GazeVector2> norm_img(n);
    GazeVector2 sum_img(0.0, 0.0);
    GazeVector3 sum_obj(0.0, 0.0, 0.0);
    double sq_norm_sum = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        norm_img[i].x = (image_points[i].x - cx) / fx;
        norm_img[i].y = (image_points[i].y - cy) / fy;
        sum_img.x += norm_img[i].x;
        sum_img.y += norm_img[i].y;
        sum_obj.x += object_points[i].x;
        sum_obj.y += object_points[i].y;
        sum_obj.z += object_points[i].z;
        sq_norm_sum += norm_img[i].x * norm_img[i].x + norm_img[i].y * norm_img[i].y;
    }

    Mat9x9 omega = {};
    Mat3x9 qa_sum = {};

    for (size_t i = 0; i < n; ++i)
    {
        double x = norm_img[i].x, y = norm_img[i].y;
        double X = object_points[i].x, Y = object_points[i].y, Z = object_points[i].z;
        double sq_norm = x * x + y * y;

        double X2 = X * X, XY = X * Y, XZ = X * Z;
        double Y2 = Y * Y, YZ = Y * Z, Z2 = Z * Z;

        omega[0][0] += X2; omega[0][1] += XY; omega[0][2] += XZ;
        omega[1][1] += Y2; omega[1][2] += YZ; omega[2][2] += Z2;

        omega[0][6] -= x * X2; omega[0][7] -= x * XY; omega[0][8] -= x * XZ;
        omega[1][7] -= x * Y2; omega[1][8] -= x * YZ;
        omega[2][8] -= x * Z2;

        omega[3][6] -= y * X2; omega[3][7] -= y * XY; omega[3][8] -= y * XZ;
        omega[4][7] -= y * Y2; omega[4][8] -= y * YZ;
        omega[5][8] -= y * Z2;

        omega[6][6] += sq_norm * X2; omega[6][7] += sq_norm * XY; omega[6][8] += sq_norm * XZ;
        omega[7][7] += sq_norm * Y2; omega[7][8] += sq_norm * YZ;
        omega[8][8] += sq_norm * Z2;

        qa_sum[0][0] += X; qa_sum[0][1] += Y; qa_sum[0][2] += Z;
        qa_sum[0][6] -= x * X; qa_sum[0][7] -= x * Y; qa_sum[0][8] -= x * Z;
        qa_sum[1][6] -= y * X; qa_sum[1][7] -= y * Y; qa_sum[1][8] -= y * Z;
        qa_sum[2][6] += sq_norm * X; qa_sum[2][7] += sq_norm * Y; qa_sum[2][8] += sq_norm * Z;
    }

    qa_sum[1][3] = qa_sum[0][0]; qa_sum[1][4] = qa_sum[0][1]; qa_sum[1][5] = qa_sum[0][2];
    qa_sum[2][0] = qa_sum[0][6]; qa_sum[2][1] = qa_sum[0][7]; qa_sum[2][2] = qa_sum[0][8];
    qa_sum[2][3] = qa_sum[1][6]; qa_sum[2][4] = qa_sum[1][7]; qa_sum[2][5] = qa_sum[1][8];

    omega[1][6] = omega[0][7]; omega[2][6] = omega[0][8]; omega[2][7] = omega[1][8];
    omega[4][6] = omega[3][7]; omega[5][6] = omega[3][8]; omega[5][7] = omega[4][8];
    omega[7][6] = omega[6][7]; omega[8][6] = omega[6][8]; omega[8][7] = omega[7][8];

    omega[3][3] = omega[0][0]; omega[3][4] = omega[0][1]; omega[3][5] = omega[0][2];
    omega[4][4] = omega[1][1]; omega[4][5] = omega[1][2]; omega[5][5] = omega[2][2];

    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < i; ++j)
            omega[i][j] = omega[j][i];

    Mat3x3 q = {};
    q[0][0] = n; q[0][1] = 0; q[0][2] = -sum_img.x;
    q[1][0] = 0; q[1][1] = n; q[1][2] = -sum_img.y;
    q[2][0] = -sum_img.x; q[2][1] = -sum_img.y; q[2][2] = sq_norm_sum;

    Mat3x3 q_inv = {};
    if (!invertSPD3x3(q, q_inv))
    {
        if (!analyticalInverse3x3Symm(q, q_inv)) return false;
    }

    Mat3x9 p_mat = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 9; ++j)
            for (int k = 0; k < 3; ++k)
                p_mat[i][j] -= q_inv[i][k] * qa_sum[k][j];

    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            for (int k = 0; k < 3; ++k)
                omega[i][j] += qa_sum[k][i] * p_mat[k][j];

    // Compute eigenvalues and eigenvectors of Omega
    Vec9 s_eig;
    Mat9x9 u_mat;
    jacobiSymmetric9x9(omega, s_eig, u_mat);

    if (s_eig[0] < 1e-7) return false;

    // Identify nullspace vectors (smallest eigenvalues)
    int num_null = 0;
    while (num_null < 6 && s_eig[8 - num_null] < RANK_TOLERANCE) num_null++;
    if (num_null == 0) num_null = 1;

    double min_reproj_err = std::numeric_limits<double>::max();
    GazeBasis3D best_R;
    GazeVector3 best_t;
    bool found_solution = false;

    auto test_candidate_r = [&](const Vec9 &r_hat) {
        GazeBasis3D R_cand(
            GazeVector3(r_hat[0], r_hat[3], r_hat[6]),
            GazeVector3(r_hat[1], r_hat[4], r_hat[7]),
            GazeVector3(r_hat[2], r_hat[5], r_hat[8])
        );
        // Translation t = P * r_hat
        GazeVector3 t_cand(0.0, 0.0, 0.0);
        for (int k = 0; k < 9; ++k)
        {
            t_cand.x += p_mat[0][k] * r_hat[k];
            t_cand.y += p_mat[1][k] * r_hat[k];
            t_cand.z += p_mat[2][k] * r_hat[k];
        }

        if (t_cand.z > 0.01)
        {
            double err = computeReprojectionError(object_points, image_points, R_cand, t_cand, fx, fy, cx, cy);
            if (err < min_reproj_err)
            {
                min_reproj_err = err;
                best_R = R_cand;
                best_t = t_cand;
                found_solution = true;
            }
        }
    };

    for (int i = 9 - num_null; i < 9; ++i)
    {
        Vec9 e;
        for (int k = 0; k < 9; ++k) e[k] = SQRT3 * u_mat[k][i];

        if (orthogonalityError(e) < ORTHOGONALITY_SQUARED_ERROR_THRESHOLD)
        {
            double d = det3x3(e);
            Vec9 r_hat;
            for (int k = 0; k < 9; ++k) r_hat[k] = d * e[k];
            test_candidate_r(r_hat);
        }
        else
        {
            Vec9 r1, r2;
            nearestRotationMatrixFOAM(e, r1);
            Vec9 opt_r1 = runSQP(omega, r1);
            test_candidate_r(opt_r1);

            Vec9 minus_e;
            for (int k = 0; k < 9; ++k) minus_e[k] = -e[k];
            nearestRotationMatrixFOAM(minus_e, r2);
            Vec9 opt_r2 = runSQP(omega, r2);
            test_candidate_r(opt_r2);
        }
    }

    if (!found_solution) return false;

    out_rotation = best_R;
    out_translation = best_t;
    return true;
}

bool SQPnPSolver::solve_rvec(
    const std::vector<GazeVector3> &object_points,
    const std::vector<GazeVector2> &image_points,
    double fx, double fy, double cx, double cy,
    GazeVector3 &out_rvec,
    GazeVector3 &out_translation)
{
    GazeBasis3D R;
    if (!solve(object_points, image_points, fx, fy, cx, cy, R, out_translation))
    {
        return false;
    }
    out_rvec = basis_to_rodrigues(R);
    return true;
}

} // namespace Gaze
