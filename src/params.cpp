#include "params.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string normalize(std::string v) {
    for (char& c : v) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return v;
}

std::string canonicalizeKey(std::string key) {
    for (char& c : key) {
        if (c == ' ' || c == '\t' || c == '-') {
            c = '_';
        }
    }
    return key;
}

}  // namespace

Config loadConfig(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("无法打开配置文件: " + file.string());
    }

    Config cfg;
    enum class Section { None, Base, Filter };
    Section section = Section::None;
    const std::filesystem::path configDir = std::filesystem::absolute(file).parent_path();

    auto parseDouble = [&](const std::string& key, const std::string& value) {
        try {
            return std::stod(value);
        } catch (...) {
            throw std::runtime_error("字段 " + key + " 解析失败: " + value);
        }
    };
    auto parseBool = [&](const std::string& value) {
        const std::string lower = normalize(value);
        return (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
    };

    std::string line;
    while (std::getline(in, line)) {
        const std::string current = trim(line);
        if (current.empty() || current.front() == '#') {
            continue;
        }

        const auto pos = current.find(':');
        if (pos == std::string::npos) {
            continue;
        }

        const std::string rawKey = trim(current.substr(0, pos));
        std::string value = trim(current.substr(pos + 1));
        if (value.empty() || value.front() == '#') {
            const std::string header = normalize(rawKey);
            if (header == "base") {
                section = Section::Base;
            } else if (header == "filter") {
                section = Section::Filter;
            } else {
                section = Section::None;
            }
            continue;
        }

        std::string key = canonicalizeKey(normalize(rawKey));
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (section == Section::Base) {
            if (key == "depth_path") {
                cfg.base.depth_path = value;
            } else if (key == "data_path" || key == "data_root") {
                cfg.base.data_root = value;
            } else if (key == "output_path" || key == "output_dir") {
                cfg.base.output_dir = value;
            } else if (key == "save_pcd_en" || key == "save_pcd" || key == "save_output_en") {
                cfg.base.save_pcd = parseBool(value);
            } else if (key == "output_pcd_path") {
                cfg.base.output_pcd_path = value;
            }
        } else if (section == Section::Filter) {
            if (key == "enable" || key == "enabled" || key == "denoise_en") {
                cfg.filter.enable = parseBool(value);
            } else if (key == "radius") {
                cfg.filter.radius = parseDouble("radius", value);
            } else if (key == "nsigma" || key == "n_sigma" || key == "max_error" || key == "maxerror") {
                cfg.filter.n_sigma = parseDouble("nSigma", value);
                cfg.filter.use_absolute_error = false;
            } else if (key == "absolute_error" || key == "absoluteerror") {
                cfg.filter.absolute_error = parseDouble("absolute_error", value);
                cfg.filter.use_absolute_error = true;
            } else if (key == "use_absolute_error") {
                cfg.filter.use_absolute_error = parseBool(value);
            } else if (key == "remove_isolated") {
                cfg.filter.remove_isolated = parseBool(value);
            }
        }
    }

    if (cfg.base.depth_path.empty()) {
        throw std::runtime_error("配置缺少 Base.depth_path");
    }

    if (cfg.base.data_root.empty()) {
        cfg.base.data_root = configDir;
    } else if (cfg.base.data_root.is_relative()) {
        cfg.base.data_root = (configDir / cfg.base.data_root).lexically_normal();
    }

    if (cfg.base.depth_path.is_relative()) {
        cfg.base.depth_path = (cfg.base.data_root / cfg.base.depth_path).lexically_normal();
    }

    {
        std::filesystem::path resolved = cfg.base.output_dir;
        if (resolved.is_relative()) {
            resolved = (configDir / resolved).lexically_normal();
        }
        cfg.base.output_dir = std::filesystem::absolute(resolved);
        std::filesystem::create_directories(cfg.base.output_dir);
    }

    if (!cfg.base.output_pcd_path.empty()) {
        if (cfg.base.output_pcd_path.is_relative()) {
            cfg.base.output_pcd_path = (configDir / cfg.base.output_pcd_path).lexically_normal();
        }
        cfg.base.output_pcd_path = std::filesystem::absolute(cfg.base.output_pcd_path);
    }

    return cfg;
}
