#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QMap>
#include "qcustomplot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btn_Browse_clicked();
    void on_btn_Calculate_clicked();
    void on_combo_GainSelect_currentIndexChanged(int index);

private:
    QMap<QString, QVector<double>> allSignals, allVariances, allDarkTimes, allDarkMeans, allTotalNoise;
    QMap<QString, double> allReadNoiseDN;   
    QMap<QString, QVector<double>> allSignalTimes;
    QMap<QString, QVector<double>> allPixelStds; 

    void drawPhotonTransferCurve(const QStringList& modes);
    void drawDarkCurrentCurve(const QStringList& modes);
    void drawNoiseCurve(const QStringList& modes);
    void updateCharts();

    Ui::MainWindow *ui;
    QCustomPlot *m_customPlot = nullptr;
};

#endif // MAINWINDOW_H