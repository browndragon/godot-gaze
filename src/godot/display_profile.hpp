#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace godot {

/**
 * @class DisplayProfile
 * @brief Resource representing the physical and logical geometry settings of the display panel.
 *
 * Exposes logical panel dimensions (pixels/points) and physical dimensions (millimeters),
 * and computes the derived DPI ratio (pixels per inch) dynamically.
 */
class DisplayProfile : public Resource {
    GDCLASS(DisplayProfile, Resource);

private:
    Vector2i logical_size_px = Vector2i(0, 0);
    Vector2 physical_size_mm = Vector2(0.0, 0.0);

protected:
    static void _bind_methods();

public:
    DisplayProfile();
    virtual ~DisplayProfile();

    void set_logical_size_px(Vector2i size);
    Vector2i get_logical_size_px() const;

    void set_physical_size_mm(Vector2 size);
    Vector2 get_physical_size_mm() const;

    Vector2 get_dpi() const;

    static Ref<DisplayProfile> estimate_from_os();
};

} // namespace godot
