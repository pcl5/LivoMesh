#include "params.h"

#include <CloudSamplingTools.h>
#include <CCGeom.h>
#include <CCTypes.h>
#include <DgmOctree.h>
#include <PointCloud.h>
#include <ReferenceCloud.h>

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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

struct FieldAttr {
    int offset = -1;
    int size = 4;
    char type = 'F';
};

struct PcdHeader {
    std::size_t pointCount = 0;
    std::size_t pointStep = 0;
    FieldAttr x;
    FieldAttr y;
    FieldAttr z;
};

PcdHeader parseBinaryHeader(std::istream& in) {
    PcdHeader header;
    std::vector<std::string> fields;
    std::vector<int> sizes;
    std::vector<char> types;
    std::vector<int> counts;
    bool dataFound = false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string current = trim(line);
        if (current.empty()) {
            continue;
        }
        if (current.rfind("DATA", 0) == 0) {
            if (normalize(trim(current.substr(4))) != "binary") {
                throw std::runtime_error("当前仅支持 DATA binary。");
            }
            dataFound = true;
            break;
        }

        std::istringstream iss(current);
        std::string token;
        iss >> token;
        token = normalize(token);

        if (token == "fields") {
            std::string name;
            while (iss >> name) {
                fields.push_back(name);
            }
        } else if (token == "size") {
            int s = 0;
            while (iss >> s) {
                sizes.push_back(s);
            }
        } else if (token == "type") {
            std::string type;
            while (iss >> type) {
                types.push_back(type.empty() ? 'F' : static_cast<char>(std::toupper(type.front())));
            }
        } else if (token == "count") {
            int c = 0;
            while (iss >> c) {
                counts.push_back(c);
            }
        } else if (token == "points") {
            iss >> header.pointCount;
        }
    }

    if (!dataFound) {
        throw std::runtime_error("PCD Header 缺少 DATA binary。");
    }
    if (fields.empty()) {
        throw std::runtime_error("PCD Header 缺少 FIELDS。");
    }
    if (sizes.size() != fields.size()) {
        sizes.assign(fields.size(), 4);
    }
    if (types.size() != fields.size()) {
        types.assign(fields.size(), 'F');
    }
    if (counts.size() != fields.size()) {
        counts.assign(fields.size(), 1);
    }

    std::size_t offset = 0;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::size_t bytes = static_cast<std::size_t>(sizes[i]) * static_cast<std::size_t>(counts[i]);
        const std::string key = normalize(fields[i]);
        if (counts[i] == 1) {
            if (key == "x") {
                header.x.offset = static_cast<int>(offset);
                header.x.size = sizes[i];
                header.x.type = types[i];
            } else if (key == "y") {
                header.y.offset = static_cast<int>(offset);
                header.y.size = sizes[i];
                header.y.type = types[i];
            } else if (key == "z") {
                header.z.offset = static_cast<int>(offset);
                header.z.size = sizes[i];
                header.z.type = types[i];
            }
        }
        offset += bytes;
    }

    if (header.x.offset < 0 || header.y.offset < 0 || header.z.offset < 0) {
        throw std::runtime_error("PCD 缺少 x/y/z 字段。");
    }

    header.pointStep = offset;
    return header;
}

double readScalar(const char* ptr, char type, int size) {
    if (type == 'F') {
        if (size == 4) {
            float v;
            std::memcpy(&v, ptr, sizeof(v));
            return v;
        }
        if (size == 8) {
            double v;
            std::memcpy(&v, ptr, sizeof(v));
            return v;
        }
    } else if (type == 'I') {
        if (size == 4) {
            std::int32_t v;
            std::memcpy(&v, ptr, sizeof(v));
            return static_cast<double>(v);
        }
    } else if (type == 'U') {
        if (size == 4) {
            std::uint32_t v;
            std::memcpy(&v, ptr, sizeof(v));
            return static_cast<double>(v);
        }
    }
    throw std::runtime_error("不支持的字段类型");
}

CCCoreLib::PointCloud loadBinaryCloud(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("无法打开点云文件: " + path.string());
    }

    const PcdHeader header = parseBinaryHeader(in);
    CCCoreLib::PointCloud cloud;
    if (!cloud.reserve(static_cast<unsigned>(header.pointCount))) {
        throw std::runtime_error("点云预分配失败");
    }

    std::vector<char> buffer(header.pointStep);
    for (std::size_t i = 0; i < header.pointCount; ++i) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (static_cast<std::size_t>(in.gcount()) != buffer.size()) {
            throw std::runtime_error("PCD 数据长度不足");
        }
        const double x = readScalar(buffer.data() + header.x.offset, header.x.type, header.x.size);
        const double y = readScalar(buffer.data() + header.y.offset, header.y.type, header.y.size);
        const double z = readScalar(buffer.data() + header.z.offset, header.z.type, header.z.size);
        const CCVector3 vec(
            static_cast<PointCoordinateType>(x),
            static_cast<PointCoordinateType>(y),
            static_cast<PointCoordinateType>(z));
        cloud.addPoint(vec);
    }
    return cloud;
}

void writeBinaryCloud(const fs::path& output, const CCCoreLib::ReferenceCloud& filtered) {
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("无法写出点云: " + output.string());
    }

    out << "# Filtered by livomesh noise filter\n";
    out << "VERSION 0.7\n";
    out << "FIELDS x y z\n";
    out << "SIZE 4 4 4\n";
    out << "TYPE F F F\n";
    out << "COUNT 1 1 1\n";
    out << "WIDTH " << filtered.size() << '\n';
    out << "HEIGHT 1\n";
    out << "VIEWPOINT 0 0 0 1 0 0 0\n";
    out << "POINTS " << filtered.size() << '\n';
    out << "DATA binary\n";

    for (unsigned i = 0; i < filtered.size(); ++i) {
        const CCVector3* pt = filtered.getPoint(i);
        const float coords[3] = {
            static_cast<float>(pt->x),
            static_cast<float>(pt->y),
            static_cast<float>(pt->z)};
        out.write(reinterpret_cast<const char*>(coords), sizeof(coords));
    }
}

std::unique_ptr<CCCoreLib::ReferenceCloud> runFilter(CCCoreLib::PointCloud& cloud, const tsdf::FilterConfig& cfg, double* octreeMs, double* filterMs) {
    const auto octreeStart = std::chrono::steady_clock::now();
    CCCoreLib::DgmOctree octree(&cloud);
    if (octree.build() <= 0) {
        throw std::runtime_error("构建八叉树失败");
    }
    const auto octreeEnd = std::chrono::steady_clock::now();

    const auto filterStart = std::chrono::steady_clock::now();
    CCCoreLib::ReferenceCloud* filtered = CCCoreLib::CloudSamplingTools::noiseFilter(
        &cloud,
        static_cast<PointCoordinateType>(cfg.radius),
        cfg.n_sigma,
        cfg.remove_isolated,
        false,
        6,
        cfg.use_absolute_error,
        cfg.absolute_error,
        &octree,
        nullptr);
    const auto filterEnd = std::chrono::steady_clock::now();

    if (!filtered) {
        throw std::runtime_error("CCCoreLib 噪声滤波失败");
    }
    if (octreeMs) {
        *octreeMs = std::chrono::duration<double, std::milli>(octreeEnd - octreeStart).count();
    }
    if (filterMs) {
        *filterMs = std::chrono::duration<double, std::milli>(filterEnd - filterStart).count();
    }
    return std::unique_ptr<CCCoreLib::ReferenceCloud>(filtered);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: livomesh_app <config.yaml>\n";
        return 1;
    }

    try {
        const tsdf::AppConfig cfg = tsdf::loadAppConfig(argv[1]);
        std::cout << "输入点云: " << cfg.base.depth_path << '\n';
        if (!cfg.filter.enable) {
            std::cout << "Filter.enable=false，跳过噪声滤波。\n";
            return 0;
        }
        std::cout << "滤波参数:"
                  << " radius=" << cfg.filter.radius
                  << " nSigma=" << cfg.filter.n_sigma
                  << " remove_isolated=" << (cfg.filter.remove_isolated ? "true" : "false")
                  << " use_absolute_error=" << (cfg.filter.use_absolute_error ? "true" : "false")
                  << " absolute_error=" << cfg.filter.absolute_error
                  << '\n';
        std::cout << "输出目录: " << cfg.base.output_dir << '\n';
        const auto loadStart = std::chrono::steady_clock::now();
        CCCoreLib::PointCloud cloud = loadBinaryCloud(cfg.base.depth_path);
        const auto loadEnd = std::chrono::steady_clock::now();
        std::cout << "载入点数: " << cloud.size()
                  << "  耗时: " << std::chrono::duration<double, std::milli>(loadEnd - loadStart).count()
                  << " ms\n";

        double octreeMs = 0.0;
        double filterMs = 0.0;
        const std::unique_ptr<CCCoreLib::ReferenceCloud> filtered = runFilter(cloud, cfg.filter, &octreeMs, &filterMs);
        std::cout << "保留点数: " << filtered->size()
                  << "  八叉树: " << octreeMs << " ms"
                  << "  滤波: " << filterMs << " ms\n";

        if (!cfg.base.save_pcd) {
            std::cout << "save_pcd_en=false，跳过写出步骤。\n";
            return 0;
        }

        const std::string defaultName = cfg.base.depth_path.stem().string() + "_denoised.pcd";
        fs::path output;
        if (!cfg.base.output_pcd_path.empty()) {
            std::string ext = cfg.base.output_pcd_path.extension().string();
            for (char& c : ext) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (!ext.empty() && ext == ".pcd") {
                output = cfg.base.output_pcd_path;
            } else {
                std::filesystem::create_directories(cfg.base.output_pcd_path);
                output = cfg.base.output_pcd_path / defaultName;
            }
        } else {
            output = cfg.base.output_dir / defaultName;
        }

        if (!output.parent_path().empty()) {
            std::filesystem::create_directories(output.parent_path());
        }
        writeBinaryCloud(output, *filtered);
        std::cout << "输出: " << output << '\n';

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "处理失败: " << ex.what() << '\n';
        return 1;
    }
}
