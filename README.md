# CameraTest — EMVA 1288 相机性能参数测试分析系统

基于 **EMVA 1288** 国际标准，对图像传感器进行完整的性能参数测试与可视化分析。支持高 / 中 / 低（HG / MG / LG）三档增益模式，可一键计算转换增益、暗电流、读出噪声、饱和容量、动态范围和最大信噪比等核心指标，并以交互图表呈现光子转移曲线、暗电流曲线和读出噪声直方图。

---

## 目录

- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [项目结构](#项目结构)
- [EMVA 1288 测试参数说明](#emva-1288-测试参数说明)
- [测试数据结构](#测试数据结构)
- [环境要求](#环境要求)
- [编译运行](#编译运行)
- [使用方法](#使用方法)
- [演示视频](#演示视频)
- [已知问题与 TODO](#已知问题与-todo)
- [License](#license)

---

## 功能特性

- **三档增益分析**：支持 HG（高增益）/ MG（中增益）/ LG（低增益）三种模式独立计算与切换查看。
- **核心参数计算**：
  - 转换增益 K（DN / e⁻）
  - 读出噪声（e⁻ 与 DN）
  - 暗电流（e⁻/ms 与 DN/ms）
  - 饱和容量（e⁻）
  - 动态范围（dB）
  - 最大信噪比（dB）
- **可视化图表**：
  - 光子转移曲线（Variance vs Signal，含线性拟合）
  - 暗电流曲线（Dark Mean vs Exposure Time，含线性拟合）
  - 读出噪声直方图（像素时域标准差分布，含 RMS 读出噪声参考线）
- **结果汇总表格**：三种增益模式的全部参数并排展示，便于对比。
- **自适应坐标轴**：X / Y 轴自动对齐到 1 / 2 / 5 量化步长，曲线与刻度美观对齐。
- **曝光时间标注**：曲线上每个数据点自动标注对应曝光时间，且带碰撞检测自动避让。

---

## 技术栈

| 类别 | 技术 |
| --- | --- |
| 语言 | C++17 |
| 框架 | Qt 6.11（Widgets + Charts + Gui + PrintSupport） |
| 绘图库 | QCustomPlot 2.x（读出噪声直方图） + Qt Charts（曲线） |
| 构建 | CMake 3.16+ |
| 编译器 | MSVC 2022 / 2026（`msvc2022_64`） |
| 系统 | Windows 10 / 11 |

---

## 项目结构

```
CameraTest/
├── main.cpp                      # 程序入口 + 全局 QSS 样式
├── MainWindow.h / .cpp           # 主窗口：事件处理、参数收集、结果表格、绘图
├── MainWindow.ui                 # Qt Designer 界面布局
│
├── emva_common.h                 # 公共数据结构与工具函数
│   ├─ Config / RawFile / EMVA_Results
│   ├─ loadRawImage()             # 读取 16-bit RAW
│   ├─ calcMean() / calcPairVariance()
│   ├─ linearRegression()         # 最小二乘
│   └─ extract_exposure_time_ms() # 从文件名解析曝光时间
│
├── emva_gain.h                   # 转换增益 K（光子转移曲线斜率）
├── emva_noise.h                  # 读出噪声（NOISE 帧像素时域 RMS）
├── emva_dark.h                   # 暗电流（DARK 帧均值 vs 曝光时间斜率）
├── emva_snr_dr.h                 # 信噪比 / 动态范围 / 饱和容量
├── emva1288_calculator.h         # 主流程 processCameraData()
│
├── qcustomplot.cpp / .h          # 第三方绘图库（直方图）
├── CMakeLists.txt                # CMake 构建脚本（含 windeployqt 部署）
├── .gitignore
│
└── test_data/                    # 随仓库附带的示例测试数据
    ├── HG/                       # 高增益模式
    ├── MG/                       # 中增益模式
    └── LG/                       # 低增益模式
```

### 关键模块职责

| 文件 | 职责 |
| --- | --- |
| `emva_common.h` | 定义 `Config`（根目录 / 宽 / 高）、`RawFile`、`EMVA_Results` 数据结构，以及 RAW 读取、统计、回归等通用函数 |
| `emva_gain.h` | `calcGain()`：对光子转移曲线（方差 vs 信号）做线性回归，斜率即为转换增益 K |
| `emva_noise.h` | `calcReadNoise()`：对 NOISE 目录多帧做逐像素时域标准差，全局均方根即为读出噪声 |
| `emva_dark.h` | `calcDarkCurrent()`：对 DARK 帧均值 vs 曝光时间做线性回归，斜率除以 K 即为暗电流 |
| `emva_snr_dr.h` | `calcSNRandDR()`：遍历 SNR 目录亮暗帧对，计算实测 SNR、最大信号、最小暗噪声，进而得到 SNR_max 与动态范围 |
| `emva1288_calculator.h` | `processCameraData()`：按顺序串联 K → 读出噪声 → 暗电流 → SNR/DR，并输出完整结果结构体 |
| `MainWindow.cpp` | 负责 UI 交互、三模式批量计算、结果表格填充、三类图表的绘制 |

---

## EMVA 1288 测试参数说明

EMVA 1288 是欧洲机器视觉协会（European Machine Vision Association）制定的图像传感器性能测试标准。本项目实现了其中的核心参数：

| 参数 | 符号 | 单位 | 物理意义 | 计算方法 |
| --- | --- | --- | --- | --- |
| 转换增益 | K | DN / e⁻ | 每电子产生的数字量 | 光子转移曲线（σ² = K·μ + σ_d²）斜率 |
| 读出噪声 | σ_d | e⁻ | 放大器等效输入噪声 | NOISE 帧像素时域 RMS / K |
| 暗电流 | I_dark | e⁻/s | 无光照时每秒积累的热生电荷 | 暗场均值 vs 曝光时间斜率 / K |
| 饱和容量 | N_sat | e⁻ | 满阱电子数 | 最大光子转移信号点 / K |
| 动态范围 | DR | dB | 饱和信号与噪声底之比 | 20·log₁₀(N_sat / √(σ_d² + σ_q²)) |
| 最大信噪比 | SNR_max | dB | 饱和处的信噪比 | 20·log₁₀(N_sat / √(σ_d² + σ_q² + N_sat)) |

> **量化噪声** σ_q = 1/√12 DN（均匀量化假设）。

---

## 测试数据结构

`test_data/` 目录下按增益模式组织，每个模式包含五类测试数据。**所有 RAW 文件为 16-bit 小端格式，分辨率 640×512，单文件 655360 字节。**

```
test_data/{HG,MG,LG}/
├── TEST_RAW_FOR_DARK/        # 暗电流测试
│   └── 20 帧 · 曝光时间 10 ~ 200 ms（每帧一个曝光时间）
│
├── TEST_RAW_FOR_K/           # 转换增益测试
│   ├── dark/   20 帧 · 曝光时间 1 ~ 10 ms（每曝光时间 2 帧）
│   └── data/   20 帧 · 曝光时间 1 ~ 10 ms（每曝光时间 2 帧）
│
├── TEST_RAW_FOR_NOISE/       # 读出噪声测试
│   └── 21 帧 · 曝光时间 0.2 ms（恒定短曝光）
│
└── TEST_RAW_FOR_SNR/         # 信噪比 / 动态范围测试
    ├── dark/   60 帧 · 曝光时间 0.1 ~ 9.9 ms（每曝光时间 2 帧）
    └── data/   60 帧 · 曝光时间 0.1 ~ 9.9 ms（每曝光时间 2 帧）
```

### 文件名格式

```
ITR_{HiGain|MdGain|LwGain}_{曝光时间}msPI_{曝光时间}msRI_n{模拟增益}c_T{时间戳}.raw
```

- `msPI`：曝光积分时间（ms）
- `msRI`：读出时间（ms）
- 程序通过文件名中首个 `ms` 前缀的数值提取曝光时间。

### 每个增益模式文件汇总

| 目录 | 文件数 |
| --- | --- |
| TEST_RAW_FOR_DARK | 20 |
| TEST_RAW_FOR_K/dark | 20 |
| TEST_RAW_FOR_K/data | 20 |
| TEST_RAW_FOR_NOISE | 21 |
| TEST_RAW_FOR_SNR/dark | 60 |
| TEST_RAW_FOR_SNR/data | 60 |

---

## 环境要求

- **操作系统**：Windows 10 / 11
- **Qt**：6.11.1（msvc2022_64 套件），安装到 `D:/Qt/6.11.1/msvc2022_64`
  - 如需安装到其他路径，请修改 `CMakeLists.txt` 中 `CMAKE_PREFIX_PATH` 与 `windeployqt` 命令路径
- **编译器**：MSVC 2022 或更新（随 Visual Studio 安装）
- **CMake**：3.16 或更高

> 注：项目当前硬编码了 Qt 路径 `D:/Qt/6.11.1/msvc2022_64`。如果你使用其他版本或路径，需同步修改 `CMakeLists.txt` 第 7 行和第 41 行。

---

## 编译运行

### 1. 配置 CMake

以 Visual Studio 自带 CMake 为例：

```powershell
# 进入项目目录
cd E:\code\code\Cpp\CameraTest

# 配置（生成器选 VS 2022）
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
```

### 2. 编译

```powershell
cmake --build build --config Debug
```

编译成功后，可执行文件位于：

```
build/Debug/CameraTest_Qt.exe
```

`CMakeLists.txt` 中已配置 `POST_BUILD` 钩子，编译完成后会自动调用 `windeployqt.exe`，将所需 Qt DLL 与插件部署到 exe 同目录，无需手动复制。

### 3. 运行

直接双击 `CameraTest_Qt.exe`，或在终端运行：

```powershell
.\build\Debug\CameraTest_Qt.exe
```

---

## 使用方法

1. **选择数据根目录**：点击「浏览…」，选中 `test_data` 目录（其下包含 HG / MG / LG 三个子目录）。
2. **设置图像尺寸**：默认 640 × 512，与附带的测试数据匹配；若使用自定义数据，请改为对应宽高。
3. **选择查看增益**：右上角下拉框切换 HG / MG / LG。
4. **开始计算**：点击「开始计算」，程序会依次处理三种增益模式：
   - 结果表格实时刷新，列出所有参数
   - 三个选项卡分别绘制光子转移曲线、暗电流曲线、读出噪声直方图
5. **查看图表**：
   - 「转换增益」选项卡：显示方差-信号散点与拟合直线，斜率即 K
   - 「暗电流」选项卡：显示暗场均值-曝光时间散点与拟合直线
   - 「读出噪声」选项卡：像素时域标准差直方图，蓝色虚线为 RMS 读出噪声

---

## 演示视频

完整功能演示视频已发布在 GitHub Releases：

👉 **[CameraTest v1.0.0 演示视频](https://github.com/ye123456-ye/CameraTest/releases)**

视频内容包括：
- 数据目录选择与参数计算流程
- 三档增益（HG / MG / LG）切换与结果对比
- 三类图表的实时渲染效果

---

## 已知问题与 TODO

- [ ] **暗电流温度漂移**：当传感器在 DARK 数据采集期间未完全热稳定时，暗场均值可能随时间下降，导致暗电流斜率为负。计划增加 fallback：当 DARK 斜率为负时，改用 `TEST_RAW_FOR_K/dark` 的暗场均值重新计算。
- [ ] **动态范围量化噪声项**：当前动态范围公式尚未加入量化噪声 σ_q 项，计划按 EMVA 1288 标准补齐。
- [ ] **饱和容量选取**：当前取最大光子转移信号点作为饱和点，严格 EMVA 1288 应取方差最大点，待修正。
- [ ] **内存优化**：RAW 图像以 `double` 加载，占用较大；计划改为 `uint16_t` 按需转换，并加入图像缓存。
- [ ] **异步计算**：当前计算在主线程同步执行，数据量大时 UI 会短暂卡顿；计划改用 `QtConcurrent` 并加进度条。
- [ ] **结果导出**：计划支持将参数表格与图表导出为 CSV / PNG。
- [ ] **额外 EMVA 参数**：量子效率 η、DSNU、PRNU、线性度等参数尚未实现。
- [ ] **窗口自适应**：当前主窗口固定为 1323×800，计划改为可调整大小。

---

## License

本项目仅供学习与相机性能测试使用。`qcustomplot` 请遵守其自身开源协议。
