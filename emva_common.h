#ifndef EMVA_COMMON_H
#define EMVA_COMMON_H

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <map>
#include <numeric>
#include <cctype> 

namespace fs = std::filesystem;

struct Config {
    fs::path root_folder;
    int width;
    int height;
};

struct RawFile {
    fs::path path;
    double exposure_time_ms;
    bool operator<(const RawFile& other) const { return path < other.path; }
};

struct EMVA_Results {
    double K;
    double read_noise_e;
    double dark_current_e_ms;
    double sat_capacity_e;
    double dynamic_range;
    double SNR_max_dB;
    bool eta_calculated;
    std::vector<double> light_signals;
    std::vector<double> light_variances;
    std::vector<double> snr_curve_dB;

    double read_noise_DN;
    double dark_current_DN_ms;
    
    std::vector<double> dark_times_ms;
    std::vector<double> dark_means_DN;

    std::vector<double> total_noise_dn;
    double DR_val;

    std::vector<double> k_times_ms; 

    std::vector<double> pixel_stds; 
};

double extract_exposure_time_ms(const std::string& filename) {
    size_t pos = filename.find("ms");
    if (pos == std::string::npos) {
        return 0.0;
    }

    size_t start = pos;
    while (start > 0 && (isdigit(static_cast<unsigned char>(filename[start - 1])) || filename[start - 1] == '.')) {
        start--;
    }

    std::string time_str = filename.substr(start, pos - start);
    if (time_str.empty() || time_str == ".") {
        return 0.0;
    }

    try {
        return std::stod(time_str); 
    } catch (...) {
        return 0.0;
    }
}

std::vector<double> loadRawImage(const fs::path& filepath, const Config& cfg) {
    size_t pixel_count = cfg.width * cfg.height;
    std::vector<double> pixels(pixel_count, 0.0);
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "警告: 无法打开文件 " << filepath << std::endl;
        return pixels;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size != static_cast<std::streamsize>(pixel_count * 2)) return pixels;
    std::vector<uint16_t> buffer(pixel_count);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    for (size_t i = 0; i < pixel_count; ++i) pixels[i] = buffer[i];

    return pixels;
}

double calcMean(const std::vector<double>& img) {
    double sum = 0.0;
    for (double v : img) sum += v;
    return sum / img.size();
}

double calcPairVariance(const std::vector<double>& A, const std::vector<double>& B) {
    double sum = 0.0;
    for (size_t i = 0; i < A.size(); ++i) {
        double diff = A[i] - B[i];
        sum += diff * diff;
    }
    return sum / (2.0 * A.size());
}

struct RegressionResult { 
    double slope, intercept; 
};

RegressionResult linearRegression(const std::vector<double>& x, const std::vector<double>& y) {
    double n = static_cast<double>(x.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for (size_t i = 0; i < n; ++i) {
        sum_x += x[i]; 
        sum_y += y[i]; 
        sum_xy += x[i] * y[i]; 
        sum_xx += x[i] * x[i];
    }
    double denom = n * sum_xx - sum_x * sum_x;
    return { (n * sum_xy - sum_x * sum_y) / denom, (sum_y * sum_xx - sum_x * sum_xy) / denom };
}

#endif // EMVA_COMMON_H