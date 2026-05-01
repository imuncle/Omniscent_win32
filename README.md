# Omniscent — Win32 复刻版

Omniscent原作是 1997 年 **SANCTION** 制作的 4095 字节 demoscene intro（4k intro），现在只能在Dosbox里运行了。后来[SuperSodaSead](https://github.com/SuperSodaSea/Omniscent)大佬以 WebGL + Typescript 重新实现。本项目在此基础上进行了 Win32 移植，使用纯 C 语言编写，以 MSVC 编译、Crinkler 链接。

可在[Release](https://github.com/imuncle/Omniscent_win32/releases)页面下载编译后的程序。

## 两个版本

| 版本 | 源文件 | 渲染方式 | 可执行文件大小 |
|------|--------|----------|----------------|
| **纯软光栅版** | `omniscent_all.c` | CPU 扫描线光栅化 | 5168 字节（Crinkler 压缩） |
| **双渲染版** | `src/` 下多个文件 | 软渲染 + OpenGL 硬渲染（F1 切换） | 7253 字节（Crinkler 压缩） |

纯软光栅版将所有代码合并为单个文件，Crinkler压缩后为 5168 字节（俺已经尽力了，由于 Windows PE 格式的额外开销以及使用 C 语言而非汇编，压缩至4KB以内有点难度，期待大神出手）

## 特性

- **双渲染模式** — 软渲染（CPU 扫描线多边形填充）和 OpenGL 2.0 硬渲染（GLSL 120 着色器），运行时按 **F1** 切换，视觉输出完全一致
- **调色板索引渲染** — 320×200 的 256 色帧缓冲，通过两阶段纹理查找（纹理图集 → 光照贴图 → RGB）转换为最终颜色，与原版定点光栅化逻辑一致
- **过程纹理** — 16 块 64×64 纹理由与原版完全相同的算法生成，包含星星动画和开门动画
- **MIDI 背景音乐** — 通过 Windows MCI（`winmm`）播放 MIDI 数据
- **完全一致的相机系统** — 相同的状态机路径表（`CAMERA_TABLE`）、速度累积和旋转矩阵
- **极小体积** — 使用 Crinkler 可执行文件压缩器，将 PE 文件压缩到极致

## 文件结构

```
omniscent_all.c        — 纯软光栅单文件版（所有模块内联）
src/
├── main.c                 — Win32 窗口、消息循环、帧计时器
├── omniscent.c / .h       — 核心状态机（初始化 / 计时 / 渲染分发）
├── camera.c / .h          — 相机动画（路径表、速度累积、旋转矩阵）
├── model.c / .h           — 3D 模型生成（"海龟建模"式挤出解析器）
├── texture.c / .h         — 过程纹理生成（16 块纹理 + 动画）
├── palette.c / .h         — 调色板生成与动态变化
├── lightmap.c / .h        — 光照贴图（调色板索引 × 光照等级 → RGB）
├── random.c               — 线性同余随机数（与原版种子一致）
├── util.c                 — 向量旋转、快速排序、位图字体、星形绘制
├── midi.c / .h            — MIDI 播放（MCI 接口）
├── renderer.c             — 渲染器分发器（软渲染 / 硬渲染切换）
├── renderer_sw.c / .h     — 软渲染器（扫描线多边形填充 + 透视校正）
├── renderer_gl.c / .h     — OpenGL 硬渲染器（GLSL 120 着色器 + VBO）
└── renderer_common.c / .h — 共享代码：四边形排序、背景文字生成
```

## 编译

### 前置条件

- **MSVC**（Visual Studio 2019 或更高版本）
- **CMake** 3.10+
- **Crinkler** (将最新版的Crinkler.exe放置在本工程根目录下)

### 编译步骤

```bash
mkdir build && cd build
cmake ..
nmake
```

编译完成后生成 `omniscent.exe`（纯软光栅版）和 `omniscent_gl.exe`（双渲染版），均为独立可执行文件，无外部 DLL 依赖。

## 按键控制

| 按键 | 功能 |
|------|------|
| **F1** | 切换软渲染 / OpenGL 硬渲染（仅双渲染版） |
| **ESC** | 退出 |

## 技术说明

### 3D 模型生成

模型数据存储在 `MODEL_TABLE` 压缩字节流中，最终生成约 362 个顶点和 367 个四边形。

### 软渲染器（扫描线光栅化）

纯 CPU 实现的多边形填充管线，包含完整的几何处理流程：

- **相机空间变换** — 顶点与相机矩阵相乘
- **深度排序** — 四边形按 Z 值快速排序（Painter's Algorithm）
- **Z 轴裁剪** — 穿越裁剪面的四边形自动计算交点并生成新顶点
- **透视校正** — 基于 1/z 的 UV 插值，每 16 像素块重新计算校正
- **扫描线填充** — 逐行计算左右边界，逐像素查纹理和光照贴图
- **雾化效果** — 基于深度和顶点光照的动态暗化

### OpenGL 硬渲染器

将软渲染器的结果映射到 GPU 管线：

- **纹理图集** — 15 块 64×64 纹理打包为 256×256 的 Luminance 纹理
- **光照贴图纹理** — 128×256 RGBA 纹理，动态更新
- **GLSL 120 着色器** — 顶点着色器执行透视变换，片元着色器执行双纹理查找
- **自定义投影矩阵** — 复现原版的伪透视 W 通道除法，通过 `glLoadMatrixf` 加载
- **动态 VBO** — 每帧重建顶点/索引缓冲区

### 编译优化

纯软光栅版使用以下策略压缩可执行文件体积：

- **Crinkler 链接器** — 专为 demoscene 设计的 PE 压缩器，配合 `/COMPMODE:SLOW` 和 `/ORDERTRIES:5000` 寻找最优节区排列
- **MSVC 体积优化** — `/Os`（优化体积）、`/Oy`（省略帧指针）、`/GS-`（关闭安全检查）、`/GR-`（关闭 RTTI）、`/Zl`（省略 .Z7 调试信息）
- **内联汇编** — 手写 `memset` 和 `_ftol2`，避免 CRT 依赖
- **`/NODEFAULTLIB`** — 不链接标准 C 运行时，入口点直接设为 `main`

### 软渲染 vs OpenGL

两种渲染器的视觉输出完全一致——软渲染是参考实现，GL 渲染器是在其基础上的 GPU 加速移植，窗口模式下画面更加高清流畅。

## 致谢

- **Omniscent**（原版）: <https://files.scene.org/view/demos/groups/sanction/snc_omni.zip>
- **Omniscent**（SuperSodaSea 的 Typescript + WebGL 移植，本项目主要参考）: <https://github.com/SuperSodaSea/Omniscent>
- **Crinkler** : <https://github.com/runestubbe/Crinkler>
- **qwen3.6-plus**
