#include "p4_text_view.h"

#include <stdio.h>
#include <string.h>

static size_t P4TextUtf8CharLen(unsigned char c)
{
    if ((c & 0x80U) == 0U) {
        return 1U;
    }
    if ((c & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if ((c & 0xF0U) == 0xE0U) {
        return 3U;
    }
    if ((c & 0xF8U) == 0xF0U) {
        return 4U;
    }
    return 1U;
}

size_t P4TextUtf8SafeLen(const char *src, size_t max_bytes)
{
    size_t pos = 0U;
    size_t last = 0U;

    if (src == NULL || max_bytes == 0U) {
        return 0U;
    }

    while (pos < max_bytes && src[pos] != '\0') {
        unsigned char c = (unsigned char)src[pos];
        size_t need = P4TextUtf8CharLen(c);

        if (pos + need > max_bytes) {
            break;
        }

        if (need > 1U) {
            size_t i;
            for (i = 1U; i < need; ++i) {
                unsigned char cc = (unsigned char)src[pos + i];
                if (cc == 0U || (cc & 0xC0U) != 0x80U) {
                    need = 1U;
                    break;
                }
            }
        }

        pos += need;
        last = pos;
    }

    return last;
}

void P4TextSafeCopyUtf8(char *dst, size_t dst_size, const char *src, size_t max_bytes)
{
    size_t safe_len;

    if (dst == NULL || dst_size == 0U) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    if (max_bytes > dst_size - 1U) {
        max_bytes = dst_size - 1U;
    }

    safe_len = P4TextUtf8SafeLen(src, max_bytes);
    if (safe_len > 0U) {
        (void)memcpy(dst, src, safe_len);
    }
    dst[safe_len] = '\0';
}

void P4TextFormatDownText(uint32_t type, const char *text, char *out, size_t out_size)
{
    const char *prefix = "文本：";
    char safe_text[704];
    int written;

    if (out == NULL || out_size == 0U) {
        return;
    }

    if (text == NULL || text[0] == '\0') {
        (void)snprintf(out, out_size, "等待对话文本");
        return;
    }

    switch (type) {
        case 0U:
            prefix = "我：";
            break;
        case 1U:
            prefix = "LLM：";
            break;
        case 2U:
            prefix = "小鸿：";
            break;
        case 3U:
            prefix = "上发：";
            break;
        default:
            prefix = "文本：";
            break;
    }

    P4TextSafeCopyUtf8(safe_text, sizeof(safe_text), text, sizeof(safe_text) - 1U);
    written = snprintf(out, out_size, "%s\n%s", prefix, safe_text);
    if (written < 0) {
        out[0] = '\0';
    }
}
