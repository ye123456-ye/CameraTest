#ifndef EMVA_GAIN_H
#define EMVA_GAIN_H

#include "emva_common.h"
inline void calcGain(const std::vector<double>& sigValues, const std::vector<double>& variances, double& K) {
    RegressionResult res = linearRegression(sigValues, variances);
    K = res.slope;
}

#endif // EMVA_GAIN_H