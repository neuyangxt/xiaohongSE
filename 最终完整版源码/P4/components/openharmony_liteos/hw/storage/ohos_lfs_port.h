#ifndef OHOS_LFS_PORT_H
#define OHOS_LFS_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OHOS_LFS_BASE_PATH       "/data"
#define OHOS_LFS_PARTITION_LABEL "storage"

uint32_t OhosLfsPortInit(void);
uint32_t OhosLfsPortEnsureDir(const char *path);
uint32_t OhosLfsPortEnsureParentDir(const char *path);
uint32_t OhosLfsPortWriteText(const char *path, const char *text);
uint32_t OhosLfsPortWriteBinary(const char *path, const void *data, size_t len);
uint32_t OhosLfsPortReadTextWithLen(const char *path, char *buf, size_t buf_len, size_t *out_len);
uint32_t OhosLfsPortReadText(const char *path, char *buf, size_t buf_len);
uint32_t OhosLfsPortGetFileSize(const char *path, size_t *out_size);
uint32_t OhosLfsPortDelete(const char *path);
uint32_t OhosLfsPortFileExists(const char *path);
uint32_t OhosLfsPortSelfTest(void);

#ifdef __cplusplus
}
#endif

#endif
