#pragma once

#include <filesystem>

namespace tsdf {

enum class PointCloudFormat {
    kPcd = 0,
    kPly = 1,
};

enum class PointCloudLoadMode {
    kWholeMap = -1,
    kFrameSequence = 1,
};

struct BaseConfig {
    bool cuda_enabled = false;
    PointCloudFormat pointcloud_format = PointCloudFormat::kPcd;
    PointCloudLoadMode load_mode = PointCloudLoadMode::kWholeMap;
    std::filesystem::path data_root;
    std::filesystem::path rgb_path;
    std::filesystem::path depth_path;
    std::filesystem::path output_dir = "output";
    bool save_pcd = true;
    std::filesystem::path output_pcd_path;
    std::filesystem::path rgb_pose = "color_poses.txt";
    std::filesystem::path depth_pose = "depth_poses.txt";
};

struct FilterConfig {
    bool enable = true;
    double radius = 0.1;
    double n_sigma = 1.0;
    bool remove_isolated = false;
    bool use_absolute_error = false;
    double absolute_error = 0.5;
};

struct AppConfig {
    BaseConfig base;
    FilterConfig filter;
};

AppConfig loadAppConfig(const std::filesystem::path& file);

}  // namespace tsdf
