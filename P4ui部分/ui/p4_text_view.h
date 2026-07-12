#ifndef P4_TEXT_VIEW_H
#define P4_TEXT_VIEW_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t P4TextUtf8SafeLen(const char *src, size_t max_bytes);
void P4TextSafeCopyUtf8(char *dst, size_t dst_size, const char *src, size_t max_bytes);
void P4TextFormatDownText(uint32_t type, const char *text, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
