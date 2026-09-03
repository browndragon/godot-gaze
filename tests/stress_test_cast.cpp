#include "opencv_space_conversions.hpp"
#include "camera_interface.hpp"
#include <godot_cpp/variant/vector3.hpp>
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

template <typename T>
void verify_round_trip(const T &original, const std::string &name)
{
    // Verify standard layout
    static_assert(std::is_standard_layout<T>::value, "Must be standard layout");
    static_assert(std::is_trivial<T>::value, "Must be trivial");

    // Copy to byte buffer
    uint8_t buffer[sizeof(T)];
    std::memcpy(buffer, &original, sizeof(T));

    // Copy back from byte buffer
    T deserialized;
    std::memcpy(&deserialized, buffer, sizeof(T));
    std::cout << "[SERIALIZATION] Round-trip verified for: " << name << " (size: " << sizeof(T) << ", align: " << alignof(T) << ")" << std::endl;
}

int main()
{
    // 1. Compile-time assertions for compatibility
    static_assert(sizeof(Gaze::GodotCameraVector3) == sizeof(godot::Vector3), "Size mismatch");
    static_assert(alignof(Gaze::GodotCameraVector3) == alignof(godot::Vector3), "Alignment mismatch");

    std::cout << "[STRESS TEST] Size check: " << sizeof(Gaze::GodotCameraVector3) << " == " << sizeof(godot::Vector3) << " (PASSED)" << std::endl;
    std::cout << "[STRESS TEST] Align check: " << alignof(Gaze::GodotCameraVector3) << " == " << alignof(godot::Vector3) << " (PASSED)" << std::endl;

    // 2. Test casting from GodotCameraVector3 to godot::Vector3
    Gaze::GodotCameraVector3 g_vec(1.5, -2.7, 3.14);
    godot::Vector3 &godot_ref = reinterpret_cast<godot::Vector3 &>(g_vec);

    assert(godot_ref.x == 1.5);
    assert(godot_ref.y == -2.7);
    assert(godot_ref.z == 3.14);
    std::cout << "[STRESS TEST] GodotCameraVector3 -> godot::Vector3& cast matches values (PASSED)" << std::endl;

    // Modify via godot_ref
    godot_ref.x = 42.0;
    assert(g_vec.x == 42.0);
    std::cout << "[STRESS TEST] Modifying godot::Vector3& updates GodotCameraVector3 (PASSED)" << std::endl;

    // 3. Test casting from godot::Vector3 to GodotCameraVector3
    godot::Vector3 godot_vec(10.0, 20.0, 30.0);
    Gaze::GodotCameraVector3 &g_ref = reinterpret_cast<Gaze::GodotCameraVector3 &>(godot_vec);

    assert(g_ref.x == 10.0);
    assert(g_ref.y == 20.0);
    assert(g_ref.z == 30.0);
    std::cout << "[STRESS TEST] godot::Vector3 -> Gaze::GodotCameraVector3& cast matches values (PASSED)" << std::endl;

    // Modify via g_ref
    g_ref.y = -99.9;
    assert(godot_vec.y == -99.9);
    std::cout << "[STRESS TEST] Modifying Gaze::GodotCameraVector3& updates godot::Vector3 (PASSED)" << std::endl;

    // 4. Test pointer and array casting
    Gaze::GodotCameraVector3 g_arr[3] = {
        Gaze::GodotCameraVector3(1.0, 2.0, 3.0),
        Gaze::GodotCameraVector3(4.0, 5.0, 6.0),
        Gaze::GodotCameraVector3(7.0, 8.0, 9.0)};

    godot::Vector3 *godot_arr = reinterpret_cast<godot::Vector3 *>(g_arr);
    assert(godot_arr[0].x == 1.0 && godot_arr[0].y == 2.0 && godot_arr[0].z == 3.0);
    assert(godot_arr[1].x == 4.0 && godot_arr[1].y == 5.0 && godot_arr[1].z == 6.0);
    assert(godot_arr[2].x == 7.0 && godot_arr[2].y == 8.0 && godot_arr[2].z == 9.0);
    std::cout << "[STRESS TEST] Array casting GodotCameraVector3[] -> godot::Vector3* (PASSED)" << std::endl;

    // 5. Verify round-trip serializability of all math POD structs and Frame
    // SpacedVector2
    Gaze::GodotDisplayVector2 v2(12.34, 56.78);
    verify_round_trip(v2, "GodotDisplayVector2");
    {
        uint8_t buf[sizeof(Gaze::GodotDisplayVector2)];
        std::memcpy(buf, &v2, sizeof(v2));
        Gaze::GodotDisplayVector2 copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 12.34 && copy.y == 56.78);
        assert(sizeof(v2) == sizeof(double) * 2); // No padding gaps
    }

    // GazeRect
    Gaze::GazeRect rect(1.0f, 2.0f, 3.0f, 4.0f);
    verify_round_trip(rect, "GazeRect");
    {
        uint8_t buf[sizeof(Gaze::GazeRect)];
        std::memcpy(buf, &rect, sizeof(rect));
        Gaze::GazeRect copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 1.0f && copy.y == 2.0f && copy.width == 3.0f && copy.height == 4.0f);
        assert(sizeof(rect) == sizeof(float) * 4); // No padding gaps
    }

    // GodotCameraVector3
    Gaze::GodotCameraVector3 v3(1.0, 2.0, 3.0);
    verify_round_trip(v3, "GodotCameraVector3");
    {
        uint8_t buf[sizeof(Gaze::GodotCameraVector3)];
        std::memcpy(buf, &v3, sizeof(v3));
        Gaze::GodotCameraVector3 copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 1.0 && copy.y == 2.0 && copy.z == 3.0);
        assert(sizeof(v3) == sizeof(double) * 3); // No padding gaps
    }

    // SpacedBasis
    Gaze::SpacedBasis<Gaze::Space::GodotCamera, Gaze::Space::GodotCamera> basis(
        Gaze::GodotCameraVector3(1, 2, 3), Gaze::GodotCameraVector3(4, 5, 6), Gaze::GodotCameraVector3(7, 8, 9));
    verify_round_trip(basis, "SpacedBasis");
    {
        uint8_t buf[sizeof(basis)];
        std::memcpy(buf, &basis, sizeof(basis));
        Gaze::SpacedBasis<Gaze::Space::GodotCamera, Gaze::Space::GodotCamera> copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x.x == 1 && copy.x.y == 2 && copy.x.z == 3);
        assert(copy.y.x == 4 && copy.y.y == 5 && copy.y.z == 6);
        assert(copy.z.x == 7 && copy.z.y == 8 && copy.z.z == 9);
        assert(sizeof(basis) == sizeof(Gaze::GodotCameraVector3) * 3); // No padding gaps
    }

    // SpacedTransform3D
    Gaze::SpacedTransform3D<Gaze::Space::GodotCamera, Gaze::Space::GodotCamera> xform(basis, v3);
    verify_round_trip(xform, "SpacedTransform3D");
    {
        uint8_t buf[sizeof(xform)];
        std::memcpy(buf, &xform, sizeof(xform));
        Gaze::SpacedTransform3D<Gaze::Space::GodotCamera, Gaze::Space::GodotCamera> copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.basis.x.x == 1 && copy.basis.y.y == 5 && copy.basis.z.z == 9);
        assert(copy.origin.x == 1.0 && copy.origin.y == 2.0 && copy.origin.z == 3.0);
        assert(sizeof(xform) == sizeof(basis) + sizeof(Gaze::GodotCameraVector3)); // No padding gaps
    }

    // Frame
    unsigned char dummy_data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    Gaze::Frame frame;
    frame.width = 640;
    frame.height = 480;
    frame.data = dummy_data;
    frame.timestamp = 123.456;
    frame.buffer_idx = 7;
    verify_round_trip(frame, "Frame");
    {
        uint8_t buf[sizeof(Gaze::Frame)];
        std::memset(buf, 0, sizeof(buf));
        std::memcpy(buf, &frame, sizeof(frame));
        Gaze::Frame copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.width == 640 && copy.height == 480);
        assert(copy.data == dummy_data);
        assert(copy.timestamp == 123.456);
        assert(copy.buffer_idx == 7);

        size_t expected_member_size = sizeof(int) * 3 + sizeof(const unsigned char *) + sizeof(double);
        size_t padding_bytes = sizeof(Gaze::Frame) - expected_member_size;
        std::cout << "[PADDING CHECK] Frame struct: sizeof(Frame)=" << sizeof(Gaze::Frame)
                  << ", sum of member sizes=" << expected_member_size
                  << ", padding bytes=" << padding_bytes << std::endl;
    }

    std::cout << "[STRESS TEST] ALL CASTING & SERIALIZATION TESTS PASSED!" << std::endl;
    return 0;
}
