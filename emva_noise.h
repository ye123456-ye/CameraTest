#ifndef EMVA_NOISE_H
#define EMVA_NOISE_H

#include "emva_common.h"

inline void calcReadNoise(const Config& cfg, double K, double& read_noise_e, double& read_noise_DN, std::vector<double>& out_pixel_stds) {
    std::vector<RawFile> noise_files;
    fs::path noise_dir = cfg.root_folder / "TEST_RAW_FOR_NOISE";
    // if (fs::exists(noise_dir)) {
    if (fs::is_directory(noise_dir)) {
        for (const auto& entry : fs::directory_iterator(noise_dir)) {
            if (entry.path().extension() == ".raw" || entry.path().extension() == ".RAW") {
                noise_files.push_back({entry.path(), 0.0});
            }
        }
    }
    std::sort(noise_files.begin(), noise_files.end());

    if (noise_files.size() > 1) {
        size_t L = noise_files.size();
        size_t pixel_count = cfg.width * cfg.height;
        std::vector<std::vector<double>> all_frames;
        all_frames.reserve(L);
        for (const auto& f : noise_files) {
            all_frames.push_back(loadRawImage(f.path, cfg));
        }
        
        double total_var_sum = 0.0;
        out_pixel_stds.clear();
        out_pixel_stds.reserve(pixel_count);

        for (size_t px = 0; px < pixel_count; ++px) {
            double px_mean = 0.0;
            for (size_t k = 0; k < L; ++k) {
                px_mean += all_frames[k][px];
            }
            px_mean /= (double)L;
            
            double px_var = 0.0;
            for (size_t k = 0; k < L; ++k) {
                double diff = all_frames[k][px] - px_mean;
                px_var += diff * diff;
            }
            px_var /= (double)L;
            total_var_sum += px_var;

            out_pixel_stds.push_back(std::sqrt(px_var));
        }

        double global_var_avg = total_var_sum / (double)pixel_count;
        double avg_rms_dn = std::sqrt(global_var_avg);
        
        read_noise_DN = avg_rms_dn;   
        read_noise_e = avg_rms_dn / K; 
        
    } else {
        read_noise_e = 0.0;
        read_noise_DN = 0.0;
        out_pixel_stds.clear();
        std::cerr << "警告: NOISE 文件夹帧数不足，无法计算读出噪声！" << std::endl;
    }
}

#endif // EMVA_NOISE_H