// TODO: Why "stress test"? Feels weird. Seems unnecessary. It's just a unit test. `typecast_test.cpp` seems fine?
// Prefer `_test.cpp` naming, so that all tests end with suffix `_test.cpp` ("Google style").
#include "math_defs.hpp"
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
    // TODO: Is this true that we've actually verified it? don't we then need to check that the original and this copied value survived the round trip equivalently? Or we _assume_ that's fine? Other than asserting std layout + trivial, does actually performing the copy _do_ anything?!
    // TODO: Our contribution guidelines say to always use our logging facilities instead of direct std::cout. I'm worried that agents will get confused. If our logging facilities need to expose more syntax sugar to make them drop in replacement (like the `<<` operator), then we should do that.
    std::cout << "[SERIALIZATION] Round-trip verified for: " << name << " (size: " << sizeof(T) << ", align: " << alignof(T) << ")" << std::endl;
}

int main()
{
    // 1. Compile-time assertions for compatibility
    static_assert(sizeof(Gaze::GazeVector3) == sizeof(godot::Vector3), "Size mismatch");
    static_assert(alignof(Gaze::GazeVector3) == alignof(godot::Vector3), "Alignment mismatch");

    std::cout << "[STRESS TEST] Size check: " << sizeof(Gaze::GazeVector3) << " == " << sizeof(godot::Vector3) << " (PASSED)" << std::endl;
    std::cout << "[STRESS TEST] Align check: " << alignof(Gaze::GazeVector3) << " == " << alignof(godot::Vector3) << " (PASSED)" << std::endl;

    // 2. Test casting from GazeVector3 to godot::Vector3
    Gaze::GazeVector3 g_vec(1.5f, -2.7f, 3.14f);
    godot::Vector3 &godot_ref = reinterpret_cast<godot::Vector3 &>(g_vec);

    assert(godot_ref.x == 1.5f);
    assert(godot_ref.y == -2.7f);
    assert(godot_ref.z == 3.14f);
    std::cout << "[STRESS TEST] GazeVector3 -> godot::Vector3& cast matches values (PASSED)" << std::endl;

    // Modify via godot_ref
    godot_ref.x = 42.0f;
    assert(g_vec.x == 42.0f);
    std::cout << "[STRESS TEST] Modifying godot::Vector3& updates GazeVector3 (PASSED)" << std::endl;

    // 3. Test casting from godot::Vector3 to GazeVector3
    godot::Vector3 godot_vec(10.0f, 20.0f, 30.0f);
    Gaze::GazeVector3 &g_ref = reinterpret_cast<Gaze::GazeVector3 &>(godot_vec);

    assert(g_ref.x == 10.0f);
    assert(g_ref.y == 20.0f);
    assert(g_ref.z == 30.0f);
    std::cout << "[STRESS TEST] godot::Vector3 -> Gaze::GazeVector3& cast matches values (PASSED)" << std::endl;

    // Modify via g_ref
    g_ref.y = -99.9f;
    assert(godot_vec.y == -99.9f);
    std::cout << "[STRESS TEST] Modifying Gaze::GazeVector3& updates godot::Vector3 (PASSED)" << std::endl;

    // 4. Test pointer and array casting
    Gaze::GazeVector3 g_arr[3] = {
        Gaze::GazeVector3(1.0f, 2.0f, 3.0f),
        Gaze::GazeVector3(4.0f, 5.0f, 6.0f),
        Gaze::GazeVector3(7.0f, 8.0f, 9.0f)};

    godot::Vector3 *godot_arr = reinterpret_cast<godot::Vector3 *>(g_arr);
    assert(godot_arr[0].x == 1.0f && godot_arr[0].y == 2.0f && godot_arr[0].z == 3.0f);
    assert(godot_arr[1].x == 4.0f && godot_arr[1].y == 5.0f && godot_arr[1].z == 6.0f);
    assert(godot_arr[2].x == 7.0f && godot_arr[2].y == 8.0f && godot_arr[2].z == 9.0f);
    std::cout << "[STRESS TEST] Array casting GazeVector3[] -> godot::Vector3* (PASSED)" << std::endl;

    // 5. Verify round-trip serializability of all math POD structs and Frame
    // GazeVector2
    Gaze::GazeVector2 v2(12.34, 56.78);
    verify_round_trip(v2, "GazeVector2");
    {
        uint8_t buf[sizeof(Gaze::GazeVector2)];
        std::memcpy(buf, &v2, sizeof(v2));
        Gaze::GazeVector2 copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 12.34 && copy.y == 56.78);
        assert(sizeof(v2) == sizeof(double) * 2); // No padding gaps
    }

    // GazeVector2i
    Gaze::GazeVector2i v2i(42, -7);
    verify_round_trip(v2i, "GazeVector2i");
    {
        uint8_t buf[sizeof(Gaze::GazeVector2i)];
        std::memcpy(buf, &v2i, sizeof(v2i));
        Gaze::GazeVector2i copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 42 && copy.y == -7);
        assert(sizeof(v2i) == sizeof(int) * 2); // No padding gaps
    }

    // GazePoint
    Gaze::GazePoint pt(3.14f, 2.71f);
    verify_round_trip(pt, "GazePoint");
    {
        uint8_t buf[sizeof(Gaze::GazePoint)];
        std::memcpy(buf, &pt, sizeof(pt));
        Gaze::GazePoint copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 3.14f && copy.y == 2.71f);
        assert(sizeof(pt) == sizeof(float) * 2); // No padding gaps
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

    // GazeVector3
    Gaze::GazeVector3 v3(1.0f, 2.0f, 3.0f);
    verify_round_trip(v3, "GazeVector3");
    {
        uint8_t buf[sizeof(Gaze::GazeVector3)];
        std::memcpy(buf, &v3, sizeof(v3));
        Gaze::GazeVector3 copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x == 1.0f && copy.y == 2.0f && copy.z == 3.0f);
        assert(sizeof(v3) == sizeof(float) * 3); // No padding gaps
    }

    // GazeBasis3D
    Gaze::GazeBasis3D basis(Gaze::GazeVector3(1, 2, 3), Gaze::GazeVector3(4, 5, 6), Gaze::GazeVector3(7, 8, 9));
    verify_round_trip(basis, "GazeBasis3D");
    {
        uint8_t buf[sizeof(Gaze::GazeBasis3D)];
        std::memcpy(buf, &basis, sizeof(basis));
        Gaze::GazeBasis3D copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.x.x == 1 && copy.x.y == 2 && copy.x.z == 3);
        assert(copy.y.x == 4 && copy.y.y == 5 && copy.y.z == 6);
        assert(copy.z.x == 7 && copy.z.y == 8 && copy.z.z == 9);
        assert(sizeof(basis) == sizeof(Gaze::GazeVector3) * 3); // No padding gaps
    }

    // GazeTransform3D
    Gaze::GazeTransform3D xform(basis, v3);
    verify_round_trip(xform, "GazeTransform3D");
    {
        uint8_t buf[sizeof(Gaze::GazeTransform3D)];
        std::memcpy(buf, &xform, sizeof(xform));
        Gaze::GazeTransform3D copy;
        std::memcpy(&copy, buf, sizeof(copy));
        assert(copy.basis.x.x == 1 && copy.basis.y.y == 5 && copy.basis.z.z == 9);
        assert(copy.origin.x == 1.0f && copy.origin.y == 2.0f && copy.origin.z == 3.0f);
        assert(sizeof(xform) == sizeof(Gaze::GazeBasis3D) + sizeof(Gaze::GazeVector3)); // No padding gaps
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
