#include "mainwindow.h"
#include <QApplication>
#include <windows.h>

int main(int argc, char *argv[])
{
    SetConsoleOutputCP(65001);
    QApplication a(argc, argv);

    // 全局美化样式
    a.setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #f5f7fa;
            font-family: "Microsoft YaHei";
            font-size: 13px;
            color: #333333;
        }
        
        /* 卡片式背景 */
        QFrame#Card {
            background-color: #ffffff;
            border-radius: 10px;
            border: 1px solid #e0e0e0;
        }

        /* 标签 */
        QLabel {
            color: #555555;
        }

        /* 输入框美化 */
        QLineEdit, QSpinBox, QComboBox {
            background-color: #ffffff;
            border: 1px solid #d0d0d0;
            border-radius: 5px;
            padding: 5px 8px;
            min-height: 24px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border: 1px solid #3498db;
        }

        /* 按钮美化 */
        QPushButton {
            background-color: #3498db;
            border: none;
            border-radius: 5px;
            color: white;
            padding: 7px 15px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #1f618d;
        }

        /* 选项卡美化 */
        QTabWidget::pane {
            border: 1px solid #e0e0e0;
            background: white;
            border-radius: 6px;
            top: -1px;
        }
        QTabBar::tab {
            background: #e0e0e0;
            border: none;
            padding: 8px 20px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 4px;
            color: #333;
        }
        QTabBar::tab:selected {
            background: #ffffff;
            border-bottom: 2px solid #3498db;
            color: #2c3e50;
        }

        /* 表格美化 */
        QTableWidget {
            background-color: white;
            border: 1px solid #e0e0e0;
            gridline-color: #f0f0f0;
            border-radius: 6px;
        }
        QHeaderView::section {
            background-color: #f8f9fa;
            border: none;
            border-bottom: 1px solid #e0e0e0;
            padding: 6px;
            font-weight: bold;
            color: #333;
        }
    )");

    MainWindow w;
    w.show();
    return a.exec();
}

