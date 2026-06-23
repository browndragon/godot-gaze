#pragma once

#include <cmath>
#include <stdexcept>

class LowPassFilter {
private:
    double y; // Last filtered value
    bool initialized;

public:
    LowPassFilter() : y(0.0), initialized(false) {}

    double filter(double value, double alpha) {
        if (!initialized) {
            y = value;
            initialized = true;
            return y;
        }
        y = alpha * value + (1.0 - alpha) * y;
        return y;
    }

    double last_value() const {
        return y;
    }

    bool is_initialized() const {
        return initialized;
    }

    void reset() {
        initialized = false;
        y = 0.0;
    }
};

class OneEuroFilter {
private:
    double freq;
    double mincutoff;
    double beta;
    double dcutoff;

    LowPassFilter x_filt;
    LowPassFilter dx_filt;
    double last_time;

    double alpha(double cutoff) {
        double tau = 1.0 / (2.0 * M_PI * cutoff);
        double te = 1.0 / freq;
        return 1.0 / (1.0 + tau / te);
    }

public:
    OneEuroFilter(double freq = 60.0, double mincutoff = 1.0, double beta = 0.0, double dcutoff = 1.0)
        : freq(freq), mincutoff(mincutoff), beta(beta), dcutoff(dcutoff), last_time(-1.0) {
#ifdef __cpp_exceptions
        if (freq <= 0.0) throw std::invalid_argument("frequency must be > 0");
        if (mincutoff <= 0.0) throw std::invalid_argument("mincutoff must be > 0");
        if (dcutoff <= 0.0) throw std::invalid_argument("dcutoff must be > 0");
#endif
    }

    double filter(double value, double timestamp = -1.0) {
        // Compute active frequency if timestamp is provided
        if (last_time != -1.0 && timestamp != -1.0) {
            double dt = timestamp - last_time;
            if (dt > 0.0) {
                freq = 1.0 / dt;
            }
        }
        last_time = timestamp;

        // Estimate local derivative (speed)
        double dx = 0.0;
        if (x_filt.is_initialized()) {
            double dt = (freq > 0.0) ? (1.0 / freq) : 0.016;
            dx = (value - x_filt.last_value()) / dt;
        }

        double edx = dx_filt.filter(dx, alpha(dcutoff));
        double cutoff = mincutoff + beta * std::abs(edx);
        return x_filt.filter(value, alpha(cutoff));
    }

    void reset() {
        x_filt.reset();
        dx_filt.reset();
        last_time = -1.0;
    }

    void set_min_cutoff(double val) { mincutoff = val; }
    void set_beta(double val) { beta = val; }
    void set_d_cutoff(double val) { dcutoff = val; }
    void set_frequency(double val) { freq = val; }
};
