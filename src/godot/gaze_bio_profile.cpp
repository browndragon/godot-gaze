/**
 * @file gaze_bio_profile.cpp
 * @brief Implement GazeBioProfile resource
 */
#include "gaze_bio_profile.hpp"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void GazeBioProfile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_bias_pitch_deg", "bias"), &GazeBioProfile::set_bias_pitch_deg);
    ClassDB::bind_method(D_METHOD("get_bias_pitch_deg"), &GazeBioProfile::get_bias_pitch_deg);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_pitch_deg"), "set_bias_pitch_deg", "get_bias_pitch_deg");

    ClassDB::bind_method(D_METHOD("set_bias_yaw_deg", "bias"), &GazeBioProfile::set_bias_yaw_deg);
    ClassDB::bind_method(D_METHOD("get_bias_yaw_deg"), &GazeBioProfile::get_bias_yaw_deg);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias_yaw_deg"), "set_bias_yaw_deg", "get_bias_yaw_deg");

    ClassDB::bind_method(D_METHOD("reset"), &GazeBioProfile::reset);
}

void GazeBioProfile::set_bias_pitch_deg(double p_val) {
    bias_pitch_deg = p_val;
    emit_changed();
}

void GazeBioProfile::set_bias_yaw_deg(double p_val) {
    bias_yaw_deg = p_val;
    emit_changed();
}

void GazeBioProfile::reset() {
    bias_pitch_deg = 0.0;
    bias_yaw_deg = 0.0;
    emit_changed();
}

void GazeBioProfile::_write_to_config(Ref<ConfigFile> &p_cfg) const {
    p_cfg->set_value("bio_profile", "bias_pitch_deg", bias_pitch_deg);
    p_cfg->set_value("bio_profile", "bias_yaw_deg", bias_yaw_deg);
}

Error GazeBioProfile::_read_from_config(const Ref<ConfigFile> &p_cfg) {
    if (!p_cfg->has_section("bio_profile")) {
        return ERR_FILE_CORRUPT;
    }
    set_bias_pitch_deg(p_cfg->get_value("bio_profile", "bias_pitch_deg", 0.0));
    set_bias_yaw_deg(p_cfg->get_value("bio_profile", "bias_yaw_deg", 0.0));
    return OK;
}

} // namespace godot
