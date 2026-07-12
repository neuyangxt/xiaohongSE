/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "linux/videodev2.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
#include "esp_heap_caps.h"

#define MEMORY_TYPE V4L2_MEMORY_USERPTR
#define MEMORY_ALIGN 64
#else
#define MEMORY_TYPE V4L2_MEMORY_MMAP
#endif

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
#define CAM_DEV_PATH ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#elif CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
#define CAM_DEV_PATH ESP_VIDEO_DVP_DEVICE_NAME
#endif

#define BUFFER_COUNT 2
#define CAPTURE_SECONDS 3

static const char *TAG = "example";

extern void OhosCameraPreviewShowRgb565Frame(const uint8_t *rgb565, uint32_t src_w, uint32_t src_h, uint32_t src_stride);
extern void OhosCameraPreviewShowRgb888Frame(const uint8_t *rgb888, uint32_t src_w, uint32_t src_h, uint32_t src_stride);

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
static const esp_video_init_csi_config_t csi_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
            },
            .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    },
};
#endif

#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
static const esp_video_init_dvp_config_t dvp_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN,
            },
            .freq      = CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN,
        .dvp_pin = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                CONFIG_EXAMPLE_DVP_D0_PIN, CONFIG_EXAMPLE_DVP_D1_PIN, CONFIG_EXAMPLE_DVP_D2_PIN, CONFIG_EXAMPLE_DVP_D3_PIN,
                CONFIG_EXAMPLE_DVP_D4_PIN, CONFIG_EXAMPLE_DVP_D5_PIN, CONFIG_EXAMPLE_DVP_D6_PIN, CONFIG_EXAMPLE_DVP_D7_PIN,
            },
            .vsync_io = CONFIG_EXAMPLE_DVP_VSYNC_PIN,
            .de_io = CONFIG_EXAMPLE_DVP_DE_PIN,
            .pclk_io = CONFIG_EXAMPLE_DVP_PCLK_PIN,
            .xclk_io = CONFIG_EXAMPLE_DVP_XCLK_PIN,
        },
        .xclk_freq = CONFIG_EXAMPLE_DVP_XCLK_FREQ,
    },
};
#endif

static const esp_video_init_config_t cam_config = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    .csi      = csi_config,
#endif
#if CONFIG_EXAMPLE_ENABLE_DVP_CAM_SENSOR
    .dvp      = dvp_config,
#endif
};

static esp_err_t camera_capture_stream(void)
{
    int fd;
    esp_err_t ret;
    int fmt_index = 0;
    uint32_t frame_size;
    uint32_t frame_count;
    struct v4l2_buffer buf;
    uint8_t *buffer[BUFFER_COUNT];
#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
    uint32_t buffer_size[BUFFER_COUNT];
#endif
    struct v4l2_format init_format;
    struct v4l2_requestbuffers req;
    struct v4l2_capability capability;
#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP || CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
#endif
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fd = open(CAM_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "failed to open device");
        return ESP_FAIL;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        ret = ESP_FAIL;
        goto exit_0;
    }

    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);
    ESP_LOGI(TAG, "capabilities:");
    if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability.capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGI(TAG, "\tREADWRITE");
    }
    if (capability.capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGI(TAG, "\tASYNCIO");
    }
    if (capability.capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGI(TAG, "\tSTREAMING");
    }
    if (capability.capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGI(TAG, "\tMETA_OUTPUT");
    }
    if (capability.capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGI(TAG, "device capabilities:");
        if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability.device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGI(TAG, "\tREADWRITE");
        }
        if (capability.device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGI(TAG, "\tASYNCIO");
        }
        if (capability.device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGI(TAG, "\tSTREAMING");
        }
        if (capability.device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGI(TAG, "\tMETA_OUTPUT");
        }
    }

    memset(&init_format, 0, sizeof(struct v4l2_format));
    init_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        ret = ESP_FAIL;
        goto exit_0;
    }

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_VFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }
#endif

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_HFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }
#endif

    while (1) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = fmt_index++,
            .type = type,
        };

        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            break;
        }

        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = init_format.fmt.pix.width,
            .fmt.pix.height = init_format.fmt.pix.height,
            .fmt.pix.pixelformat = fmtdesc.pixelformat,
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            if (errno == ESRCH) {
                continue;
            } else {
                ESP_LOGE(TAG, "failed to set format");
                ret = ESP_FAIL;
                goto exit_0;
            }
        }

        bool is_rgb888_preview_format = (strstr((const char *)fmtdesc.description, "RGB 8-8-8") != NULL);
        int capture_seconds_this_format = is_rgb888_preview_format ? 300 : CAPTURE_SECONDS;

        ESP_LOGI(TAG, "Capture %s format frames for %d seconds:", (char *)fmtdesc.description, capture_seconds_this_format);
        uint32_t preview_frame_seq = 0;


        memset(&req, 0, sizeof(req));
        req.count  = BUFFER_COUNT;
        req.type   = type;
        req.memory = MEMORY_TYPE;
        if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0) {
            ESP_LOGE(TAG, "failed to require buffer");
            ret = ESP_FAIL;
            goto exit_0;
        }

        for (int i = 0; i < BUFFER_COUNT; i++) {
            struct v4l2_buffer buf;

            memset(&buf, 0, sizeof(buf));
            buf.type        = type;
            buf.memory      = MEMORY_TYPE;
            buf.index       = i;
            if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to query buffer");
                ret = ESP_FAIL;
                goto exit_0;
            }

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
            buffer[i] = heap_caps_aligned_alloc(MEMORY_ALIGN, buf.length, MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
#else
            buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, buf.m.offset);
#endif
            if (!buffer[i]) {
                ESP_LOGE(TAG, "failed to map buffer");
                ret = ESP_FAIL;
                goto exit_0;
            }
#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
            else {
                buf.m.userptr = (unsigned long)buffer[i];
                buffer_size[i] = buf.length;
            }
#endif

            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }
        }

        if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
            ESP_LOGE(TAG, "failed to start stream");
            ret = ESP_FAIL;
            goto exit_0;
        }

        frame_count = 0;
        frame_size = 0;
        int64_t start_time_us = esp_timer_get_time();
        while (esp_timer_get_time() - start_time_us < ((int64_t)capture_seconds_this_format * 1000LL * 1000LL)) {
            memset(&buf, 0, sizeof(buf));
            buf.type   = type;
            buf.memory = MEMORY_TYPE;
            if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to receive video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }

            frame_size += buf.bytesused;

            if (is_rgb888_preview_format && buf.bytesused >= (format.fmt.pix.width * format.fmt.pix.height * 3)) {
                preview_frame_seq++;
                if ((preview_frame_seq % 5U) == 1U) {
#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
                    const uint8_t *frame_ptr = buffer[buf.index];
#else
                    const uint8_t *frame_ptr = buffer[buf.index];
#endif
                    ESP_LOGI(TAG, "Send RGB888 camera frame to LVGL live preview seq=%u", (unsigned)preview_frame_seq);
                    OhosCameraPreviewShowRgb888Frame(frame_ptr,
                                                     format.fmt.pix.width,
                                                     format.fmt.pix.height,
                                                     format.fmt.pix.width * 3);
                }
            }

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
            buf.m.userptr = (unsigned long)buffer[buf.index];
            buf.length = buffer_size[buf.index];
#endif
            if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "failed to queue video frame");
                ret = ESP_FAIL;
                goto exit_0;
            }

            frame_count++;
        }

        if (ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "failed to stop stream");
            ret = ESP_FAIL;
            goto exit_0;
        }

#if CONFIG_EXAMPLE_VIDEO_BUFFER_TYPE_USER
        for (int i = 0; i < BUFFER_COUNT; i++) {
            heap_caps_free(buffer[i]);
        }
#endif

        ESP_LOGI(TAG, "\twidth:  %" PRIu32, format.fmt.pix.width);
        ESP_LOGI(TAG, "\theight: %" PRIu32, format.fmt.pix.height);
        ESP_LOGI(TAG, "\tsize:   %" PRIu32, frame_size / frame_count);
        ESP_LOGI(TAG, "\tFPS:    %" PRIu32, frame_count / (uint32_t)capture_seconds_this_format);
    }

    ret = ESP_OK;

exit_0:
    close(fd);
    return ret;
}

void OhosCameraCaptureStreamProbe(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "camera power settle delay before esp_video_init");
    vTaskDelay(pdMS_TO_TICKS(1500));

    ret = esp_video_init(&cam_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return;
    }

    ret = camera_capture_stream();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera capture stream failed with error 0x%x", ret);
        return;
    }
}
