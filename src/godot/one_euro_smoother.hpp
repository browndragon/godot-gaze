#pragma once

#include "smoother.hpp"
#include "one_euro_filter.hpp"

namespace godot {

class OneEuroFilterState : public RefCounted {
    GDCLASS(OneEuroFilterState, RefCounted);

protected:
    static void _bind_methods();

public:
    OneEuroFilter* impl = nullptr;

    OneEuroFilterState();
    virtual ~OneEuroFilterState();
};

class OneEuroSmoother : public Smoother {
    GDCLASS(OneEuroSmoother, Smoother);

private:
    double min_cutoff = 1.0;
    double beta = 0.01;
    double d_cutoff = 1.0;

protected:
    static void _bind_methods();

public:
    OneEuroSmoother();
    virtual ~OneEuroSmoother();

    virtual Array _smoother_init() override;
    virtual Variant _smoother_next(Array state, double tstamp, Variant value) override;

    void set_min_cutoff(double val);
    double get_min_cutoff() const;

    void set_beta(double val);
    double get_beta() const;

    void set_d_cutoff(double val);
    double get_d_cutoff() const;
};

} // namespace godot
