/*
MIT License

Copyright (c) 2026 Shenzhen Open Source Co-Creation Technology Co., Ltd. (AtomGit)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * @file lv_font_stream_bin.h
 * @brief 流式读取 LVGL bin 字库（head/cmap/loca 进 RAM，glyf 位图按需从文件读），降低峰值堆占用。
 *
 * 内置约 1KB 的轻量 LRU：缓存已读过的字形 **打包位图**，重复绘制时不再读 Flash（大于单槽容量的大字仍走堆缓冲）。
 * 另有小 LRU（默认可选若干槽）缓存 **glyph 度量**（adv/box/ofs），减轻 get_glyph_dsc 与 get_glyph_bitmap 对同一字的重复 seek/按位读。
 * 可通过编译前定义 XH_STREAM_GLYPH_CACHE_* / XH_STREAM_METRICS_CACHE_SLOTS 调整（见 lv_font_stream_bin.c）。
 *
 * 约束：
 * - 仅支持非压缩位图（compression_id == LV_FONT_FMT_TXT_PLAIN）；压缩 bin 请用官方一次性加载或后续扩展。
 * - 创建后保持 lv_fs 文件句柄打开；请在单任务（如 LvglTask）内使用，或自加互斥保护 seek/read。
 * - 未加载 kern 表，字距为 0（与 kern_dsc == NULL 的 fmt_txt 行为一致）。
 */
#ifndef LV_FONT_STREAM_BIN_H
#define LV_FONT_STREAM_BIN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从 LVGL 文件路径打开字库（须已注册对应盘符驱动，如 L: LittleFS）。
 * @return 成功返回字体指针，失败 NULL（压缩格式、打开失败、解析失败等）。
 */
lv_font_t *lv_font_stream_bin_create(const char *lv_fs_path);

/**
 * 释放流式字库（关闭文件、释放 cmap/loca 等）。
 * @param font lv_font_stream_bin_create 返回值；NULL 安全。
 */
void lv_font_stream_bin_destroy(lv_font_t *font);

#ifdef __cplusplus
}
#endif

#endif /* LV_FONT_STREAM_BIN_H */
