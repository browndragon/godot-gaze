#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "../core/fill_accumulator.hpp"

namespace godot {

/**
 * @brief Second-order inertial dwell / charge accumulator node for gaze UI interactions.
 *
 * Smoothly charges `fill` in `[0.0, 1.0]` toward `1.0` when `target_error <= 0` (or `active_target` is true)
 * and drains toward `0.0` when outside the target, accelerating rate changes via `fill_acceleration` and
 * `drain_acceleration` and scaling drain acceleration with `(1 + error / error_doubling_distance)`.
 *
 * Configure `fill_sec` / `drain_sec` (or `fill_rate` / `drain_rate`) and choose automatic updates via
 * `process_callback` (`PROCESS_MANUAL`, `PROCESS_IDLE`, or `PROCESS_PHYSICS`), or call `process_error_delta()` /
 * `process_dwell_delta()` manually.
 *
 * Signals emitted: `fill_changed(fill: float)`, `fill_started()`, `filled()`, `emptied()`.
 */
class FillAccumulator : public Node {
    GDCLASS(FillAccumulator, Node);

public:
    /**
     * @brief Controls when the accumulator steps automatically.
     */
    enum ProcessCallback {
        PROCESS_MANUAL = 0,  ///< Only advances when process_error_delta() or process_dwell_delta() is called manually.
        PROCESS_IDLE = 1,    ///< Advances automatically during _process(delta) using active_target / target_error.
        PROCESS_PHYSICS = 2, ///< Advances automatically during _physics_process(delta) using active_target / target_error.
    };

private:
    Gaze::FillAccumulator impl;
    ProcessCallback process_callback = PROCESS_MANUAL;
    bool last_was_charging = false;
    bool last_is_filled = false;
    bool last_is_empty = true;
    double current_error = 0.0;
    bool is_active_target = false;

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    FillAccumulator();
    virtual ~FillAccumulator() = default;

    /** @brief Sets the normalized fill level clamped to [0.0, 1.0]. */
    void set_fill(double p_fill);
    /** @brief Returns the normalized fill level in [0.0, 1.0]. */
    double get_fill() const;

    /** @brief Returns the current fill rate-of-change velocity in fill units per second. */
    double get_velocity() const;

    /** @brief Returns true when fill has reached 1.0. */
    bool is_filled() const;
    /** @brief Returns true when fill is at 0.0. */
    bool is_empty() const;

    /** @brief Sets maximum charging rate in normalized fill units per second (default 1.6). */
    void set_fill_rate(double p_rate);
    /** @brief Returns maximum charging rate in normalized fill units per second. */
    double get_fill_rate() const;

    /** @brief Sets base draining rate in normalized fill units per second (default 2.0). */
    void set_drain_rate(double p_rate);
    /** @brief Returns base draining rate in normalized fill units per second. */
    double get_drain_rate() const;

    /** @brief Sets time in seconds to fill from 0.0 to 1.0 at max rate (inverse of fill_rate). */
    void set_fill_sec(double p_sec);
    /** @brief Returns time in seconds to fill from 0.0 to 1.0 at max rate. */
    double get_fill_sec() const;

    /** @brief Sets time in seconds to drain from 1.0 to 0.0 at base drain rate (inverse of drain_rate). */
    void set_drain_sec(double p_sec);
    /** @brief Returns time in seconds to drain from 1.0 to 0.0 at base drain rate. */
    double get_drain_sec() const;

    /** @brief Sets acceleration toward fill_rate in units/sec^2 (default 32.0). */
    void set_fill_acceleration(double p_accel);
    /** @brief Returns acceleration toward fill_rate in units/sec^2. */
    double get_fill_acceleration() const;

    /** @brief Sets deceleration / drain acceleration in units/sec^2 (default 32.0). */
    void set_drain_acceleration(double p_accel);
    /** @brief Returns drain acceleration in units/sec^2. */
    double get_drain_acceleration() const;

    /** @brief Sets distance (px) at which positive target_error doubles drain acceleration (default 100.0). */
    void set_error_doubling_distance(double p_dist);
    /** @brief Returns error doubling distance in logical pixels. */
    double get_error_doubling_distance() const;

    /** @brief Sets automatic stepping mode (PROCESS_MANUAL, PROCESS_IDLE, or PROCESS_PHYSICS). */
    void set_process_callback(ProcessCallback p_callback);
    /** @brief Returns automatic stepping mode. */
    ProcessCallback get_process_callback() const;

    /** @brief Sets signed target boundary distance in pixels (<= 0 charges, > 0 drains) used by automatic processing. */
    void set_target_error(double p_error);
    /** @brief Returns signed target boundary distance in pixels. */
    double get_target_error() const;

    /** @brief Sets boolean dwell active flag used by automatic processing when true. */
    void set_active_target(bool p_active);
    /** @brief Returns whether automatic processing treats the dwell target as active. */
    bool is_active_target_dwell() const;

    /**
     * @brief Advances the accumulator for a single time step given signed boundary error.
     * @param p_error Signed distance outside target boundary in logical pixels (<= 0 fills, > 0 drains).
     * @param p_delta Elapsed time in seconds.
     */
    void process_error_delta(double p_error, double p_delta);

    /**
     * @brief Advances the accumulator for a single time step given boolean activation.
     * @param p_is_active True to charge toward 1.0, false to drain toward 0.0.
     * @param p_delta Elapsed time in seconds.
     */
    void process_dwell_delta(bool p_is_active, double p_delta);

    /** @brief Resets fill and velocity to 0.0 and emits emptied/fill_changed signals if state changed. */
    void reset();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::FillAccumulator::ProcessCallback);
