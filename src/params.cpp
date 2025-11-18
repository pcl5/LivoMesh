#include "params.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string toLowerCopy(std::string v) {
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

std::string stripQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == value.back()
        && (value.front() == '"' || value.front() == '\'')) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string stripInlineComment(const std::string& line) {
    bool inQuotes = false;
    char currentQuote = '\0';
    std::string result;
    result.reserve(line.size());
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if ((c == '"' || c == '\'') && (i == 0 || line[i - 1] != '\\')) {
            if (!inQuotes) {
                inQuotes = true;
                currentQuote = c;
            } else if (currentQuote == c) {
                inQuotes = false;
            }
        }
        if (c == '#' && !inQuotes) {
            break;
        }
        result.push_back(c);
    }
    return result;
}

class RawConfig {
public:
    explicit RawConfig(const std::filesystem::path& file) : file_(file) {
        std::ifstream in(file);
        if (!in) {
            throw std::runtime_error("无法打开配置文件: " + file.string());
        }
        parse(in);
    }

    std::optional<std::string> get(const std::string& section, const std::string& key) const {
        const std::string sectionKey = canonicalizeKey(toLowerCopy(section));
        const std::string normalizedKey = canonicalizeKey(toLowerCopy(key));
        const auto sectionIt = values_.find(sectionKey);
        if (sectionIt == values_.end()) {
            return std::nullopt;
        }
        const auto keyIt = sectionIt->second.find(normalizedKey);
        if (keyIt == sectionIt->second.end()) {
            return std::nullopt;
        }
        return keyIt->second;
    }

private:
    void parse(std::istream& in) {
        std::string currentSection = canonicalizeKey("global");
        values_[currentSection];
        std::string line;
        while (std::getline(in, line)) {
            std::string cleaned = trim(stripInlineComment(line));
            if (cleaned.empty() || cleaned.front() == '#') {
                continue;
            }

            const auto pos = cleaned.find(':');
            if (pos == std::string::npos) {
                continue;
            }

            std::string rawKey = trim(cleaned.substr(0, pos));
            std::string rawValue = trim(cleaned.substr(pos + 1));
            if (rawKey.empty()) {
                continue;
            }

            if (rawValue.empty()) {
                currentSection = canonicalizeKey(toLowerCopy(rawKey));
                values_[currentSection];
                continue;
            }

            const std::string key = canonicalizeKey(toLowerCopy(rawKey));
            const std::string value = trim(stripQuotes(rawValue));
            values_[currentSection][key] = value;
        }
    }

    std::filesystem::path file_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> values_;
};

struct NamedValue {
    std::string key;
    std::string value;
};

std::optional<NamedValue> pickValue(const RawConfig& raw, const std::string& section, std::initializer_list<std::string_view> keys) {
    for (const std::string_view key : keys) {
        if (auto value = raw.get(section, std::string(key))) {
            return NamedValue{std::string(key), *value};
        }
    }
    return std::nullopt;
}

bool parseBool(const std::string& value, const std::string& fieldName) {
    const std::string normalized = toLowerCopy(trim(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        return false;
    }
    throw std::runtime_error("字段 " + fieldName + " 解析失败: " + value);
}

double parseDouble(const std::string& value, const std::string& fieldName) {
    try {
        return std::stod(value);
    } catch (...) {
        throw std::runtime_error("字段 " + fieldName + " 解析失败: " + value);
    }
}

int parseInt(const std::string& value, const std::string& fieldName) {
    try {
        return std::stoi(value);
    } catch (...) {
        throw std::runtime_error("字段 " + fieldName + " 解析失败: " + value);
    }
}

tsdf::PointCloudFormat parsePointCloudFormat(const std::string& value, const std::string& fieldName) {
    const std::string normalized = toLowerCopy(trim(value));
    if (normalized == "0" || normalized == "pcd") {
        return tsdf::PointCloudFormat::kPcd;
    }
    if (normalized == "1" || normalized == "ply") {
        return tsdf::PointCloudFormat::kPly;
    }
    throw std::runtime_error("字段 " + fieldName + " 仅支持 pcd/ply 或 0/1");
}

tsdf::PointCloudLoadMode parsePointCloudLoadMode(const std::string& value, const std::string& fieldName) {
    const std::string normalized = toLowerCopy(trim(value));
    if (normalized == "-1" || normalized == "map" || normalized == "full" || normalized == "global") {
        return tsdf::PointCloudLoadMode::kWholeMap;
    }
    if (normalized == "1" || normalized == "frames" || normalized == "multi" || normalized == "sequence") {
        return tsdf::PointCloudLoadMode::kFrameSequence;
    }
    throw std::runtime_error("字段 " + fieldName + " 仅支持 -1/1 或 map/frames");
}

std::filesystem::path resolveRelativeTo(const std::filesystem::path& anchor, std::filesystem::path candidate) {
    if (candidate.empty()) {
        return candidate;
    }
    if (candidate.is_absolute() || anchor.empty()) {
        return candidate.lexically_normal();
    }
    return (anchor / candidate).lexically_normal();
}

std::filesystem::path resolveRelativeTo(const std::filesystem::path& anchor, const std::string& raw) {
    if (raw.empty()) {
        return {};
    }
    return resolveRelativeTo(anchor, std::filesystem::path(raw));
}

std::filesystem::path makeAbsolute(const std::filesystem::path& input) {
    if (input.empty()) {
        return input;
    }
    return std::filesystem::absolute(input.lexically_normal());
}

}  // namespace

namespace tsdf {

AppConfig loadAppConfig(const std::filesystem::path& file) {
    RawConfig raw(file);
    AppConfig cfg;

    const std::filesystem::path configPath = std::filesystem::absolute(file);
    const std::filesystem::path configDir = configPath.parent_path();

    if (auto value = pickValue(raw, "base", {"data_root", "data_path"})) {
        cfg.base.data_root = resolveRelativeTo(configDir, value->value);
    } else {
        cfg.base.data_root = configDir;
    }
    cfg.base.data_root = makeAbsolute(cfg.base.data_root);

    if (auto value = pickValue(raw, "base", {"cuda_en", "use_cuda"})) {
        cfg.base.cuda_enabled = parseBool(value->value, "Base." + value->key);
    }
    if (auto value = pickValue(raw, "base", {"pcl_type", "point_cloud_type"})) {
        cfg.base.pointcloud_format = parsePointCloudFormat(value->value, "Base." + value->key);
    }
    if (auto value = pickValue(raw, "base", {"pcl_load", "load_mode"})) {
        cfg.base.load_mode = parsePointCloudLoadMode(value->value, "Base." + value->key);
    }
    if (auto value = pickValue(raw, "base", {"save_pcd_en", "save_pcd", "save_output_en"})) {
        cfg.base.save_pcd = parseBool(value->value, "Base." + value->key);
    }

    std::filesystem::path desiredOutputDir = cfg.base.output_dir;
    if (auto value = pickValue(raw, "base", {"output_dir", "output_path"})) {
        desiredOutputDir = value->value;
    }
    cfg.base.output_dir = makeAbsolute(resolveRelativeTo(configDir, desiredOutputDir));
    std::filesystem::create_directories(cfg.base.output_dir);

    if (auto value = pickValue(raw, "base", {"output_pcd_path"})) {
        cfg.base.output_pcd_path = makeAbsolute(resolveRelativeTo(configDir, value->value));
    } else {
        cfg.base.output_pcd_path.clear();
    }

    auto resolveDataPath = [&](std::filesystem::path candidate) -> std::filesystem::path {
        if (candidate.empty()) {
            return candidate;
        }
        return makeAbsolute(resolveRelativeTo(cfg.base.data_root, std::move(candidate)));
    };

    if (auto value = pickValue(raw, "base", {"rgb_path"})) {
        cfg.base.rgb_path = resolveDataPath(std::filesystem::path(value->value));
    }

    if (auto value = pickValue(raw, "base", {"depth_path"})) {
        cfg.base.depth_path = resolveDataPath(std::filesystem::path(value->value));
    }
    if (cfg.base.depth_path.empty()) {
        throw std::runtime_error("配置缺少 Base.depth_path");
    }

    if (auto value = pickValue(raw, "base", {"rgb_pose"})) {
        cfg.base.rgb_pose = resolveDataPath(std::filesystem::path(value->value));
    } else {
        cfg.base.rgb_pose = resolveDataPath(cfg.base.rgb_pose);
    }

    if (auto value = pickValue(raw, "base", {"depth_pose"})) {
        cfg.base.depth_pose = resolveDataPath(std::filesystem::path(value->value));
    } else {
        cfg.base.depth_pose = resolveDataPath(cfg.base.depth_pose);
    }

    if (auto value = pickValue(raw, "filter", {"enable", "enabled", "denoise_en"})) {
        cfg.filter.enable = parseBool(value->value, "Filter." + value->key);
    }
    if (auto value = pickValue(raw, "filter", {"radius"})) {
        cfg.filter.radius = parseDouble(value->value, "Filter." + value->key);
    }
    if (auto value = pickValue(raw, "filter", {"n_sigma", "nsigma"})) {
        cfg.filter.n_sigma = parseDouble(value->value, "Filter." + value->key);
        cfg.filter.use_absolute_error = false;
    } else if (auto value = pickValue(raw, "filter", {"max_error", "maxerror"})) {
        cfg.filter.n_sigma = parseDouble(value->value, "Filter." + value->key);
        cfg.filter.use_absolute_error = false;
    }
    if (auto value = pickValue(raw, "filter", {"absolute_error", "absoluteerror"})) {
        cfg.filter.absolute_error = parseDouble(value->value, "Filter." + value->key);
        cfg.filter.use_absolute_error = true;
    }
    if (auto value = pickValue(raw, "filter", {"use_absolute_error"})) {
        cfg.filter.use_absolute_error = parseBool(value->value, "Filter." + value->key);
    }
    if (auto value = pickValue(raw, "filter", {"remove_isolated"})) {
        cfg.filter.remove_isolated = parseBool(value->value, "Filter." + value->key);
    }

    return cfg;
}

}  // namespace tsdf
