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
 * @file lv_font_stream_bin.c
 * @brief 与官方 lv_binfont 相同的磁盘布局，按需从文件读取 glyf，不把整库位图拉入 LV_MEM。
 */

#include "lv_font_stream_bin.h"

#include <stddef.h>
#include <stdio.h>

#include "los_mux.h"
#include "lvgl.h"

/** 单字形最大宽高（像素），异常 bin/损坏 metrics 时防止写爆 draw_buf、破坏栈 */
#ifndef XH_STREAM_GLYPH_BOX_MAX
#define XH_STREAM_GLYPH_BOX_MAX 512U
#endif

static UINT32 s_xh_stream_fs_mu;
static volatile uint32_t s_xh_stream_fs_mu_ready;

static void xh_stream_fs_lock(void)
{
    if(!s_xh_stream_fs_mu_ready) {
        UINT32 ret = LOS_MuxCreate(&s_xh_stream_fs_mu);
        if(ret == LOS_OK) {
            s_xh_stream_fs_mu_ready = 1U;
        }
        else {
            printf("[stream_bin] xh_stream_fs_lock: LOS_MuxCreate failed ret=%u\r\n", (unsigned)ret);
        }
    }
    if(s_xh_stream_fs_mu_ready) {
        (void)LOS_MuxPend(s_xh_stream_fs_mu, LOS_WAIT_FOREVER);
    }
}

static void xh_stream_fs_unlock(void)
{
    if(s_xh_stream_fs_mu_ready) {
        (void)LOS_MuxPost(s_xh_stream_fs_mu);
    }
}

/* 字形 LRU：总字节数（元数据+数据池），槽位数；单槽容量 = (TOTAL - sizeof(meta)*SLOTS) / SLOTS */
#ifndef XH_STREAM_GLYPH_CACHE_TOTAL
#define XH_STREAM_GLYPH_CACHE_TOTAL 1024U
#endif
#ifndef XH_STREAM_GLYPH_CACHE_SLOTS
#define XH_STREAM_GLYPH_CACHE_SLOTS 6U
#endif

/* 度量 LRU：仅元数据，约 slots * 16B + 4B；默认 8 槽约 132B */
#ifndef XH_STREAM_METRICS_CACHE_SLOTS
#define XH_STREAM_METRICS_CACHE_SLOTS 8U
#endif

/** 校验 font->dsc 确为本模块的 xh_stream_bin_dsc_t，避免与其它 lv_font_t.dsc 混淆解引用 */
#ifndef XH_STREAM_FONT_MAGIC
#define XH_STREAM_FONT_MAGIC 0x58485331u
#endif

typedef struct {
    uint32_t gid;       /* 0 表示空槽 */
    uint16_t bmp_size; /* 本槽内有效打包字节数 */
    uint16_t reserved;
    uint32_t stamp; /* 越大越新；淘汰最小 stamp */
} xh_glyph_cache_meta_t;

typedef struct {
    uint32_t gid; /* 0 表示空槽 */
    uint16_t adv_w;
    uint16_t box_w;
    uint16_t box_h;
    int16_t ofs_x;
    int16_t ofs_y;
    int32_t nbits;    /* 与 xh_glyph_calc_nbits_bmp_size 一致，命中时不再反复 seek loca */
    int32_t bmp_size;
    uint32_t stamp;
} xh_metric_cache_meta_t;

#define XH_GC_DATA_BYTES                                                                                 \
    (XH_STREAM_GLYPH_CACHE_TOTAL - (uint32_t)sizeof(xh_glyph_cache_meta_t) * XH_STREAM_GLYPH_CACHE_SLOTS)
#define XH_GC_SLOT_CAP (XH_GC_DATA_BYTES / XH_STREAM_GLYPH_CACHE_SLOTS)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(XH_STREAM_GLYPH_CACHE_TOTAL > sizeof(xh_glyph_cache_meta_t) * XH_STREAM_GLYPH_CACHE_SLOTS,
               "xh_stream glyph cache: TOTAL must exceed meta size");
#endif

/* ---- 与官方 lv_binfont_loader 一致的二进制布局 ---- */

typedef struct {
    lv_fs_file_t *fp;
    int8_t bit_pos;
    uint8_t byte_value;
} xh_bit_it_t;

typedef struct {
    uint32_t version;
    uint16_t tables_count;
    uint16_t font_size;
    uint16_t ascent;
    int16_t descent;
    uint16_t typo_ascent;
    int16_t typo_descent;
    uint16_t typo_line_gap;
    int16_t min_y;
    int16_t max_y;
    uint16_t default_advance_width;
    uint16_t kerning_scale;
    uint8_t index_to_loc_format;
    uint8_t glyph_id_format;
    uint8_t advance_width_format;
    uint8_t bits_per_pixel;
    uint8_t xy_bits;
    uint8_t wh_bits;
    uint8_t advance_width_bits;
    uint8_t compression_id;
    uint8_t subpixels_mode;
    uint8_t padding;
    int16_t underline_position;
    uint16_t underline_thickness;
} xh_font_header_bin_t;

typedef struct {
    uint32_t data_offset;
    uint32_t range_start;
    uint16_t range_length;
    uint16_t glyph_id_start;
    uint16_t data_entries_count;
    uint8_t format_type;
    uint8_t padding;
} xh_cmap_table_bin_t;

typedef struct {
    lv_fs_file_t file;
    xh_font_header_bin_t fh;

    lv_font_fmt_txt_cmap_t *cmaps;
    uint16_t cmap_num;

    /** loca 表中第一条 offset 在文件内的绝对字节偏移（紧跟 4 字节 loca_count 之后） */
    uint32_t loca_table_file_pos;
    uint32_t loca_count;

    uint32_t glyf_start;
    int32_t glyf_length;

    uint16_t line_stride_align;

    uint32_t glyph_cache_clock;
    xh_glyph_cache_meta_t glyph_cache_meta[XH_STREAM_GLYPH_CACHE_SLOTS];
    uint8_t glyph_cache_data[XH_GC_DATA_BYTES];

    uint32_t metrics_cache_clock;
    xh_metric_cache_meta_t metrics_cache_meta[XH_STREAM_METRICS_CACHE_SLOTS];

    uint32_t xh_magic;
} xh_stream_bin_dsc_t;

typedef struct {
    lv_font_t font;
    xh_stream_bin_dsc_t sd;
} xh_stream_font_wrap_t;

static const uint8_t xh_opa4_table[16] = {0,  17,  34,  51,  68,  85,  102, 119,
                                            136, 153, 170, 187, 204, 221, 238, 255};

static const uint8_t xh_opa2_table[4] = {0, 85, 170, 255};

static xh_stream_font_wrap_t *xh_font_to_wrap(lv_font_t *f)
{
    if(f == NULL) {
        return NULL;
    }
    return (xh_stream_font_wrap_t *)((uint8_t *)f - offsetof(xh_stream_font_wrap_t, font));
}

static xh_bit_it_t xh_bit_it_init(lv_fs_file_t *fp)
{
    xh_bit_it_t it;
    it.fp = fp;
    it.bit_pos = -1;
    it.byte_value = 0;
    return it;
}

static unsigned int xh_read_bits(xh_bit_it_t *it, int n_bits, lv_fs_res_t *res)
{
    unsigned int value = 0;
    while(n_bits--) {
        it->byte_value = (uint8_t)(it->byte_value << 1);
        it->bit_pos--;

        if(it->bit_pos < 0) {
            it->bit_pos = 7;
            *res = lv_fs_read(it->fp, &(it->byte_value), 1, NULL);
            if(*res != LV_FS_RES_OK) {
                return 0;
            }
        }
        int8_t bit = (it->byte_value & 0x80) ? 1 : 0;
        value |= (unsigned int)((unsigned int)bit << (unsigned int)n_bits);
    }
    *res = LV_FS_RES_OK;
    return value;
}

static int xh_read_bits_signed(xh_bit_it_t *it, int n_bits, lv_fs_res_t *res)
{
    unsigned int value = xh_read_bits(it, n_bits, res);
    if(value & (1U << (n_bits - 1))) {
        value |= ~0U << n_bits;
    }
    return (int)value;
}

static int xh_read_label(lv_fs_file_t *fp, int start, const char *label)
{
    lv_fs_seek(fp, start, LV_FS_SEEK_SET);

    uint32_t length;
    char buf[4];

    if(lv_fs_read(fp, &length, 4, NULL) != LV_FS_RES_OK || lv_fs_read(fp, buf, 4, NULL) != LV_FS_RES_OK ||
       lv_memcmp(label, buf, 4) != 0) {
        LV_LOG_WARN("stream_bin: bad label @%d", start);
        return -1;
    }

    return (int)length;
}

static uint16_t *xh_bsearch_u16_list(uint16_t key, const uint16_t *arr, uint32_t n)
{
    int32_t lo = 0;
    int32_t hi = (int32_t)n - 1;
    while(lo <= hi) {
        int32_t mid = (lo + hi) / 2;
        if(arr[mid] == key) {
            return (uint16_t *)(arr + mid);
        }
        if(arr[mid] < key) {
            lo = mid + 1;
        }
        else {
            hi = mid - 1;
        }
    }
    return NULL;
}

static uint32_t xh_get_glyph_id(const xh_stream_bin_dsc_t *sd, uint32_t letter)
{
    if(letter == '\0') {
        return 0;
    }

    for(uint16_t i = 0; i < sd->cmap_num; i++) {
        uint32_t rcp = letter - sd->cmaps[i].range_start;
        if(rcp >= sd->cmaps[i].range_length) {
            continue;
        }

        uint32_t glyph_id = 0;
        if(sd->cmaps[i].type == LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY) {
            glyph_id = sd->cmaps[i].glyph_id_start + rcp;
        }
        else if(sd->cmaps[i].type == LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
            const uint8_t *gid_ofs_8 = sd->cmaps[i].glyph_id_ofs_list;
            if(gid_ofs_8[rcp] == 0 && letter != sd->cmaps[i].range_start) {
                continue;
            }
            glyph_id = sd->cmaps[i].glyph_id_start + gid_ofs_8[rcp];
        }
        else if(sd->cmaps[i].type == LV_FONT_FMT_TXT_CMAP_SPARSE_TINY) {
            uint16_t key = (uint16_t)rcp;
            uint16_t *p = xh_bsearch_u16_list(key, sd->cmaps[i].unicode_list, sd->cmaps[i].list_length);
            if(p == NULL) {
                /* 落在本段 range 内但未列入稀疏表：须继续查后续 cmap（如另一段含标点/ASCII） */
                continue;
            }
            {
                lv_uintptr_t ofs = (lv_uintptr_t)(p - sd->cmaps[i].unicode_list);

                glyph_id = sd->cmaps[i].glyph_id_start + (uint32_t)ofs;
            }
        }
        else if(sd->cmaps[i].type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
            uint16_t key = (uint16_t)rcp;
            uint16_t *p = xh_bsearch_u16_list(key, sd->cmaps[i].unicode_list, sd->cmaps[i].list_length);
            if(p == NULL) {
                continue;
            }
            {
                lv_uintptr_t ofs = (lv_uintptr_t)(p - sd->cmaps[i].unicode_list);
                const uint16_t *gid_ofs_16 = sd->cmaps[i].glyph_id_ofs_list;

                glyph_id = sd->cmaps[i].glyph_id_start + gid_ofs_16[ofs];
            }
        }

        return glyph_id;
    }

    return 0;
}

static void xh_free_cmaps(lv_font_fmt_txt_cmap_t *cmaps, uint16_t cmap_num)
{
    if(cmaps == NULL) {
        return;
    }
    for(int i = 0; i < cmap_num; ++i) {
        lv_free((void *)cmaps[i].glyph_id_ofs_list);
        lv_free((void *)cmaps[i].unicode_list);
    }
    lv_free(cmaps);
}

static bool xh_load_cmaps_tables(lv_fs_file_t *fp, xh_stream_bin_dsc_t *sd, uint32_t cmaps_start,
                                 xh_cmap_table_bin_t *cmap_table)
{
    if(lv_fs_read(fp, cmap_table, sd->cmap_num * sizeof(xh_cmap_table_bin_t), NULL) != LV_FS_RES_OK) {
        return false;
    }

    for(unsigned int i = 0; i < sd->cmap_num; ++i) {
        lv_fs_res_t res = lv_fs_seek(fp, cmaps_start + cmap_table[i].data_offset, LV_FS_SEEK_SET);
        if(res != LV_FS_RES_OK) {
            return false;
        }

        lv_font_fmt_txt_cmap_t *cmap = &sd->cmaps[i];
        cmap->range_start = cmap_table[i].range_start;
        cmap->range_length = cmap_table[i].range_length;
        cmap->glyph_id_start = cmap_table[i].glyph_id_start;
        cmap->type = cmap_table[i].format_type;

        switch(cmap_table[i].format_type) {
            case LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL: {
                uint8_t ids_size = (uint8_t)(sizeof(uint8_t) * cmap_table[i].data_entries_count);
                uint8_t *glyph_id_ofs_list = lv_malloc(ids_size);
                cmap->glyph_id_ofs_list = glyph_id_ofs_list;
                if(lv_fs_read(fp, glyph_id_ofs_list, ids_size, NULL) != LV_FS_RES_OK) {
                    return false;
                }
                cmap->list_length = cmap->range_length;
                break;
            }
            case LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY:
                break;
            case LV_FONT_FMT_TXT_CMAP_SPARSE_FULL:
            case LV_FONT_FMT_TXT_CMAP_SPARSE_TINY: {
                uint32_t list_size = sizeof(uint16_t) * cmap_table[i].data_entries_count;
                uint16_t *unicode_list = lv_malloc(list_size);
                cmap->unicode_list = unicode_list;
                cmap->list_length = cmap_table[i].data_entries_count;
                if(lv_fs_read(fp, unicode_list, list_size, NULL) != LV_FS_RES_OK) {
                    return false;
                }
                if(cmap_table[i].format_type == LV_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
                    uint16_t *buf = lv_malloc(sizeof(uint16_t) * cmap->list_length);
                    cmap->glyph_id_ofs_list = buf;
                    if(lv_fs_read(fp, buf, sizeof(uint16_t) * cmap->list_length, NULL) != LV_FS_RES_OK) {
                        return false;
                    }
                }
                break;
            }
            default:
                LV_LOG_WARN("stream_bin: unknown cmap type %u", cmap_table[i].format_type);
                return false;
        }
    }
    return true;
}

static int32_t xh_load_cmaps(lv_fs_file_t *fp, xh_stream_bin_dsc_t *sd, uint32_t cmaps_start)
{
    int32_t cmaps_length = xh_read_label(fp, (int)cmaps_start, "cmap");
    if(cmaps_length < 0) {
        printf("[stream_bin] cmap: label @0x%lx not found or bad (expect chunk after head)\r\n",
               (unsigned long)cmaps_start);
        return -1;
    }

    uint32_t cmaps_subtables_count;
    if(lv_fs_read(fp, &cmaps_subtables_count, sizeof(uint32_t), NULL) != LV_FS_RES_OK) {
        printf("[stream_bin] cmap: read subtable_count failed\r\n");
        return -1;
    }
    if(cmaps_subtables_count == 0U || cmaps_subtables_count > 65535U) {
        LV_LOG_WARN("stream_bin: bad cmap subtable count %" LV_PRIu32, cmaps_subtables_count);
        printf("[stream_bin] cmap: bad subtable_count=%lu\r\n", (unsigned long)cmaps_subtables_count);
        return -1;
    }

    lv_font_fmt_txt_cmap_t *cmaps = lv_malloc(cmaps_subtables_count * sizeof(lv_font_fmt_txt_cmap_t));
    if(cmaps == NULL) {
        printf("[stream_bin] cmap: lv_malloc(cmaps) failed (need %u bytes)\r\n",
               (unsigned)(cmaps_subtables_count * (unsigned)sizeof(lv_font_fmt_txt_cmap_t)));
        return -1;
    }
    lv_memset(cmaps, 0, cmaps_subtables_count * sizeof(lv_font_fmt_txt_cmap_t));
    sd->cmaps = cmaps;
    sd->cmap_num = (uint16_t)cmaps_subtables_count;

    xh_cmap_table_bin_t *cmaps_tables = lv_malloc(sizeof(xh_cmap_table_bin_t) * sd->cmap_num);
    if(cmaps_tables == NULL) {
        printf("[stream_bin] cmap: lv_malloc(cmaps_tables) failed\r\n");
        xh_free_cmaps(sd->cmaps, sd->cmap_num);
        sd->cmaps = NULL;
        sd->cmap_num = 0;
        return -1;
    }

    bool success = xh_load_cmaps_tables(fp, sd, cmaps_start, cmaps_tables);
    lv_free(cmaps_tables);
    if(!success) {
        printf("[stream_bin] cmap: xh_load_cmaps_tables failed (read/unknown cmap type)\r\n");
    }

    return success ? cmaps_length : -1;
}

/** 从文件读取 loca 第 gid 条（不整表进 RAM，省堆；大字号库 loca 可达数万×4B） */
static bool xh_stream_read_loca_off(xh_stream_bin_dsc_t *sd, uint32_t gid, uint32_t *out_off)
{
    if(gid >= sd->loca_count || out_off == NULL) {
        return false;
    }
    lv_fs_res_t res;
    if(sd->fh.index_to_loc_format == 0) {
        res = lv_fs_seek(&sd->file, sd->loca_table_file_pos + gid * 2U, LV_FS_SEEK_SET);
        if(res != LV_FS_RES_OK) {
            return false;
        }
        uint16_t v16;
        if(lv_fs_read(&sd->file, &v16, sizeof(v16), NULL) != LV_FS_RES_OK) {
            return false;
        }
        *out_off = v16;
    }
    else {
        res = lv_fs_seek(&sd->file, sd->loca_table_file_pos + gid * 4U, LV_FS_SEEK_SET);
        if(res != LV_FS_RES_OK) {
            return false;
        }
        uint32_t v32;
        if(lv_fs_read(&sd->file, &v32, sizeof(v32), NULL) != LV_FS_RES_OK) {
            return false;
        }
        *out_off = v32;
    }
    return true;
}

static void xh_glyph_calc_nbits_bmp_size(xh_stream_bin_dsc_t *sd, uint32_t gid, int *nbits_out, int *bmp_size_out)
{
    const xh_font_header_bin_t *h = &sd->fh;
    int nbits = h->advance_width_bits + 2 * h->xy_bits + 2 * h->wh_bits;
    uint32_t cur;
    uint32_t next_u;

    if(!xh_stream_read_loca_off(sd, gid, &cur)) {
        *nbits_out = nbits;
        *bmp_size_out = 0;
        return;
    }
    if(gid < sd->loca_count - 1U) {
        if(!xh_stream_read_loca_off(sd, gid + 1U, &next_u)) {
            *nbits_out = nbits;
            *bmp_size_out = 0;
            return;
        }
    }
    else {
        next_u = (uint32_t)sd->glyf_length;
    }
    int next_off = (int)next_u;
    *nbits_out = nbits;
    int bmp = next_off - (int)cur - nbits / 8;
    if(bmp < 0) {
        bmp = 0;
    }
    /* 单字形打包位图异常大时视为损坏，避免 lv_malloc / 展开写越界 */
    if(bmp > (256 * 256 * 4 + 4096)) {
        bmp = 0;
    }
    *bmp_size_out = bmp;
}

static int xh_mh_find_hit(xh_stream_bin_dsc_t *sd, uint32_t gid)
{
    for(uint32_t i = 0; i < XH_STREAM_METRICS_CACHE_SLOTS; i++) {
        if(sd->metrics_cache_meta[i].gid == gid) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t xh_mh_pick_victim(xh_stream_bin_dsc_t *sd)
{
    for(uint32_t i = 0; i < XH_STREAM_METRICS_CACHE_SLOTS; i++) {
        if(sd->metrics_cache_meta[i].gid == 0) {
            return i;
        }
    }
    uint32_t v = 0;
    uint32_t m = sd->metrics_cache_meta[0].stamp;
    for(uint32_t i = 1; i < XH_STREAM_METRICS_CACHE_SLOTS; i++) {
        if(sd->metrics_cache_meta[i].stamp < m) {
            m = sd->metrics_cache_meta[i].stamp;
            v = i;
        }
    }
    return v;
}

static void xh_mh_touch(xh_stream_bin_dsc_t *sd, uint32_t slot)
{
    sd->metrics_cache_clock++;
    if(sd->metrics_cache_clock == 0u) {
        lv_memset(sd->metrics_cache_meta, 0, sizeof(sd->metrics_cache_meta));
        sd->metrics_cache_clock = 1u;
    }
    sd->metrics_cache_meta[slot].stamp = sd->metrics_cache_clock;
}

static bool xh_stream_read_glyph_metrics(xh_stream_bin_dsc_t *sd, uint32_t gid, uint16_t *adv_w, int16_t *ofs_x,
                                         int16_t *ofs_y, uint16_t *box_w, uint16_t *box_h, int *nbits_out,
                                         int *bmp_size_out)
{
    const xh_font_header_bin_t *h = &sd->fh;
    if(gid >= sd->loca_count) {
        return false;
    }

    {
        int hi = xh_mh_find_hit(sd, gid);
        if(hi >= 0) {
            const xh_metric_cache_meta_t *me = &sd->metrics_cache_meta[(uint32_t)hi];
            *adv_w = me->adv_w;
            *ofs_x = me->ofs_x;
            *ofs_y = me->ofs_y;
            *box_w = me->box_w;
            *box_h = me->box_h;
            *nbits_out = me->nbits;
            *bmp_size_out = me->bmp_size;
            xh_mh_touch(sd, (uint32_t)hi);
            return true;
        }
    }

    uint32_t goff;
    if(!xh_stream_read_loca_off(sd, gid, &goff)) {
        return false;
    }
    lv_fs_res_t res = lv_fs_seek(&sd->file, sd->glyf_start + goff, LV_FS_SEEK_SET);
    if(res != LV_FS_RES_OK) {
        return false;
    }

    xh_bit_it_t bit_it = xh_bit_it_init(&sd->file);

    uint16_t aw;
    if(h->advance_width_bits == 0) {
        aw = h->default_advance_width;
    }
    else {
        aw = (uint16_t)xh_read_bits(&bit_it, h->advance_width_bits, &res);
        if(res != LV_FS_RES_OK) {
            return false;
        }
    }
    if(h->advance_width_format == 0) {
        aw = (uint16_t)(aw * 16U);
    }

    int16_t ox = (int16_t)xh_read_bits_signed(&bit_it, h->xy_bits, &res);
    if(res != LV_FS_RES_OK) {
        return false;
    }
    int16_t oy = (int16_t)xh_read_bits_signed(&bit_it, h->xy_bits, &res);
    if(res != LV_FS_RES_OK) {
        return false;
    }
    uint16_t bw = (uint16_t)xh_read_bits(&bit_it, h->wh_bits, &res);
    if(res != LV_FS_RES_OK) {
        return false;
    }
    uint16_t bh = (uint16_t)xh_read_bits(&bit_it, h->wh_bits, &res);
    if(res != LV_FS_RES_OK) {
        return false;
    }

    int nbits;
    int bmp_size;

    if(gid == 0) {
        aw = 0;
        bw = 0;
        bh = 0;
        ox = 0;
        oy = 0;
    }

    xh_glyph_calc_nbits_bmp_size(sd, gid, &nbits, &bmp_size);

    *adv_w = aw;
    *ofs_x = ox;
    *ofs_y = oy;
    *box_w = bw;
    *box_h = bh;
    *nbits_out = nbits;
    *bmp_size_out = bmp_size;

    {
        uint32_t vi = xh_mh_pick_victim(sd);
        xh_metric_cache_meta_t *me = &sd->metrics_cache_meta[vi];
        me->gid = gid;
        me->adv_w = aw;
        me->ofs_x = ox;
        me->ofs_y = oy;
        me->box_w = bw;
        me->box_h = bh;
        me->nbits = nbits;
        me->bmp_size = bmp_size;
        xh_mh_touch(sd, vi);
    }

    return true;
}

static bool xh_read_glyph_packed_bitmap(xh_stream_bin_dsc_t *sd, uint32_t gid, int nbits, int bmp_size,
                                        uint8_t *out_packed)
{
    if(bmp_size <= 0 || out_packed == NULL) {
        return false;
    }

    uint32_t goff;
    if(!xh_stream_read_loca_off(sd, gid, &goff)) {
        return false;
    }
    lv_fs_res_t res = lv_fs_seek(&sd->file, sd->glyf_start + goff, LV_FS_SEEK_SET);
    if(res != LV_FS_RES_OK) {
        return false;
    }

    xh_bit_it_t bit_it = xh_bit_it_init(&sd->file);
    xh_read_bits(&bit_it, nbits, &res);
    if(res != LV_FS_RES_OK) {
        return false;
    }

    if(nbits % 8 == 0) {
        if(lv_fs_read(&sd->file, out_packed, (uint32_t)bmp_size, NULL) != LV_FS_RES_OK) {
            return false;
        }
    }
    else {
        for(int k = 0; k < bmp_size - 1; ++k) {
            out_packed[k] = (uint8_t)xh_read_bits(&bit_it, 8, &res);
            if(res != LV_FS_RES_OK) {
                return false;
            }
        }
        out_packed[bmp_size - 1] = (uint8_t)xh_read_bits(&bit_it, 8 - nbits % 8, &res);
        if(res != LV_FS_RES_OK) {
            return false;
        }
        out_packed[bmp_size - 1] = (uint8_t)(out_packed[bmp_size - 1] << (nbits % 8));
    }
    return true;
}

static void xh_expand_plain_to_draw_buf(const uint8_t *bitmap_in, uint8_t bpp, uint16_t box_w, uint16_t box_h,
                                        uint32_t stride_in, lv_draw_buf_t *draw_buf)
{
    if(bitmap_in == NULL || draw_buf == NULL || draw_buf->data == NULL) {
        return;
    }
    /*
     * 须与 LVGL 9.3 lv_font_get_bitmap_fmt_txt() 一致：始终用 lv_draw_buf_width_to_stride(box_w, A8)
     * 作为行步长，并从 draw_buf->data 起写。误用 header.stride（若小于上述值）会在 bitmap_out[x] 横向写越界，
     * 破坏栈/堆，表现为 fp 失效、NMI。
     */
    uint32_t stride_out = lv_draw_buf_width_to_stride(box_w, LV_COLOR_FORMAT_A8);
    if(stride_out < (uint32_t)box_w) {
        printf("[stream_bin] expand: stride_out %u < box_w %u\r\n", (unsigned)stride_out, (unsigned)box_w);
        return;
    }
    if((uint64_t)stride_out * (uint64_t)box_h > (uint64_t)draw_buf->data_size) {
        printf("[stream_bin] expand: need %u*%u bytes, data_size=%u\r\n", (unsigned)box_h, (unsigned)stride_out,
               (unsigned)draw_buf->data_size);
        return;
    }
    uint8_t *bitmap_out_tmp = draw_buf->data;
    int32_t i = 0;
    int32_t x, y;

    if(bpp == 1) {
        for(y = 0; y < box_h; y++) {
            uint16_t line_rem = stride_in != 0 ? (uint16_t)stride_in : box_w;
            for(x = 0; x < box_w; x++, i++) {
                i = i & 0x7;
                if(i == 0) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x80 ? 0xffU : 0x00U;
                }
                else if(i == 1) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x40 ? 0xffU : 0x00U;
                }
                else if(i == 2) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x20 ? 0xffU : 0x00U;
                }
                else if(i == 3) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x10 ? 0xffU : 0x00U;
                }
                else if(i == 4) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x08 ? 0xffU : 0x00U;
                }
                else if(i == 5) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x04 ? 0xffU : 0x00U;
                }
                else if(i == 6) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x02 ? 0xffU : 0x00U;
                }
                else if(i == 7) {
                    bitmap_out_tmp[x] = (*bitmap_in) & 0x01 ? 0xffU : 0x00U;
                    line_rem--;
                    bitmap_in++;
                }
            }
            if(stride_in) {
                i = 0;
                bitmap_in += line_rem;
            }
            bitmap_out_tmp += stride_out;
        }
    }
    else if(bpp == 2) {
        for(y = 0; y < box_h; y++) {
            uint32_t line_rem = stride_in != 0 ? stride_in : box_w;
            for(x = 0; x < box_w; x++, i++) {
                i = i & 0x3;
                if(i == 0) {
                    bitmap_out_tmp[x] = xh_opa2_table[(*bitmap_in) >> 6];
                }
                else if(i == 1) {
                    bitmap_out_tmp[x] = xh_opa2_table[((*bitmap_in) >> 4) & 0x3];
                }
                else if(i == 2) {
                    bitmap_out_tmp[x] = xh_opa2_table[((*bitmap_in) >> 2) & 0x3];
                }
                else if(i == 3) {
                    bitmap_out_tmp[x] = xh_opa2_table[((*bitmap_in) >> 0) & 0x3];
                    line_rem--;
                    bitmap_in++;
                }
            }
            if(stride_in) {
                i = 0;
                bitmap_in += line_rem;
            }
            bitmap_out_tmp += stride_out;
        }
    }
    else if(bpp == 4) {
        for(y = 0; y < box_h; y++) {
            uint16_t line_rem = stride_in != 0 ? (uint16_t)stride_in : box_w;
            for(x = 0; x < box_w; x++, i++) {
                i = i & 0x1;
                if(i == 0) {
                    bitmap_out_tmp[x] = xh_opa4_table[(*bitmap_in) >> 4];
                }
                else if(i == 1) {
                    bitmap_out_tmp[x] = xh_opa4_table[(*bitmap_in) & 0xF];
                    line_rem--;
                    bitmap_in++;
                }
            }
            if(stride_in) {
                i = 0;
                bitmap_in += line_rem;
            }
            bitmap_out_tmp += stride_out;
        }
    }
    else if(bpp == 8) {
        for(y = 0; y < box_h; y++) {
            uint16_t line_rem = stride_in != 0 ? (uint16_t)stride_in : box_w;
            for(x = 0; x < box_w; x++, i++) {
                bitmap_out_tmp[x] = *bitmap_in;
                line_rem--;
                bitmap_in++;
            }
            bitmap_out_tmp += stride_out;
            bitmap_in += line_rem;
        }
    }
    else {
        LV_LOG_WARN("stream_bin: bpp %u not supported", bpp);
    }
}

static uint8_t *xh_gc_slot_data(xh_stream_bin_dsc_t *sd, uint32_t slot)
{
    return sd->glyph_cache_data + slot * XH_GC_SLOT_CAP;
}

static int xh_gc_find_hit(xh_stream_bin_dsc_t *sd, uint32_t gid, int bmp_size)
{
    if(bmp_size <= 0 || (uint32_t)bmp_size > XH_GC_SLOT_CAP) {
        return -1;
    }
    uint16_t bs = (uint16_t)bmp_size;
    for(uint32_t i = 0; i < XH_STREAM_GLYPH_CACHE_SLOTS; i++) {
        if(sd->glyph_cache_meta[i].gid == gid && sd->glyph_cache_meta[i].bmp_size == bs) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t xh_gc_pick_victim(xh_stream_bin_dsc_t *sd)
{
    for(uint32_t i = 0; i < XH_STREAM_GLYPH_CACHE_SLOTS; i++) {
        if(sd->glyph_cache_meta[i].gid == 0) {
            return i;
        }
    }
    uint32_t v = 0;
    uint32_t m = sd->glyph_cache_meta[0].stamp;
    for(uint32_t i = 1; i < XH_STREAM_GLYPH_CACHE_SLOTS; i++) {
        if(sd->glyph_cache_meta[i].stamp < m) {
            m = sd->glyph_cache_meta[i].stamp;
            v = i;
        }
    }
    return v;
}

static void xh_gc_touch(xh_stream_bin_dsc_t *sd, uint32_t slot)
{
    sd->glyph_cache_clock++;
    if(sd->glyph_cache_clock == 0u) {
        lv_memset(sd->glyph_cache_meta, 0, sizeof(sd->glyph_cache_meta));
        sd->glyph_cache_clock = 1u;
    }
    sd->glyph_cache_meta[slot].stamp = sd->glyph_cache_clock;
}

static bool xh_stream_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter,
                                    uint32_t unicode_letter_next)
{
    LV_UNUSED(unicode_letter_next);
    /* 与 xh_stream_get_glyph_bitmap 一致：font 释放后若仍被样式引用会 UAF；至少避免 NULL dsc 直接 fault */
    if(font == NULL || font->dsc == NULL || dsc_out == NULL) {
        return false;
    }
    xh_stream_bin_dsc_t *sd = (xh_stream_bin_dsc_t *)font->dsc;
    if(sd->xh_magic != XH_STREAM_FONT_MAGIC) {
        return false;
    }

    bool is_tab = unicode_letter == '\t';
    if(is_tab) {
        unicode_letter = ' ';
    }

    uint32_t gid = xh_get_glyph_id(sd, unicode_letter);
    if(!gid) {
        return false;
    }
    if(gid >= sd->loca_count) {
        return false;
    }

    uint16_t adv_w;
    int16_t ofs_x, ofs_y;
    uint16_t box_w, box_h;
    int nbits, bmp_size;
    xh_stream_fs_lock();
    bool metrics_ok = xh_stream_read_glyph_metrics(sd, gid, &adv_w, &ofs_x, &ofs_y, &box_w, &box_h, &nbits, &bmp_size);
    xh_stream_fs_unlock();
    if(!metrics_ok) {
        return false;
    }
    LV_UNUSED(bmp_size);
    LV_UNUSED(nbits);

    uint32_t adv_final = adv_w;
    if(is_tab) {
        adv_final *= 2U;
    }
    adv_final = (adv_final + (1U << 3)) >> 4;

    dsc_out->adv_w = adv_final;
    dsc_out->box_h = box_h;
    dsc_out->box_w = box_w;
    dsc_out->ofs_x = ofs_x;
    dsc_out->ofs_y = ofs_y;

    if(sd->line_stride_align == 0) {
        dsc_out->stride = 0;
    }
    else {
        uint32_t bit_count = (uint32_t)box_w * sd->fh.bits_per_pixel;
        uint32_t width_in_bytes = (bit_count + 7U) >> 3;
        dsc_out->stride = (uint16_t)LV_ROUND_UP(width_in_bytes, sd->line_stride_align);
    }

    /* LVGL 9.3：lv_font_glyph_dsc_t 含 outline_stroke_width；未初始化可能导致矢量/绘制路径异常 */
    dsc_out->outline_stroke_width = 0;
    dsc_out->format = (lv_font_glyph_format_t)sd->fh.bits_per_pixel;
    dsc_out->is_placeholder = false;
    dsc_out->gid.index = gid;

    if(is_tab) {
        dsc_out->box_w = dsc_out->box_w * 2U;
    }

    return true;
}

static const void *xh_stream_get_glyph_bitmap(lv_font_glyph_dsc_t *g_dsc, lv_draw_buf_t *draw_buf)
{
    if(g_dsc == NULL) {
        return NULL;
    }
    if(g_dsc->req_raw_bitmap) {
        LV_LOG_WARN("stream_bin: req_raw_bitmap not supported");
        return NULL;
    }
    /* 未解析到具体 lv_font_t 时勿解引用 font->dsc，否则 Load fault（mcause 取指/加载，mtval/a0≈0） */
    const lv_font_t *font = g_dsc->resolved_font;
    if(font == NULL || font->dsc == NULL) {
        return NULL;
    }
    if(draw_buf == NULL || draw_buf->data == NULL) {
        return NULL;
    }

    xh_stream_bin_dsc_t *sd = (xh_stream_bin_dsc_t *)font->dsc;
    if(sd->xh_magic != XH_STREAM_FONT_MAGIC) {
        return NULL;
    }
    uint32_t gid = g_dsc->gid.index;
    if(!gid) {
        return NULL;
    }
    if(gid >= sd->loca_count) {
        return NULL;
    }

    uint16_t adv_w;
    int16_t ofs_x, ofs_y;
    uint16_t box_w, box_h;
    int nbits, bmp_size;

    xh_stream_fs_lock();
    if(!xh_stream_read_glyph_metrics(sd, gid, &adv_w, &ofs_x, &ofs_y, &box_w, &box_h, &nbits, &bmp_size)) {
        xh_stream_fs_unlock();
        return NULL;
    }
    LV_UNUSED(adv_w);
    LV_UNUSED(ofs_x);
    LV_UNUSED(ofs_y);

    int32_t gsize = (int32_t)box_w * (int32_t)box_h;
    if(gsize == 0) {
        xh_stream_fs_unlock();
        return NULL;
    }

    if(bmp_size <= 0) {
        xh_stream_fs_unlock();
        return NULL;
    }

    if(box_w > XH_STREAM_GLYPH_BOX_MAX || box_h > XH_STREAM_GLYPH_BOX_MAX) {
        printf("[stream_bin] reject glyph box %ux%u (max %u)\r\n", (unsigned)box_w, (unsigned)box_h,
               (unsigned)XH_STREAM_GLYPH_BOX_MAX);
        xh_stream_fs_unlock();
        return NULL;
    }
    if(draw_buf->header.w > 0U && draw_buf->header.h > 0U) {
        if(box_w > draw_buf->header.w || box_h > draw_buf->header.h) {
            printf("[stream_bin] box %ux%u > draw_buf %ux%u\r\n", (unsigned)box_w, (unsigned)box_h,
                   (unsigned)draw_buf->header.w, (unsigned)draw_buf->header.h);
            xh_stream_fs_unlock();
            return NULL;
        }
    }

    const uint8_t *packed_src = NULL;
    uint8_t *packed_heap = NULL;

    if((uint32_t)bmp_size <= XH_GC_SLOT_CAP) {
        int hi = xh_gc_find_hit(sd, gid, bmp_size);
        if(hi >= 0) {
            xh_gc_touch(sd, (uint32_t)hi);
            packed_src = xh_gc_slot_data(sd, (uint32_t)hi);
        }
        else {
            uint32_t vi = xh_gc_pick_victim(sd);
            uint8_t *dst = xh_gc_slot_data(sd, vi);
            if(xh_read_glyph_packed_bitmap(sd, gid, nbits, bmp_size, dst)) {
                sd->glyph_cache_meta[vi].gid = gid;
                sd->glyph_cache_meta[vi].bmp_size = (uint16_t)bmp_size;
                xh_gc_touch(sd, vi);
                packed_src = dst;
            }
        }
    }

    if(packed_src == NULL) {
        packed_heap = lv_malloc((size_t)bmp_size);
        if(packed_heap == NULL) {
            printf("[stream_bin] lv_malloc packed glyph failed bmp_size=%d (heap tight?)\r\n", bmp_size);
            xh_stream_fs_unlock();
            return NULL;
        }
        if(!xh_read_glyph_packed_bitmap(sd, gid, nbits, bmp_size, packed_heap)) {
            lv_free(packed_heap);
            xh_stream_fs_unlock();
            return NULL;
        }
        packed_src = packed_heap;
    }

    xh_stream_fs_unlock();

    xh_expand_plain_to_draw_buf(packed_src, sd->fh.bits_per_pixel, box_w, box_h, g_dsc->stride, draw_buf);
    if(packed_heap != NULL) {
        lv_free(packed_heap);
    }

    /* 不在此调用 lv_draw_buf_flush_cache：9.3 依赖 draw_buf->handlers；无 D-cache 的片内 SRAM 亦无需 flush */
    return draw_buf;
}

static void xh_stream_wrap_destroy(xh_stream_font_wrap_t *w)
{
    if(w == NULL) {
        return;
    }
    xh_stream_bin_dsc_t *sd = &w->sd;
    xh_free_cmaps(sd->cmaps, sd->cmap_num);
    xh_stream_fs_lock();
    lv_fs_close(&sd->file);
    xh_stream_fs_unlock();
    lv_memset(sd, 0, sizeof(*sd));
    lv_free(w);
}

static void xh_stream_bin_dump_prefix(lv_fs_file_t *fp, const char *path_for_log)
{
    uint8_t b[16];

    (void)lv_fs_seek(fp, 0, LV_FS_SEEK_SET);
    if(lv_fs_read(fp, b, sizeof(b), NULL) == LV_FS_RES_OK) {
        printf("[stream_bin] %s first 16 bytes:", path_for_log != NULL ? path_for_log : "?");
        for(unsigned int i = 0; i < sizeof(b); i++) {
            printf(" %02x", (unsigned int)b[i]);
        }
        printf(" (valid LVGL bin: LE uint32 len + \"head\" at offset 0)\r\n");
    }
}

lv_font_t *lv_font_stream_bin_create(const char *lv_fs_path)
{
    if(lv_fs_path == NULL) {
        return NULL;
    }

    xh_stream_font_wrap_t *w = lv_malloc_zeroed(sizeof(xh_stream_font_wrap_t));
    if(w == NULL) {
        printf("[stream_bin] lv_malloc(wrapper) failed\r\n");
        return NULL;
    }

    xh_stream_bin_dsc_t *sd = &w->sd;
    lv_fs_res_t fs_res = lv_fs_open(&sd->file, lv_fs_path, LV_FS_MODE_RD);
    if(fs_res != LV_FS_RES_OK) {
        LV_LOG_WARN("stream_bin: open fail %d", fs_res);
        printf("[stream_bin] open failed res=%d path=%s\r\n", (int)fs_res, lv_fs_path);
        lv_free(w);
        return NULL;
    }

    int32_t header_length = xh_read_label(&sd->file, 0, "head");
    if(header_length < 0) {
        printf("[stream_bin] not a valid LVGL **--format bin** file (missing \"head\" chunk at offset 0). "
               "Do not use .ttf or --format lvgl .c as font.bin.\r\n");
        xh_stream_bin_dump_prefix(&sd->file, lv_fs_path);
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    if(lv_fs_read(&sd->file, &sd->fh, sizeof(xh_font_header_bin_t), NULL) != LV_FS_RES_OK) {
        printf("[stream_bin] read font header failed\r\n");
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    if(sd->fh.compression_id != 0) { /* LV_FONT_FMT_TXT_PLAIN */
        LV_LOG_WARN("stream_bin: only PLAIN bitmap supported (compression_id=%u)", sd->fh.compression_id);
        printf("[stream_bin] compression_id=%u not supported (need uncompressed PLAIN; "
               "regenerate with lv_font_conv without compression / --compress 0 if available)\r\n",
               (unsigned)sd->fh.compression_id);
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    if(sd->fh.index_to_loc_format > 1) {
        LV_LOG_WARN("stream_bin: bad loca format %u", sd->fh.index_to_loc_format);
        printf("[stream_bin] index_to_loc_format=%u invalid\r\n", (unsigned)sd->fh.index_to_loc_format);
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    uint32_t cmaps_start = (uint32_t)header_length;
    int32_t cmaps_length = xh_load_cmaps(&sd->file, sd, cmaps_start);
    if(cmaps_length < 0) {
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    uint32_t loca_start = cmaps_start + (uint32_t)cmaps_length;
    int32_t loca_length = xh_read_label(&sd->file, (int)loca_start, "loca");
    if(loca_length < 0) {
        printf("[stream_bin] loca label @0x%lx missing or corrupt\r\n", (unsigned long)loca_start);
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    if(lv_fs_read(&sd->file, &sd->loca_count, sizeof(uint32_t), NULL) != LV_FS_RES_OK) {
        printf("[stream_bin] read loca_count failed\r\n");
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    if(sd->loca_count == 0 || sd->loca_count > 65536U) {
        LV_LOG_WARN("stream_bin: bad loca_count %" LV_PRIu32, sd->loca_count);
        printf("[stream_bin] bad loca_count=%lu\r\n", (unsigned long)sd->loca_count);
        xh_stream_wrap_destroy(w);
        return NULL;
    }

    /*
     * 不在堆上分配 loca_count×4 的整表（大字库可达 100KB+，易撑爆 LV_MEM）。
     * 当前文件位置在 loca_count 之后，即为第一条 offset；按需 xh_stream_read_loca_off()。
     */
    sd->loca_table_file_pos = loca_start + 8U + 4U;

    sd->glyf_start = loca_start + (uint32_t)loca_length;
    int32_t glyf_len = xh_read_label(&sd->file, (int)sd->glyf_start, "glyf");
    if(glyf_len < 0) {
        printf("[stream_bin] glyf label @0x%lx missing or corrupt\r\n", (unsigned long)sd->glyf_start);
        xh_stream_wrap_destroy(w);
        return NULL;
    }
    sd->glyf_length = glyf_len;

    sd->line_stride_align = 0;
    sd->xh_magic = XH_STREAM_FONT_MAGIC;

    lv_font_t *font = &w->font;
    lv_memset(font, 0, sizeof(*font));
    font->dsc = sd;
    font->get_glyph_dsc = xh_stream_get_glyph_dsc;
    font->get_glyph_bitmap = xh_stream_get_glyph_bitmap;
    font->line_height = (lv_coord_t)(sd->fh.ascent - sd->fh.descent);
    font->base_line = (lv_coord_t)(-sd->fh.descent);
    font->subpx = sd->fh.subpixels_mode;
    font->underline_position = (int8_t)sd->fh.underline_position;
    font->underline_thickness = (int8_t)sd->fh.underline_thickness;

    return font;
}

void lv_font_stream_bin_destroy(lv_font_t *font)
{
    if(font == NULL) {
        return;
    }
    xh_stream_font_wrap_t *w = xh_font_to_wrap(font);
    xh_stream_wrap_destroy(w);
}
