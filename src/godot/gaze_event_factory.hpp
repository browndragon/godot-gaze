#pragma once

#include <godot_cpp/classes/resource.hpp>
#include "input_event_gaze.hpp"

namespace godot {

/**
 * @class GazeEventFactory
 * @brief Base Resource for constructing, enriching, and minting gaze input events.
 */
class GazeEventFactory : public Resource {
    GDCLASS(GazeEventFactory, Resource);

protected:
    static void _bind_methods();

public:
    GazeEventFactory() = default;
    virtual ~GazeEventFactory() = default;

    virtual Ref<InputEventGazeBase> create_gaze_event();
    virtual Ref<InputEventGazeBase> create_missing_event(int p_reason);
};

/**
 * @class GazeServerEventFactory
 * @brief Concrete C++ default event factory that mints standard InputEventGaze from GazeServer.
 */
class GazeServerEventFactory : public GazeEventFactory {
    GDCLASS(GazeServerEventFactory, GazeEventFactory);

protected:
    static void _bind_methods();

public:
    GazeServerEventFactory() = default;
    virtual ~GazeServerEventFactory() = default;

    virtual Ref<InputEventGazeBase> create_gaze_event() override;
    virtual Ref<InputEventGazeBase> create_missing_event(int p_reason) override;
};

} // namespace godot
