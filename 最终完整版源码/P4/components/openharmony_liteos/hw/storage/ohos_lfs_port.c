#include "ohos_lfs_port.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"

#define OHOS_LFS_TAG "OHOS-LFS"
#define OHOS_LFS_PATH_MAX 192U

static int g_lfs_mounted = 0;

static uint32_t OhosLfsPortMkdirOne(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 1U;
    }

    if (mkdir(path, 0775) == 0) {
        ESP_LOGI(OHOS_LFS_TAG, "mkdir ok path=%s", path);
        return 0U;
    }

    if (errno == EEXIST) {
        return 0U;
    }

    ESP_LOGE(OHOS_LFS_TAG, "mkdir failed path=%s errno=%d", path, errno);
    return 2U;
}

uint32_t OhosLfsPortInit(void)
{
    if (g_lfs_mounted) {
        ESP_LOGI(OHOS_LFS_TAG, "LittleFS already mounted at %s", OHOS_LFS_BASE_PATH);
        return 0;
    }

    esp_vfs_littlefs_conf_t conf = {
        .base_path = OHOS_LFS_BASE_PATH,
        .partition_label = OHOS_LFS_PARTITION_LABEL,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    ESP_LOGI(OHOS_LFS_TAG,
             "Mount LittleFS base=%s partition=%s",
             OHOS_LFS_BASE_PATH,
             OHOS_LFS_PARTITION_LABEL);

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(OHOS_LFS_TAG, "LittleFS mount failed ret=%s", esp_err_to_name(ret));
        return (uint32_t)ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_littlefs_info(OHOS_LFS_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(OHOS_LFS_TAG,
                 "LittleFS mounted total=%u used=%u",
                 (unsigned)total,
                 (unsigned)used);
    } else {
        ESP_LOGW(OHOS_LFS_TAG,
                 "LittleFS mounted but info failed ret=%s",
                 esp_err_to_name(ret));
    }

    g_lfs_mounted = 1;
    return 0;
}

uint32_t OhosLfsPortEnsureDir(const char *path)
{
    char tmp[OHOS_LFS_PATH_MAX];
    size_t len;
    uint32_t ret;

    if (path == NULL || path[0] == '\0') {
        ESP_LOGE(OHOS_LFS_TAG, "EnsureDir invalid argument");
        return 1U;
    }

    ret = OhosLfsPortInit();
    if (ret != 0U) {
        return ret;
    }

    len = strlen(path);
    if (len >= sizeof(tmp)) {
        ESP_LOGE(OHOS_LFS_TAG, "EnsureDir path too long len=%u path=%s", (unsigned)len, path);
        return 2U;
    }

    (void)memcpy(tmp, path, len + 1U);
    if (len > 1U && tmp[len - 1U] == '/') {
        tmp[len - 1U] = '\0';
    }

    for (char *p = tmp + 1; *p != '\0'; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        ret = OhosLfsPortMkdirOne(tmp);
        *p = '/';
        if (ret != 0U) {
            return ret;
        }
    }

    return OhosLfsPortMkdirOne(tmp);
}

uint32_t OhosLfsPortEnsureParentDir(const char *path)
{
    char tmp[OHOS_LFS_PATH_MAX];
    char *slash;
    size_t len;

    if (path == NULL || path[0] == '\0') {
        ESP_LOGE(OHOS_LFS_TAG, "EnsureParentDir invalid argument");
        return 1U;
    }

    len = strlen(path);
    if (len >= sizeof(tmp)) {
        ESP_LOGE(OHOS_LFS_TAG, "EnsureParentDir path too long len=%u path=%s", (unsigned)len, path);
        return 2U;
    }

    (void)memcpy(tmp, path, len + 1U);
    slash = strrchr(tmp, '/');
    if (slash == NULL || slash == tmp) {
        return 0U;
    }
    *slash = '\0';

    return OhosLfsPortEnsureDir(tmp);
}

uint32_t OhosLfsPortWriteText(const char *path, const char *text)
{
    if (path == NULL || text == NULL) {
        ESP_LOGE(OHOS_LFS_TAG, "WriteText invalid argument");
        return 1;
    }

    uint32_t ret = OhosLfsPortInit();
    if (ret != 0) {
        return ret;
    }

    ret = OhosLfsPortEnsureParentDir(path);
    if (ret != 0U) {
        return ret;
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        ESP_LOGE(OHOS_LFS_TAG, "Open for write failed path=%s errno=%d", path, errno);
        return 2;
    }

    size_t textLen = strlen(text);
    size_t written = fwrite(text, 1, textLen, fp);
    int flushRet = fflush(fp);
    int closeRet = fclose(fp);

    if (written != textLen || flushRet != 0 || closeRet != 0) {
        ESP_LOGE(OHOS_LFS_TAG,
                 "Write failed path=%s written=%u expected=%u flush=%d close=%d errno=%d",
                 path,
                 (unsigned)written,
                 (unsigned)textLen,
                 flushRet,
                 closeRet,
                 errno);
        return 3;
    }

    ESP_LOGI(OHOS_LFS_TAG, "Write ok path=%s bytes=%u", path, (unsigned)written);
    return 0;
}

uint32_t OhosLfsPortWriteBinary(const char *path, const void *data, size_t len)
{
    if (path == NULL || (data == NULL && len != 0U)) {
        ESP_LOGE(OHOS_LFS_TAG, "WriteBinary invalid argument");
        return 1U;
    }

    uint32_t ret = OhosLfsPortInit();
    if (ret != 0U) {
        return ret;
    }

    ret = OhosLfsPortEnsureParentDir(path);
    if (ret != 0U) {
        return ret;
    }

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        ESP_LOGE(OHOS_LFS_TAG, "Open for binary write failed path=%s errno=%d", path, errno);
        return 2U;
    }

    size_t written = (len > 0U) ? fwrite(data, 1, len, fp) : 0U;
    int flushRet = fflush(fp);
    int closeRet = fclose(fp);

    if (written != len || flushRet != 0 || closeRet != 0) {
        ESP_LOGE(OHOS_LFS_TAG,
                 "Binary write failed path=%s written=%u expected=%u flush=%d close=%d errno=%d",
                 path,
                 (unsigned)written,
                 (unsigned)len,
                 flushRet,
                 closeRet,
                 errno);
        return 3U;
    }

    ESP_LOGI(OHOS_LFS_TAG, "Binary write ok path=%s bytes=%u", path, (unsigned)written);
    return 0U;
}

uint32_t OhosLfsPortReadTextWithLen(const char *path, char *buf, size_t buf_len, size_t *out_len)
{
    if (path == NULL || buf == NULL || buf_len == 0) {
        ESP_LOGE(OHOS_LFS_TAG, "ReadText invalid argument");
        return 1;
    }

    if (out_len != NULL) {
        *out_len = 0;
    }

    uint32_t ret = OhosLfsPortInit();
    if (ret != 0) {
        return ret;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        ESP_LOGE(OHOS_LFS_TAG, "Open for read failed path=%s errno=%d", path, errno);
        return 2;
    }

    size_t n = fread(buf, 1, buf_len - 1, fp);
    int readErr = ferror(fp);
    fclose(fp);

    buf[n] = '\0';
    if (out_len != NULL) {
        *out_len = n;
    }

    if (readErr != 0) {
        ESP_LOGE(OHOS_LFS_TAG, "Read failed path=%s errno=%d", path, errno);
        return 3;
    }

    ESP_LOGI(OHOS_LFS_TAG, "Read ok path=%s bytes=%u", path, (unsigned)n);
    return 0;
}

uint32_t OhosLfsPortReadText(const char *path, char *buf, size_t buf_len)
{
    return OhosLfsPortReadTextWithLen(path, buf, buf_len, NULL);
}

uint32_t OhosLfsPortGetFileSize(const char *path, size_t *out_size)
{
    struct stat st;
    uint32_t ret;

    if (path == NULL || out_size == NULL) {
        ESP_LOGE(OHOS_LFS_TAG, "GetFileSize invalid argument");
        return 1U;
    }

    *out_size = 0U;
    ret = OhosLfsPortInit();
    if (ret != 0U) {
        return ret;
    }

    if (stat(path, &st) != 0) {
        ESP_LOGE(OHOS_LFS_TAG, "stat failed path=%s errno=%d", path, errno);
        return 2U;
    }

    if (st.st_size < 0) {
        ESP_LOGE(OHOS_LFS_TAG, "stat invalid size path=%s size=%ld", path, (long)st.st_size);
        return 3U;
    }

    *out_size = (size_t)st.st_size;
    return 0U;
}

uint32_t OhosLfsPortDelete(const char *path)
{
    if (path == NULL) {
        ESP_LOGE(OHOS_LFS_TAG, "Delete invalid argument");
        return 1;
    }

    uint32_t ret = OhosLfsPortInit();
    if (ret != 0) {
        return ret;
    }

    if (remove(path) != 0) {
        ESP_LOGW(OHOS_LFS_TAG,
                 "Delete failed or file not exist path=%s errno=%d",
                 path,
                 errno);
        return 2;
    }

    ESP_LOGI(OHOS_LFS_TAG, "Delete ok path=%s", path);
    return 0;
}

uint32_t OhosLfsPortFileExists(const char *path)
{
    struct stat st;
    uint32_t ret;

    if (path == NULL) {
        return 0U;
    }

    ret = OhosLfsPortInit();
    if (ret != 0U) {
        return 0U;
    }

    return (stat(path, &st) == 0) ? 1U : 0U;
}

uint32_t OhosLfsPortSelfTest(void)
{
    const char *path = OHOS_LFS_BASE_PATH "/ohos_lfs_selftest.txt";
    const char *text = "OpenHarmony LiteOS-M LittleFS selftest OK\n";
    char read_buf[128] = {0};

    ESP_LOGI(OHOS_LFS_TAG, "LittleFS selftest start");

    uint32_t ret = OhosLfsPortInit();
    if (ret != 0) {
        ESP_LOGE(OHOS_LFS_TAG, "Selftest init failed ret=%u", (unsigned)ret);
        return ret;
    }

    ret = OhosLfsPortWriteText(path, text);
    if (ret != 0) {
        ESP_LOGE(OHOS_LFS_TAG, "Selftest write failed ret=%u", (unsigned)ret);
        return ret;
    }

    ret = OhosLfsPortReadText(path, read_buf, sizeof(read_buf));
    if (ret != 0) {
        ESP_LOGE(OHOS_LFS_TAG, "Selftest read failed ret=%u", (unsigned)ret);
        return ret;
    }

    if (strcmp(read_buf, text) != 0) {
        ESP_LOGE(OHOS_LFS_TAG, "Selftest verify failed read='%s'", read_buf);
        return 4;
    }

    ESP_LOGI(OHOS_LFS_TAG, "LittleFS selftest success path=%s", path);
    return 0;
}
