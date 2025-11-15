#pragma once

#include <filesystem>
#include <string>

struct Config {
    struct Base {
        std::filesystem::path data_root;
        std::filesystem::path depth_path;
        std::filesystem::path output_dir = "output";
        bool save_pcd = true;
        std::filesystem::path output_pcd_path;
    } base;

    struct Filter {
        bool enable = true;
        double radius = 0.1;
        double n_sigma = 1.0;
        bool remove_isolated = false;
        bool use_absolute_error = false;
        double absolute_error = 0.5;
    } filter;
};

Config loadConfig(const std::filesystem::path& file);
