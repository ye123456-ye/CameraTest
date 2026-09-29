#ifndef EMVA_SNR_DR_H
#define EMVA_SNR_DR_H

#include "emva_common.h"

inline void calcSNRandDR(const Config& cfg, double K, double& dynamic_range, double& SNR_max_dB, 
                         std::vector<double>& outSignals, std::vector<double>& outSNR, std::vector<double>& outVariances, std::vector<double>& outTotalNoise) {
    std::vector<RawFile> snr_dark_files;
    std::vector<RawFile> snr_data_files;
    fs::path snr_dir = cfg.root_folder / "TEST_RAW_FOR_SNR";
    fs::path snr_dark_dir = snr_dir / "dark";
    fs::path snr_data_dir = snr_dir / "data";

    if (fs::is_directory(snr_dark_dir)) {
        for (const auto& entry : fs::directory_iterator(snr_dark_dir)) {
            if (entry.path().extension() == ".raw" || entry.path().extension() == ".RAW") {
                snr_dark_files.push_back({entry.path(), extract_exposure_time_ms(entry.path().filename().string())});
            }
        }
    }

    if (fs::is_directory(snr_data_dir)) {
        for (const auto& entry : fs::directory_iterator(snr_data_dir)) {
            if (entry.path().extension() == ".raw" || entry.path().extension() == ".RAW") {
                snr_data_files.push_back({entry.path(), extract_exposure_time_ms(entry.path().filename().string())});
            }
        }
    }

    std::map<double, std::vector<RawFile>> snr_dark_map;
    for (const auto& f : snr_dark_files) {
        snr_dark_map[f.exposure_time_ms].push_back(f);
    }
    std::sort(snr_data_files.begin(), snr_data_files.end(), [](const RawFile& a, const RawFile& b) {
        return a.exposure_time_ms < b.exposure_time_ms;
    });

    std::vector<double> snr_curve_dB_measured;
    std::vector<double> snr_signals_dn;
    std::vector<double> snr_variances_dn; 
    std::vector<double> total_noise_dn;

    double min_dark_noise_dn = 1e18;
    double max_signal_dn = 0.0;

    double sigma_q = 1.0 / std::sqrt(12.0); 
    double sigma_q_e = sigma_q / K;          

    for (const auto& [exp_time, files] : snr_dark_map) {
        if (files.size() < 2) continue;
        auto D_A = loadRawImage(files[0].path, cfg);
        auto D_B = loadRawImage(files[1].path, cfg);
        double var_dark = calcPairVariance(D_A, D_B);
        double current_dark_noise_dn = std::sqrt(var_dark);
        if (current_dark_noise_dn < min_dark_noise_dn) {
            min_dark_noise_dn = current_dark_noise_dn;
        }
    }
    double sigma_d_e = min_dark_noise_dn / K;

    for (size_t i = 0; i + 1 < snr_data_files.size(); i += 2) {
        double exp_time = snr_data_files[i].exposure_time_ms;
        
        if (snr_dark_map.find(exp_time) == snr_dark_map.end() || snr_dark_map[exp_time].size() < 2) continue;
        
        auto L_A = loadRawImage(snr_data_files[i].path, cfg);
        auto L_B = loadRawImage(snr_data_files[i+1].path, cfg);
        double mu_y = (calcMean(L_A) + calcMean(L_B)) / 2.0;
        double var_y = calcPairVariance(L_A, L_B);
        
        auto D_A = loadRawImage(snr_dark_map[exp_time][0].path, cfg);
        auto D_B = loadRawImage(snr_dark_map[exp_time][1].path, cfg);
        double mu_dark = (calcMean(D_A) + calcMean(D_B)) / 2.0;
        double var_dark = calcPairVariance(D_A, D_B);
        
        double pure_signal_dn = mu_y - mu_dark; 
        double signal_e = pure_signal_dn / K;    

        double total_var_e = sigma_d_e * sigma_d_e + sigma_q_e * sigma_q_e + signal_e;
        double snr_point = signal_e / std::sqrt(total_var_e);
        double snr_point_dB = 20.0 * std::log10(snr_point);             //信噪比
        
        snr_signals_dn.push_back(pure_signal_dn);
        snr_variances_dn.push_back(var_y - var_dark);
        total_noise_dn.push_back(std::sqrt(total_var_e));
        snr_curve_dB_measured.push_back(snr_point_dB);
        
        if (pure_signal_dn > max_signal_dn) max_signal_dn = pure_signal_dn;
    }

    if (!snr_curve_dB_measured.empty()) {
        double max_snr_found = *std::max_element(snr_curve_dB_measured.begin(), snr_curve_dB_measured.end());
        SNR_max_dB = max_snr_found;

        dynamic_range = (max_signal_dn / K) / std::sqrt(sigma_d_e * sigma_d_e + sigma_q_e * sigma_q_e);     //动态范围

        outSignals = snr_signals_dn;
        outSNR = snr_curve_dB_measured;
        outVariances = snr_variances_dn;
        outTotalNoise = total_noise_dn;
    } else {
        dynamic_range = 0;
        SNR_max_dB = 0;
        std::cerr << "警告: SNR 文件夹未找到或数据为空！" << std::endl;
    }
}

#endif // EMVA_SNR_DR_H