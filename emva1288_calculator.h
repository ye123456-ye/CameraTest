#ifndef EMVA1288_CALCULATOR_H
#define EMVA1288_CALCULATOR_H

#include "emva_common.h"
#include "emva_gain.h"
#include "emva_noise.h"
#include "emva_dark.h"
#include "emva_snr_dr.h"

inline EMVA_Results processCameraData(const Config& cfg) {
    EMVA_Results res = {};
    res.eta_calculated = false;

    std::vector<RawFile> k_dark_files;
    std::vector<RawFile> k_data_files;
    fs::path k_dir = cfg.root_folder / "TEST_RAW_FOR_K";
    fs::path k_dark_dir = k_dir / "dark";
    fs::path k_data_dir = k_dir / "data";

    if (fs::is_directory(k_dark_dir)) {
        for (const auto& entry : fs::directory_iterator(k_dark_dir)) {
            if (entry.path().extension() == ".raw" || entry.path().extension() == ".RAW") {
                k_dark_files.push_back({entry.path(), extract_exposure_time_ms(entry.path().filename().string())});
            }
        }
    }
    std::map<double, std::vector<RawFile>> dark_map;
    for (const auto& f : k_dark_files) {
        dark_map[f.exposure_time_ms].push_back(f);
    }

    std::map<double, double> dark_mean_map;
    std::map<double, double> dark_var_map; 
    double sum_dark_var = 0.0;
    int count_dark_var = 0;
    for (const auto& [t, files] : dark_map) {
        if (files.size() >= 2) {
            auto imgA = loadRawImage(files[0].path, cfg);
            auto imgB = loadRawImage(files[1].path, cfg);
            dark_mean_map[t] = (calcMean(imgA) + calcMean(imgB)) / 2.0;
            double var = calcPairVariance(imgA, imgB);
            dark_var_map[t] = var;
            sum_dark_var += var;
            count_dark_var++;
        }
    }

    if (fs::is_directory(k_data_dir)) {
        for (const auto& entry : fs::directory_iterator(k_data_dir)) {
            if (entry.path().extension() == ".raw" || entry.path().extension() == ".RAW") {
                k_data_files.push_back({entry.path(), extract_exposure_time_ms(entry.path().filename().string())});
            }
        }
    }
    std::sort(k_data_files.begin(), k_data_files.end(), [](const RawFile& a, const RawFile& b) {
        return a.exposure_time_ms < b.exposure_time_ms;
    });

    std::vector<double> light_signals;
    std::vector<double> light_variances;

    for (size_t i = 0; i + 1 < k_data_files.size(); i += 2) {
        auto imgA = loadRawImage(k_data_files[i].path, cfg);
        auto imgB = loadRawImage(k_data_files[i + 1].path, cfg);
        double mean_y = (calcMean(imgA) + calcMean(imgB)) / 2.0;
        double var_y = calcPairVariance(imgA, imgB);
        double exp_time = k_data_files[i].exposure_time_ms;
        double dark_mean = (dark_mean_map.count(exp_time) > 0) ? dark_mean_map[exp_time] : 0.0;
        double dark_var  = (dark_var_map.count(exp_time) > 0) ? dark_var_map[exp_time] : 0.0;

        light_signals.push_back(mean_y - dark_mean);
        light_variances.push_back(var_y - dark_var); 

        res.k_times_ms.push_back(exp_time);
    }

    // 计算 K
    calcGain(light_signals, light_variances, res.K);
    if (res.K <= 0) res.K = 1.0;

    // 2. 计算读出噪声
    calcReadNoise(cfg, res.K, res.read_noise_e, res.read_noise_DN, res.pixel_stds);
    
    // 3. 计算暗电流（单位：毫秒）
    calcDarkCurrent(cfg, res.K, res.dark_current_e_ms, res.dark_current_DN_ms, 
                    res.dark_times_ms, res.dark_means_DN);
    
    // 4. 计算 SNR 和 DR
    std::vector<double> snrSignals, snrCurve, snrVars, snrTotalNoise;
    calcSNRandDR(cfg, res.K, res.dynamic_range, res.SNR_max_dB, snrSignals, snrCurve, snrVars, snrTotalNoise);

    res.light_signals = light_signals;
    res.light_variances = light_variances;

    res.snr_curve_dB = snrCurve;
    res.total_noise_dn = snrTotalNoise;

    // 6. 保存 SNR 曲线
    res.snr_curve_dB = snrCurve;
    if (res.snr_curve_dB.empty()) {
        res.light_signals = light_signals;
        res.light_variances = light_variances;
        double quant_noise_e_sq = 1.0 / (12.0 * res.K * res.K);
        res.snr_curve_dB.clear();
        for (size_t i = 0; i < light_signals.size(); ++i) {
            double signal_e = light_signals[i] / res.K;
            double snr = signal_e / std::sqrt(res.read_noise_e * res.read_noise_e + quant_noise_e_sq + signal_e);
            res.snr_curve_dB.push_back(20.0 * std::log10(snr));
        }
    }

    // 7. 计算饱和容量
    if (!res.light_signals.empty()) {
        res.sat_capacity_e = *std::max_element(res.light_signals.begin(), res.light_signals.end()) / res.K;
    }

    res.DR_val = res.dynamic_range;

    return res;
}

#endif // EMVA1288_CALCULATOR_H