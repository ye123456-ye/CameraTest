#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "emva1288_calculator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QLineSeries>
#include <QValueAxis>
#include <QLogValueAxis>
#include <QChart>
#include <QtMath>
#include <QHeaderView>
#include <QGraphicsDropShadowEffect>
#include <QScatterSeries>
#include <QGraphicsSimpleTextItem>
#include <QVBoxLayout>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    QGraphicsDropShadowEffect *shadow1 = new QGraphicsDropShadowEffect(this);
    shadow1->setBlurRadius(15); shadow1->setXOffset(0); shadow1->setYOffset(4);
    shadow1->setColor(QColor(0, 0, 0, 30)); ui->Frame_Input->setGraphicsEffect(shadow1);

    QGraphicsDropShadowEffect *shadow2 = new QGraphicsDropShadowEffect(this);
    shadow2->setBlurRadius(15); shadow2->setXOffset(0); shadow2->setYOffset(4);
    shadow2->setColor(QColor(0, 0, 0, 30)); ui->Frame_Results->setGraphicsEffect(shadow2);

    ui->combo_GainSelect->setCurrentIndex(0);

    ui->chartView_Photon->setRubberBand(QChartView::NoRubberBand);
    ui->chartView_Photon->setInteractive(false);
    ui->chartView_Dark->setRubberBand(QChartView::NoRubberBand);
    ui->chartView_Dark->setInteractive(false);

    QVBoxLayout *noiseLayout = qobject_cast<QVBoxLayout*>(ui->tab_Noise->layout());
    if (!noiseLayout) {
        noiseLayout = new QVBoxLayout(ui->tab_Noise);
    }
    if (ui->chartView_Noise) {
        noiseLayout->removeWidget(ui->chartView_Noise);
        delete ui->chartView_Noise;
        ui->chartView_Noise = nullptr; 
    }
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::on_btn_Browse_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择数据根目录");
    if (!dir.isEmpty()) ui->lineEdit_Path->setText(dir);
}

void MainWindow::on_btn_Calculate_clicked() {
    QString rootPath = ui->lineEdit_Path->text();
    if (rootPath.isEmpty()) { QMessageBox::warning(this, "提示", "请选择数据根目录！"); return; }

    Config cfg;
    cfg.root_folder = rootPath.toStdString();
    cfg.width = ui->spinBox_W->value();
    cfg.height = ui->spinBox_H->value();
    
    ui->tableWidget_Results->clear();
    ui->tableWidget_Results->setColumnCount(4);
    ui->tableWidget_Results->setHorizontalHeaderLabels({"参数", "HG 增益", "MG 增益", "LG 增益"});
    ui->tableWidget_Results->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_Results->setRowCount(0);

    allSignals.clear(); allVariances.clear(); allDarkTimes.clear(); allDarkMeans.clear(); allTotalNoise.clear(); allReadNoiseDN.clear(); allSignalTimes.clear();
    allPixelStds.clear();

    QMap<QString, QStringList> resultsMap;

    for (const auto& mode : {"HG", "MG", "LG"}) {
        Config temp = cfg;
        temp.root_folder = cfg.root_folder / mode;
        EMVA_Results results = processCameraData(temp);

        resultsMap["转换增益 K"].append(QString::number(results.K) + " DN/e-");
        resultsMap["读出噪声 (e-)"].append(QString::number(results.read_noise_e) + " e-");
        resultsMap["读出噪声 (DN)"].append(QString::number(results.read_noise_DN) + " DN");
        resultsMap["暗电流 (e/ms)"].append(QString::number(results.dark_current_e_ms) + " e/ms");
        resultsMap["暗电流 (DN/ms)"].append(QString::number(results.dark_current_DN_ms) + " DN/ms");
        resultsMap["动态范围"].append(QString::number(20 * log10(results.dynamic_range)) + " dB");
        resultsMap["最大信噪比"].append(QString::number(results.SNR_max_dB) + " dB");

        QVector<double> x, y, dt, dm, noise;
        QVector<double> times;
        for (double v : results.light_signals) x.append(v);
        for (double v : results.light_variances) y.append(v);
        for (double v : results.dark_times_ms) dt.append(v);
        for (double v : results.dark_means_DN) dm.append(v);
        for (double v : results.total_noise_dn) noise.append(v);
        for (double v : results.k_times_ms) times.append(v);

        allSignals[mode] = x; allVariances[mode] = y;
        allDarkTimes[mode] = dt; allDarkMeans[mode] = dm;
        allTotalNoise[mode] = noise; allReadNoiseDN[mode] = results.read_noise_DN;
        allSignalTimes[mode] = times;
        allPixelStds[mode] = QVector<double>(results.pixel_stds.begin(), results.pixel_stds.end());
    }

    QStringList params = {"转换增益 K", "读出噪声 (e-)", "读出噪声 (DN)", "暗电流 (e/ms)", "暗电流 (DN/ms)", "动态范围", "最大信噪比"};
    
    for (const QString& param : params) {
        if (!resultsMap.contains(param) || resultsMap[param].size() < 3) continue;

        int row = ui->tableWidget_Results->rowCount();
        ui->tableWidget_Results->insertRow(row);

        ui->tableWidget_Results->setItem(row, 0, new QTableWidgetItem(param));
        ui->tableWidget_Results->setItem(row, 1, new QTableWidgetItem(resultsMap[param].at(0)));
        ui->tableWidget_Results->setItem(row, 2, new QTableWidgetItem(resultsMap[param].at(1)));
        ui->tableWidget_Results->setItem(row, 3, new QTableWidgetItem(resultsMap[param].at(2)));
    }

    updateCharts();
}

void MainWindow::on_combo_GainSelect_currentIndexChanged(int index) {
    Q_UNUSED(index);
    QString current = ui->combo_GainSelect->currentText();
    if (allSignals.value(current).isEmpty() && allPixelStds.value(current).isEmpty()) {
        return; 
    }
    updateCharts();
}

void MainWindow::updateCharts() {
    QString current = ui->combo_GainSelect->currentText();
    QStringList activeModes = {current};

    drawPhotonTransferCurve(activeModes);
    drawDarkCurrentCurve(activeModes);
    drawNoiseCurve(activeModes);
}

void MainWindow::drawPhotonTransferCurve(const QStringList& modes) {
    QChart *chart = new QChart();
    chart->setTitle("转换增益曲线 (原始点 + 拟合直线)");

    QList<QColor> colors = {QColor(Qt::red), QColor(Qt::green), QColor(Qt::blue)};
    double cur_minX = 1e18, cur_maxX = 0, cur_minY = 1e18, cur_maxY = 0;

    for (int i = 0; i < modes.size(); ++i) {
        const QVector<double>& x = allSignals.value(modes[i]);
        const QVector<double>& y = allVariances.value(modes[i]);
        if (x.size() != y.size() || x.isEmpty()) continue;

        for (int j = 0; j < x.size(); ++j) {
            if (x[j] < cur_minX) cur_minX = x[j];
            if (x[j] > cur_maxX) cur_maxX = x[j];
            if (y[j] < cur_minY) cur_minY = y[j];
            if (y[j] > cur_maxY) cur_maxY = y[j];
        }
    }

    if (cur_maxX == 0 && cur_maxY == 0) { ui->chartView_Photon->setChart(new QChart()); return; }

    double x_range = cur_maxX - cur_minX;
    if (x_range <= 0) x_range = 1.0;
    double x_padding = x_range * 0.1;
    double raw_minX = std::max(0.0, cur_minX - x_padding);
    double raw_maxX = cur_maxX + x_padding;
    
    double x_step = (raw_maxX - raw_minX) / 6.0;
    double mag = std::pow(10, std::floor(std::log10(x_step)));
    double norm = x_step / mag;
    if (norm < 1.5) x_step = mag;
    else if (norm < 3.5) x_step = 2 * mag;
    else if (norm < 7.5) x_step = 5 * mag;
    else x_step = 10 * mag;
    
    double minX = std::max(0.0, std::floor(raw_minX / x_step) * x_step);
    double maxX = minX + x_step * 6;
    while (maxX < raw_maxX) {
        x_step *= 2;
        minX = std::max(0.0, std::floor(raw_minX / x_step) * x_step);
        maxX = minX + x_step * 6;
    }

    double y_range = cur_maxY - cur_minY;
    if (y_range <= 0) y_range = 1.0;
    double y_padding = y_range * 0.05; 
    double raw_minY = std::max(0.0, cur_minY - y_padding);
    double raw_maxY = cur_maxY + y_padding;

    double y_step = (raw_maxY - raw_minY) / 6.0;
    double y_mag = std::pow(10, std::floor(std::log10(y_step)));
    double y_norm = y_step / y_mag;
    if (y_norm < 1.5) y_step = y_mag;
    else if (y_norm < 2.5) y_step = 2 * y_mag;
    else if (y_norm < 3.5) y_step = 2.5 * y_mag; 
    else if (y_norm < 7.5) y_step = 5 * y_mag;
    else y_step = 10 * y_mag;

    if ((raw_maxY - raw_minY) / y_step < 4) y_step /= 2;

    double minY = std::max(0.0, std::floor(raw_minY / y_step) * y_step);
    double maxY = std::ceil(raw_maxY / y_step) * y_step;

    if ((maxY - raw_maxY) > y_step * 0.6) {
        y_step /= 2;
        minY = std::max(0.0, std::floor(raw_minY / y_step) * y_step);
        maxY = std::ceil(raw_maxY / y_step) * y_step;
    }
    
    if (minY == maxY) maxY += y_step;

    int ySegments = static_cast<int>(std::round((maxY - minY) / y_step));

    auto *axisX = new QValueAxis();
    auto *axisY = new QValueAxis();
    axisX->setTitleText("平均灰度值差 (μ_y - μ_y.dark) / DN");
    axisY->setTitleText("时域方差差 (σ²_y - σ²_y.dark) / DN²");
    axisX->setRange(minX, maxX);
    axisX->setTickCount(7);
    axisX->setLabelFormat("%.0f");

    axisY->setRange(minY, maxY);
    axisY->setTickCount(ySegments + 1);
    axisY->setLabelFormat("%.0f");

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    for (int i = 0; i < modes.size(); ++i) {
        const QVector<double>& x = allSignals.value(modes[i]);
        const QVector<double>& y = allVariances.value(modes[i]);
        const QVector<double>& times = allSignalTimes.value(modes[i]);
        if (x.size() != y.size() || x.isEmpty()) continue;

        QScatterSeries *scatter = new QScatterSeries();
        scatter->setName(modes[i] + " 原始点");
        scatter->setColor(Qt::blue);
        scatter->setBorderColor(Qt::black);
        scatter->setMarkerSize(10.0);

        std::vector<double> std_x, std_y;
        for (int j = 0; j < x.size(); ++j) {
            scatter->append(x[j], y[j]);
            std_x.push_back(x[j]);
            std_y.push_back(y[j]);
        }

        QLineSeries *fitLine = new QLineSeries();
        fitLine->setColor(colors[i % 3]);
        QPen pen(colors[i % 3], 3);
        pen.setCosmetic(true);
        fitLine->setPen(pen);

        if (std_x.size() >= 2) {
            RegressionResult res = linearRegression(std_x, std_y);
            double K = res.slope;
            fitLine->setName(modes[i] + " 拟合直线 (K=" + QString::number(K) + " DN/e-)");
            fitLine->append(cur_minX, res.slope * cur_minX + res.intercept);
            fitLine->append(cur_maxX, res.slope * cur_maxX + res.intercept);
        }

        chart->addSeries(scatter);
        chart->addSeries(fitLine);
        scatter->attachAxis(axisX);
        scatter->attachAxis(axisY);
        fitLine->attachAxis(axisX);
        fitLine->attachAxis(axisY);

        chart->legend()->setVisible(true);
        ui->chartView_Photon->setChart(chart);

        for (int j = 0; j < x.size(); ++j) {
            if (j < times.size()) {
                QString label = QString::number(times[j]) + "ms";
                QGraphicsSimpleTextItem *text = new QGraphicsSimpleTextItem(label, chart);
                text->setBrush(Qt::black);
                QFont font("Arial", 8);
                font.setBold(true);
                text->setFont(font);

                QPointF pos = chart->mapToPosition(QPointF(x[j], y[j]));
                text->setPos(pos.x() + 5, pos.y() + 2);
            }
        }
    }
}

void MainWindow::drawDarkCurrentCurve(const QStringList& modes) {
    QChart *chart = new QChart();
    chart->setTitle("暗电流曲线 (原始点 + 拟合直线)");

    QList<QColor> colors = {QColor(Qt::red), QColor(Qt::green), QColor(Qt::blue)};
    double cur_minX = 1e18, cur_maxX = 0, cur_minY = 1e18, cur_maxY = 0;

    for (int i = 0; i < modes.size(); ++i) {
        const QVector<double>& x = allDarkTimes.value(modes[i]);
        const QVector<double>& y = allDarkMeans.value(modes[i]);
        if (x.size() != y.size() || x.isEmpty()) continue;

        for (int j = 0; j < x.size(); ++j) {
            if (x[j] < cur_minX) cur_minX = x[j];
            if (x[j] > cur_maxX) cur_maxX = x[j];
            if (y[j] < cur_minY) cur_minY = y[j];
            if (y[j] > cur_maxY) cur_maxY = y[j];
        }
    }

    if (cur_maxX == 0 && cur_maxY == 0) { ui->chartView_Dark->setChart(new QChart()); return; }

    std::vector<double> niceSteps = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};

    auto findNiceStep = [&](double range) {
        double rawStep = range / 6.0; 
        for (double step : niceSteps) {
            if (step >= rawStep) return step;
        }
        double lastStep = niceSteps.back();
        while (lastStep < rawStep) lastStep *= 10;
        return lastStep;
    };

    double x_padding = (cur_maxX - cur_minX) * 0.1;
    double y_padding = (cur_maxY - cur_minY) * 0.1;
    double raw_minX = std::max(0.0, cur_minX - x_padding);
    double raw_maxX = cur_maxX + x_padding;
    double raw_minY = std::max(0.0, cur_minY - y_padding);
    double raw_maxY = cur_maxY + y_padding;

    double x_step = findNiceStep(raw_maxX - raw_minX);
    double y_step = findNiceStep(raw_maxY - raw_minY);

    while ((raw_maxX - raw_minX) / x_step > 8) x_step *= 2;
    while ((raw_maxY - raw_minY) / y_step > 8) y_step *= 2;

    double minX = std::max(0.0, std::floor(raw_minX / x_step) * x_step);
    double maxX = std::ceil(raw_maxX / x_step) * x_step;
    double minY = std::max(0.0, std::floor(raw_minY / y_step) * y_step);
    double maxY = std::ceil(raw_maxY / y_step) * y_step;

    if (maxX <= minX) maxX = minX + x_step;
    if (maxY <= minY) maxY = minY + y_step;

    int tickX = static_cast<int>((maxX - minX) / x_step) + 1;
    int tickY = static_cast<int>((maxY - minY) / y_step) + 1;

    auto *axisX = new QValueAxis();
    auto *axisY = new QValueAxis();
    axisX->setTitleText("曝光时间 (ms)");
    axisY->setTitleText("暗场均值 (DN)");
    axisX->setRange(minX, maxX);
    axisX->setTickCount(tickX);
    axisX->setLabelFormat("%.0f");
    axisY->setRange(minY, maxY);
    axisY->setTickCount(tickY);
    axisY->setLabelFormat("%.0f");
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    QVector<QRectF> placedRects;

    for (int i = 0; i < modes.size(); ++i) {
        const QVector<double>& x = allDarkTimes.value(modes[i]);
        const QVector<double>& y = allDarkMeans.value(modes[i]);
        if (x.size() != y.size() || x.isEmpty()) continue;

        QScatterSeries *scatter = new QScatterSeries();
        scatter->setName(modes[i] + " 原始点");
        scatter->setColor(Qt::blue);
        scatter->setBorderColor(Qt::black);
        scatter->setMarkerSize(10.0);

        std::vector<double> std_x, std_y;
        for (int j = 0; j < x.size(); ++j) {
            scatter->append(x[j], y[j]);
            std_x.push_back(x[j]);
            std_y.push_back(y[j]);
        }

        QLineSeries *fitLine = new QLineSeries();
        fitLine->setColor(colors[i % 3]);
        QPen pen(colors[i % 3], 3);
        pen.setCosmetic(true);
        fitLine->setPen(pen);

        if (std_x.size() >= 2) {
            RegressionResult res = linearRegression(std_x, std_y);
            double K = res.slope;
            fitLine->setName(modes[i] + " 拟合直线 (暗电流=" + QString::number(K) + " DN/ms)");
            fitLine->append(cur_minX, res.slope * cur_minX + res.intercept);
            fitLine->append(cur_maxX, res.slope * cur_maxX + res.intercept);
        }

        chart->addSeries(scatter);
        chart->addSeries(fitLine);
        scatter->attachAxis(axisX);
        scatter->attachAxis(axisY);
        fitLine->attachAxis(axisX);
        fitLine->attachAxis(axisY);
        chart->legend()->setVisible(true);
        ui->chartView_Dark->setChart(chart);

        for (int j = 0; j < x.size(); ++j) {
            QString label = QString::number(x[j]) + "ms";
            QGraphicsSimpleTextItem *text = new QGraphicsSimpleTextItem(label, chart);
            text->setBrush(Qt::black);
            QFont font("Arial", 8);
            font.setBold(true);
            text->setFont(font);

            QFontMetrics metrics(font);
            QRectF textRect = metrics.boundingRect(label);
            double textW = textRect.width();
            double textH = textRect.height();

            QPointF pos = chart->mapToPosition(QPointF(x[j], y[j]));

            QVector<QPointF> candidates = {
                {pos.x() + 5, pos.y() - textH / 2},
                {pos.x() - textW - 5, pos.y() - textH / 2},
                {pos.x() + 5, pos.y() - textH - 2},
                {pos.x() + 5, pos.y() + 2}
            };

            for (const auto& cand : candidates) {
                QRectF testRect(cand.x(), cand.y(), textW, textH);
                bool overlap = false;
                for (const auto& rect : placedRects) {
                    if (testRect.intersects(rect)) {
                        overlap = true;
                        break;
                    }
                }
                if (!overlap) {
                    text->setPos(cand);
                    placedRects.append(testRect);
                    break;
                }
            }
            if (text->pos() == QPointF()) {
                text->setPos(candidates[3]);
                placedRects.append(QRectF(candidates[3].x(), candidates[3].y(), textW, textH));
            }
        }
    }
}

void MainWindow::drawNoiseCurve(const QStringList& modes) {
    if (allPixelStds.value(modes.value(0)).isEmpty()) {
        return;
    }

    if (!m_customPlot) {
        m_customPlot = new QCustomPlot(ui->tab_Noise);
        QVBoxLayout *noiseLayout = qobject_cast<QVBoxLayout*>(ui->tab_Noise->layout());
        if (!noiseLayout) {
            noiseLayout = new QVBoxLayout(ui->tab_Noise);
        }
        noiseLayout->addWidget(m_customPlot);
    }

    m_customPlot->clearPlottables();
    m_customPlot->clearGraphs();
    m_customPlot->clearItems();
    if (m_customPlot->legend) {
        m_customPlot->legend->clear();
    }

    double cur_minX = 1e18, cur_maxX = 0, cur_maxY = 0;

    for (int i = 0; i < modes.size(); ++i) {
        const QVector<double>& stds = allPixelStds.value(modes[i]);
        if (stds.isEmpty()) continue;

        std::vector<double> sorted_data(stds.begin(), stds.end());
        std::sort(sorted_data.begin(), sorted_data.end());
        if (sorted_data.size() > 10) {
            cur_minX = std::min(cur_minX, sorted_data[(int)(sorted_data.size() * 0.01)]);
            cur_maxX = std::max(cur_maxX, sorted_data[(int)(sorted_data.size() * 0.99)]);
        } else {
            for (double v : stds) { if(v < cur_minX) cur_minX = v; if(v > cur_maxX) cur_maxX = v; }
        }
    }

    if (cur_minX == 1e18) { m_customPlot->replot(); return; }

    int binCount = 40;
    double binWidth = (cur_maxX - cur_minX) / binCount;
    if (binWidth <= 0) binWidth = 1.0;

    for (int i = 0; i < modes.size(); ++i) {
        const QVector<double>& stds = allPixelStds.value(modes[i]);
        if (stds.isEmpty()) continue;

        QVector<double> keys, values;
        for (int k = 0; k < binCount; ++k) {
            double center = cur_minX + (k + 0.5) * binWidth;
            int count = 0;
            for (double val : stds) {
                if (val >= cur_minX + k * binWidth && val < cur_minX + (k + 1) * binWidth) count++;
            }
            keys.append(center);
            values.append(count);
            cur_maxY = qMax(cur_maxY, (double)count);
        }

        QCPBars *bars = new QCPBars(m_customPlot->xAxis, m_customPlot->yAxis);
        bars->setData(keys, values);
        bars->setWidth(binWidth); 
        bars->setName(modes[i]); 
        bars->setPen(QPen(QColor(0, 0, 0, 100), 1));
        bars->setBrush(QColor(135, 206, 235, 150));
    }

    if (!modes.isEmpty()) {
        double read_noise = allReadNoiseDN.value(modes[0]);
        QCPGraph *noiseLine = m_customPlot->addGraph();
        noiseLine->setName(QString("读出噪声: %1 DN").arg(read_noise));
        noiseLine->setPen(QPen(Qt::blue, 2, Qt::DashLine));
        QVector<double> x = {read_noise, read_noise};
        QVector<double> y = {0, cur_maxY * 1.1};
        noiseLine->setData(x, y);
    }

    m_customPlot->xAxis->setLabel("像素点时域标准差 (DN)");
    m_customPlot->yAxis->setLabel("像素点数量");
    m_customPlot->xAxis->setRange(0, cur_maxX * 1.1);
    m_customPlot->yAxis->setRange(0, cur_maxY * 1.1);

    m_customPlot->legend->setVisible(true);
    m_customPlot->replot();
}


