#ifndef EMVA_DARK_H
#define EMVA_DARK_H

#include "emva_common.h"

// 计算暗电流 (e/ms 和 DN/ms)
inline void calcDarkCurrent(const Config& cfg, double K, double& dark_current_e_ms, double& dark_current_DN_ms, std::vector<double>& out_times, std::vector<double>& out_means) {
    std::vector<RawFile> dark_files;
    fs::path dark_dir = cfg.root_folder / "TEST_RAW_FOR_DARK";
    if (fs::is_directory(dark_dir)) {
        for (const auto& entry : fs::directory_iterator(dark_dir)) {
            if (entry.path().extension() == ".raw" || entry.path().extension() == ".RAW") {
                dark_files.push_back({entry.path(), extract_exposure_time_ms(entry.path().filename().string())});
            }
        }
    }
    std::sort(dark_files.begin(), dark_files.end(), [](const RawFile& a, const RawFile& b) {
        return a.exposure_time_ms < b.exposure_time_ms;
    });

    std::vector<double> times_ms;
    std::vector<double> dark_means;
    for (const auto& f : dark_files) {
        auto img = loadRawImage(f.path, cfg);
        dark_means.push_back(calcMean(img));
        times_ms.push_back(f.exposure_time_ms); 
    }

    if (!times_ms.empty() && !dark_means.empty()) {
        RegressionResult res = linearRegression(times_ms, dark_means);
        double slope_DN_ms = res.slope; 
        
        dark_current_DN_ms = slope_DN_ms;         
        dark_current_e_ms = slope_DN_ms / K;      
    } else {
        dark_current_e_ms = 0.0;
        dark_current_DN_ms = 0.0;
    }
    out_times = times_ms;
    out_means = dark_means;
}

#endif // EMVA_DARK_H