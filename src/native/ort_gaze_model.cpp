#include "ort_gaze_model.hpp"
#include "opencv_space_conversions.hpp"
#include "log.hpp"
#include "platform_ort.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace Gaze {

ORTGazeModel::ORTGazeModel(const std::string& gaze_ort_path)
    : model_path(gaze_ort_path),
      memory_info(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU)) {
}

ORTGazeModel::ORTGazeModel(const std::vector<uint8_t>& ort_buffer)
    : model_buffer(ort_buffer), load_from_buffer(true),
      memory_info(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU)) {
}

ORTGazeModel::ORTGazeModel(const std::vector<uint8_t>& xml_buffer, const std::vector<uint8_t>& bin_buffer)
    : ORTGazeModel(xml_buffer) {
    // OpenVINO fallback to ONNX conversion, we just use the first buffer (which should be .ort or .onnx)
}

bool ORTGazeModel::initialize() {
    try {
        get_ort_env();
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        if (load_from_buffer) {
            log_info("ORTGazeModelInitAttemptBuffer", "size", (int)model_buffer.size());
            session = platform_create_ort_session(get_ort_env(), model_buffer, &session_options);
        } else {
            log_info("ORTGazeModelInitAttempt", "model_path", model_path);
            session = platform_create_ort_session(get_ort_env(), model_path, &session_options);
        }

        if (!session) {
            log_error("ORTGazeModelSessionNull");
            return false;
        }

    } catch (...) {
        log_warning("ORTGazeModelEPFailed", "reason", "EP initialization threw exception, falling back to CPU");
        try {
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
            if (load_from_buffer) {
                session = platform_create_ort_session(get_ort_env(), model_buffer, &session_options);
            } else {
                session = platform_create_ort_session(get_ort_env(), model_path, &session_options);
            }
            if (!session) {
                log_error("ORTGazeModelSessionNullCPU");
                return false;
            }
        } catch (const std::exception& e) {
            log_error("ORTGazeModelCPUFailed", "what", e.what());
            return false;
        } catch (...) {
            log_error("ORTGazeModelCPUFailedUnknown");
            return false;
        }
    }
    try {
        Ort::AllocatorWithDefaultOptions allocator;
        size_t num_input_nodes = session->GetInputCount();
        for (size_t i = 0; i < num_input_nodes; i++) {
            auto name = session->GetInputNameAllocated(i, allocator);
            log_info("ORTGazeModelInputNode", "index", (int)i, "name", name.get());
        }
        size_t num_output_nodes = session->GetOutputCount();
        for (size_t i = 0; i < num_output_nodes; i++) {
            auto name = session->GetOutputNameAllocated(i, allocator);
            log_info("ORTGazeModelOutputNode", "index", (int)i, "name", name.get());
        }
    } catch (...) {}
    log_info("ORTGazeModelInitSuccess");
    return true;
}

void ORTGazeModel::preprocess_eye_crop(const uint8_t* raw_crop_bgr, float* out_buffer) {
    constexpr int channel_size = EyeCrops::EYE_CROP_WIDTH * EyeCrops::EYE_CROP_HEIGHT;
    for (int i = 0; i < channel_size; ++i) {
        // Raw crop is BGR8. OpenVINO ADAS model expects NCHW BGR tensor:
        // Channel 0 = Blue, Channel 1 = Green, Channel 2 = Red
        out_buffer[i] = static_cast<float>(raw_crop_bgr[3 * i + 0]);                 // Blue
        out_buffer[channel_size + i] = static_cast<float>(raw_crop_bgr[3 * i + 1]);  // Green
        out_buffer[2 * channel_size + i] = static_cast<float>(raw_crop_bgr[3 * i + 2]);  // Red
    }
}

bool ORTGazeModel::estimate_raw_gaze(const EyeCrops& crops, OpenVINOGazeVector3& out_gaze_dir_openvino) {
    if (!session) return false;

    std::vector<float> left_eye_tensor_data(EyeCrops::EYE_CROP_SIZE, 0.0f);
    std::vector<float> right_eye_tensor_data(EyeCrops::EYE_CROP_SIZE, 0.0f);
    std::vector<float> head_pose_tensor_data(3, 0.0f);

    // 1. Preprocess eye crops
    // OpenVINO ADAS gaze-estimation-adas-0002 model specification:
    // "left_eye_image" is the crop of the eye appearing on IMAGE-LEFT (landmarks [0, 1], subject's anatomical right eye).
    // "right_eye_image" is the crop of the eye appearing on IMAGE-RIGHT (landmarks [2, 3], subject's anatomical left eye).
    // In EyeCrops, right_eye_data contains image-left (landmarks [0, 1]) and left_eye_data contains image-right (landmarks [2, 3]).
    preprocess_eye_crop(crops.right_eye_data, left_eye_tensor_data.data());
    preprocess_eye_crop(crops.left_eye_data, right_eye_tensor_data.data());

    // 2. Prepare head pose angles
    if (std::isnan(crops.head_pose_rotation.x) || std::isnan(crops.head_pose_rotation.y) || std::isnan(crops.head_pose_rotation.z) ||
        std::isinf(crops.head_pose_rotation.x) || std::isinf(crops.head_pose_rotation.y) || std::isinf(crops.head_pose_rotation.z)) {
        return false;
    }

    SpacedVector3<Space::OpenCVCamera> openvino_angles = CoordinateConversions::opencv_head_pose_to_openvino_angles_deg(crops.head_pose_rotation);
    head_pose_tensor_data[0] = static_cast<float>(openvino_angles.x); // Yaw
    head_pose_tensor_data[1] = static_cast<float>(openvino_angles.y); // Pitch
    head_pose_tensor_data[2] = static_cast<float>(openvino_angles.z); // Roll

    // 3. Create input tensors referencing staging buffers
    std::vector<int64_t> eye_shape = {1, 3, EyeCrops::EYE_CROP_WIDTH, EyeCrops::EYE_CROP_HEIGHT};
    std::vector<int64_t> head_shape = {1, 3};

    Ort::Value left_tensor = Ort::Value::CreateTensor<float>(
        memory_info, left_eye_tensor_data.data(), left_eye_tensor_data.size(),
        eye_shape.data(), eye_shape.size()
    );
    Ort::Value right_tensor = Ort::Value::CreateTensor<float>(
        memory_info, right_eye_tensor_data.data(), right_eye_tensor_data.size(),
        eye_shape.data(), eye_shape.size()
    );
    Ort::Value head_tensor = Ort::Value::CreateTensor<float>(
        memory_info, head_pose_tensor_data.data(), head_pose_tensor_data.size(),
        head_shape.data(), head_shape.size()
    );

    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(left_tensor));
    inputs.push_back(std::move(right_tensor));
    inputs.push_back(std::move(head_tensor));

    try {
        // Run forward pass
        auto output_values = session->Run(
            Ort::RunOptions{nullptr},
            input_names.data(),
            inputs.data(),
            inputs.size(),
            output_names.data(),
            output_names.size()
        );

        if (output_values.empty()) return false;

        float* out_data = output_values[0].GetTensorMutableData<float>();
        size_t num_elements = output_values[0].GetTensorTypeAndShapeInfo().GetElementCount();

        if (num_elements == 2) {
            double pitch = out_data[0];
            double yaw = out_data[1];
            double cos_pitch = std::cos(pitch);
            double dx = std::sin(yaw) * cos_pitch;
            double dy = std::sin(pitch);
            double dz = std::cos(yaw) * cos_pitch;
            out_gaze_dir_openvino = OpenVINOGazeVector3(dx, -dy, dz).normalized();
        } else if (num_elements == 3) {
            // OpenVINO ADAS gaze model outputs 3D gaze vector (dx, dy, dz) in subject frame:
            out_gaze_dir_openvino = OpenVINOGazeVector3(out_data[0], out_data[1], out_data[2]).normalized();
        } else {
            return false;
        }

    } catch (const std::exception& e) {
        log_error("ORTGazeModelForwardException", "what", e.what());
        return false;
    }

    return true;
}

} // namespace Gaze
