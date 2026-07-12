/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_err.h"
#include "esp_log.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "ohos_liteos_media_task.h"
#include "osptek_app_video.h"

static const char *TAG = "app_video";

#define MAX_BUFFER_COUNT                (3)
#define MIN_BUFFER_COUNT                (2)
#define VIDEO_TASK_STACK_SIZE           (4 * 1024)
#define VIDEO_TASK_PRIORITY             (26)
#define OHOS_CAMERA_SENSOR_VFLIP        1
#define OHOS_CAMERA_SENSOR_HFLIP        1
#define OHOS_CAMERA_TARGET_FPS          30

typedef struct {
    uint8_t *camera_buffer[MAX_BUFFER_COUNT];
    size_t camera_buf_size;
    uint8_t camera_buf_count;
    uint32_t camera_buf_hes;
    uint32_t camera_buf_ves;
    struct v4l2_buffer v4l2_buf;
    uint8_t camera_mem_mode;
    osptek_video_frame_operation_cb_t user_camera_video_frame_operation_cb;
    UINT32 video_stream_task_id;
    uint8_t video_task_core_id;
    volatile bool video_task_delete;
    volatile bool video_task_running;
    void *video_task_user_data;
} app_video_t;

static app_video_t s_osptek_camera_video;
static volatile bool s_osptek_video_hw_ready;

static bool osptek_video_device_exists(const char *dev)
{
    int fd = open(dev, O_RDONLY);

    if (fd < 0) {
        return false;
    }

    close(fd);
    return true;
}

static void ohos_s64_log_sensor_format(int fd, const char *stage)
{
    esp_cam_sensor_format_t sensor_format;
    memset(&sensor_format, 0, sizeof(sensor_format));

    if (ioctl(fd, VIDIOC_G_SENSOR_FMT, &sensor_format) != 0) {
        ESP_LOGW(TAG, "S64E %s G_SENSOR_FMT failed", stage);
        return;
    }

    ESP_LOGI(TAG,
             "S64E %s sensor fmt name=%s size=%ux%u fps=%u pix=%u mipi_clk=%u lane=%u isp=%p",
             stage,
             sensor_format.name ? sensor_format.name : "(null)",
             (unsigned)sensor_format.width,
             (unsigned)sensor_format.height,
             (unsigned)sensor_format.fps,
             (unsigned)sensor_format.format,
             (unsigned)sensor_format.mipi_info.mipi_clk,
             (unsigned)sensor_format.mipi_info.lane_num,
             sensor_format.isp_info);
}

static void ohos_s64_log_video_format(int fd, const char *stage)
{
    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGW(TAG, "S64E %s G_FMT failed", stage);
        return;
    }

    ESP_LOGI(TAG,
             "S64E %s video fmt size=%" PRIu32 "x%" PRIu32 " pix=0x%08" PRIx32 " bytesperline=%" PRIu32 " sizeimage=%" PRIu32,
             stage,
             format.fmt.pix.width,
             format.fmt.pix.height,
             format.fmt.pix.pixelformat,
             format.fmt.pix.bytesperline,
             format.fmt.pix.sizeimage);
}

static void ohos_s64_log_stream_parm(int fd, const char *stage)
{
    struct v4l2_streamparm sparm;
    memset(&sparm, 0, sizeof(sparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_G_PARM, &sparm) != 0) {
        ESP_LOGW(TAG, "S64E %s G_PARM failed", stage);
        return;
    }

    const struct v4l2_fract *tpf = &sparm.parm.capture.timeperframe;
    uint32_t fps_x10 = 0;
    if (tpf->numerator != 0) {
        fps_x10 = (tpf->denominator * 10U) / tpf->numerator;
    }

    ESP_LOGI(TAG,
             "S64E %s stream parm capability=0x%08" PRIx32 " timeperframe=%" PRIu32 "/%" PRIu32 " fps=%u.%u",
             stage,
             sparm.parm.capture.capability,
             tpf->numerator,
             tpf->denominator,
             (unsigned)(fps_x10 / 10U),
             (unsigned)(fps_x10 % 10U));
}

static esp_err_t ohos_s64_force_stream_fps(int fd, uint32_t fps)
{
    struct v4l2_streamparm sparm;
    memset(&sparm, 0, sizeof(sparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sparm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
    sparm.parm.capture.timeperframe.numerator = 1;
    sparm.parm.capture.timeperframe.denominator = fps;

    int ret = ioctl(fd, VIDIOC_S_PARM, &sparm);
    ESP_LOGI(TAG, "S64E force stream fps=%u S_PARM ret=%d", (unsigned)fps, ret);
    if (ret != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t osptek_video_main(i2c_master_bus_handle_t i2c_bus_handle)
{
    bool cam_dev_ready = osptek_video_device_exists(EXAMPLE_CAM_DEV_PATH);
    bool isp_dev_ready = osptek_video_device_exists(ESP_VIDEO_ISP1_DEVICE_NAME);

    if (s_osptek_video_hw_ready || cam_dev_ready) {
        s_osptek_video_hw_ready = true;
        ESP_LOGI(TAG, "S64E video device already ready path=%s", EXAMPLE_CAM_DEV_PATH);
        return ESP_OK;
    }

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
    esp_video_init_csi_config_t csi_config[] = {
        {
            .sccb_config = {
                .init_sccb = true,
                .i2c_config = {
                    .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                    .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                    .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
                },
                .freq      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
            },
            .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
            .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
        },
    };

    if (i2c_bus_handle != NULL) {
        csi_config[0].sccb_config.init_sccb = false;
        csi_config[0].sccb_config.i2c_handle = i2c_bus_handle;
    }
#endif

    esp_video_init_config_t cam_config = {
#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR > 0
        .csi      = csi_config,
#endif
    };

    esp_err_t ret;

    if (isp_dev_ready) {
        ret = esp_video_init_with_flags(&cam_config, ESP_VIDEO_INIT_FLAGS_MIPI_CSI);
        ESP_LOGI(TAG,
                 "S64E reuse existing ISP path=%s, init MIPI-CSI ret=%s",
                 ESP_VIDEO_ISP1_DEVICE_NAME,
                 esp_err_to_name(ret));
    } else {
        ret = esp_video_init_with_flags(&cam_config,
                                        ESP_VIDEO_INIT_FLAGS_ISP |
                                        ESP_VIDEO_INIT_FLAGS_MIPI_CSI);
        ESP_LOGI(TAG,
                 "S64E esp_video_init ISP+MIPI-CSI ret=%s",
                 esp_err_to_name(ret));
    }

    if (ret == ESP_OK || osptek_video_device_exists(EXAMPLE_CAM_DEV_PATH)) {
        s_osptek_video_hw_ready = true;
        ESP_LOGI(TAG,
                 "S64E video init usable ret=%s path=%s",
                 esp_err_to_name(ret),
                 EXAMPLE_CAM_DEV_PATH);
        return ESP_OK;
    }

    if (!isp_dev_ready && osptek_video_device_exists(ESP_VIDEO_ISP1_DEVICE_NAME)) {
        ret = esp_video_init_with_flags(&cam_config, ESP_VIDEO_INIT_FLAGS_MIPI_CSI);
        ESP_LOGI(TAG,
                 "S64E full init left ISP registered, retry MIPI-CSI ret=%s",
                 esp_err_to_name(ret));
        if (ret == ESP_OK || osptek_video_device_exists(EXAMPLE_CAM_DEV_PATH)) {
            s_osptek_video_hw_ready = true;
            return ESP_OK;
        }
    }

    return ret;
}

int osptek_video_open(char *dev, video_fmt_t init_fmt)
{
    struct v4l2_format default_format;
    struct v4l2_capability capability;
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP || CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
#endif

    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open video failed");
        return -1;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        goto exit_0;
    }

    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);
    ohos_s64_log_sensor_format(fd, "open-before-format");
    ohos_s64_log_stream_parm(fd, "open-before-format");

    memset(&default_format, 0, sizeof(struct v4l2_format));
    default_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &default_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        goto exit_0;
    }

    ESP_LOGI(TAG, "width=%" PRIu32 " height=%" PRIu32, default_format.fmt.pix.width, default_format.fmt.pix.height);
    ohos_s64_log_video_format(fd, "open-default");

    if (default_format.fmt.pix.pixelformat != init_fmt) {
        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = default_format.fmt.pix.width,
            .fmt.pix.height = default_format.fmt.pix.height,
            .fmt.pix.pixelformat = init_fmt,
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            ESP_LOGE(TAG, "failed to set format");
            goto exit_0;
        }
    }

    ohos_s64_log_video_format(fd, "open-after-sfmt");
    ohos_s64_log_stream_parm(fd, "open-before-sparm");
    if (ohos_s64_force_stream_fps(fd, OHOS_CAMERA_TARGET_FPS) != ESP_OK) {
        ESP_LOGW(TAG, "S64E force stream fps failed, continue with driver default");
    }
    ohos_s64_log_stream_parm(fd, "open-after-sparm");
    ohos_s64_log_sensor_format(fd, "open-after-sparm");

    struct v4l2_format active_format;
    memset(&active_format, 0, sizeof(active_format));
    active_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &active_format) != 0) {
        ESP_LOGE(TAG, "failed to get active format");
        goto exit_0;
    }

    s_osptek_camera_video.camera_buf_hes = active_format.fmt.pix.width;
    s_osptek_camera_video.camera_buf_ves = active_format.fmt.pix.height;

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP || CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    ESP_LOGI(TAG,
             "OHOS camera sensor flip override vflip=%d hflip=%d sdk_vflip=%d sdk_hflip=%d",
             OHOS_CAMERA_SENSOR_VFLIP,
             OHOS_CAMERA_SENSOR_HFLIP,
             CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP,
             CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP);
#endif

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_VFLIP
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_VFLIP;
    control[0].value    = OHOS_CAMERA_SENSOR_VFLIP;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set VFLIP and skip this step");
    }
#endif

#if CONFIG_EXAMPLE_ENABLE_CAM_SENSOR_PIC_HFLIP
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_HFLIP;
    control[0].value    = OHOS_CAMERA_SENSOR_HFLIP;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set HFLIP and skip this step");
    }
#endif

    return fd;
exit_0:
    close(fd);
    return -1;
}

esp_err_t osptek_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb)
{
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num is too large");
        return ESP_FAIL;
    } else if (fb_num < MIN_BUFFER_COUNT) {
        ESP_LOGE(TAG, "At least two buffers are required");
        return ESP_FAIL;
    }

    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    memset(&req, 0, sizeof(req));
    req.count = fb_num;
    s_osptek_camera_video.camera_buf_count = fb_num;
    req.type = type;

    s_osptek_camera_video.camera_mem_mode = req.memory = fb ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;

    if (ioctl(video_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "req bufs failed");
        goto errout_req_bufs;
    }
    for (int i = 0; i < fb_num; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = req.memory;
        buf.index = i;

        if (ioctl(video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "query buf failed");
            goto errout_req_bufs;
        }

        if (req.memory == V4L2_MEMORY_MMAP) {
            s_osptek_camera_video.camera_buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd, buf.m.offset);
            if (s_osptek_camera_video.camera_buffer[i] == NULL) {
                ESP_LOGE(TAG, "mmap failed");
                goto errout_req_bufs;
            }
        } else {
            if (!fb[i]) {
                ESP_LOGE(TAG, "frame buffer is NULL");
                goto errout_req_bufs;
            }
            buf.m.userptr = (unsigned long)fb[i];
            s_osptek_camera_video.camera_buffer[i] = (uint8_t *)fb[i];
        }

        s_osptek_camera_video.camera_buf_size = buf.length;

        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "queue frame buffer failed");
            goto errout_req_bufs;
        }
    }

    return ESP_OK;

errout_req_bufs:
    close(video_fd);
    return ESP_FAIL;
}

esp_err_t osptek_video_get_bufs(int fb_num, void **fb)
{
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num is too large");
        return ESP_FAIL;
    } else if (fb_num < MIN_BUFFER_COUNT) {
        ESP_LOGE(TAG, "At least two buffers are required");
        return ESP_FAIL;
    }

    for (int i = 0; i < fb_num; i++) {
        if (s_osptek_camera_video.camera_buffer[i] != NULL) {
            fb[i] = s_osptek_camera_video.camera_buffer[i];
        } else {
            ESP_LOGE(TAG, "frame buffer is NULL");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

uint32_t osptek_video_get_buf_size(void)
{
    uint32_t buf_size = s_osptek_camera_video.camera_buf_hes * s_osptek_camera_video.camera_buf_ves * (OSPTEK_VIDEO_FMT == OSPTEK_VIDEO_FMT_RGB565 ? 2 : 3);

    return buf_size;
}

static inline esp_err_t video_receive_video_frame(int video_fd)
{
    memset(&s_osptek_camera_video.v4l2_buf, 0, sizeof(s_osptek_camera_video.v4l2_buf));
    s_osptek_camera_video.v4l2_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    s_osptek_camera_video.v4l2_buf.memory = s_osptek_camera_video.camera_mem_mode;

    int res = ioctl(video_fd, VIDIOC_DQBUF, &(s_osptek_camera_video.v4l2_buf));
    if (res != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static inline void video_operation_video_frame(int video_fd)
{
    s_osptek_camera_video.v4l2_buf.m.userptr = (unsigned long)s_osptek_camera_video.camera_buffer[s_osptek_camera_video.v4l2_buf.index];
    s_osptek_camera_video.v4l2_buf.length = s_osptek_camera_video.camera_buf_size;

    uint8_t buf_index = s_osptek_camera_video.v4l2_buf.index;

    s_osptek_camera_video.user_camera_video_frame_operation_cb(
                        s_osptek_camera_video.camera_buffer[buf_index],
                        buf_index,
                        s_osptek_camera_video.camera_buf_hes,
                        s_osptek_camera_video.camera_buf_ves,
                        s_osptek_camera_video.camera_buf_size,
                        s_osptek_camera_video.video_task_user_data
                    );
}

static inline esp_err_t video_free_video_frame(int video_fd)
{
    if (ioctl(video_fd, VIDIOC_QBUF, &(s_osptek_camera_video.v4l2_buf)) != 0) {
        ESP_LOGE(TAG, "failed to free video frame");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static inline esp_err_t video_stream_start(int video_fd)
{
    ESP_LOGI(TAG, "Video Stream Start");

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        ESP_LOGE(TAG, "failed to start stream");
        goto errout;
    }

    ohos_s64_log_video_format(video_fd, "stream-start");
    ohos_s64_log_stream_parm(video_fd, "stream-start");

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static inline esp_err_t video_stream_stop(int video_fd)
{
    ESP_LOGI(TAG, "Video Stream Stop");

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMOFF, &type)) {
        ESP_LOGE(TAG, "failed to stop stream");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

static void video_stream_task(void *arg)
{
    int video_fd = (int)(intptr_t)arg;

    s_osptek_camera_video.video_task_running = true;
    ESP_LOGI(TAG, "S64C video stream task enter fd=%d", video_fd);

    while (1) {
        if (s_osptek_camera_video.video_task_delete) {
            break;
        }

        uint32_t loop_start_ms = (uint32_t)esp_log_timestamp();
        esp_err_t recv_ret = video_receive_video_frame(video_fd);
        uint32_t recv_done_ms = (uint32_t)esp_log_timestamp();
        if (recv_ret != ESP_OK) {
            if (s_osptek_camera_video.video_task_delete) {
                break;
            }
            static uint32_t s_s64c_recv_fail_cnt = 0;
            if ((s_s64c_recv_fail_cnt++ % 30U) == 0U) {
                ESP_LOGW(TAG, "S64C receive video frame failed ret=%s fd=%d, retry",
                         esp_err_to_name(recv_ret), video_fd);
            }
            OhosLiteosDelayMs(10);
            continue;
        }

        if (s_osptek_camera_video.video_task_delete) {
            (void)video_free_video_frame(video_fd);
            break;
        }

        video_operation_video_frame(video_fd);
        uint32_t op_done_ms = (uint32_t)esp_log_timestamp();

        esp_err_t free_ret = video_free_video_frame(video_fd);
        uint32_t free_done_ms = (uint32_t)esp_log_timestamp();
        if (free_ret != ESP_OK) {
            if (s_osptek_camera_video.video_task_delete) {
                break;
            }
            ESP_LOGW(TAG, "S64C free video frame failed ret=%s fd=%d",
                     esp_err_to_name(free_ret), video_fd);
            OhosLiteosDelayMs(10);
            continue;
        }

        if (s_osptek_camera_video.video_task_delete) {
            break;
        }

        static uint32_t s_s64d_stream_start_ms = 0;
        static uint32_t s_s64d_stream_frames = 0;
        static uint64_t s_s64d_wait_sum_ms = 0;
        static uint64_t s_s64d_cb_sum_ms = 0;
        static uint64_t s_s64d_qbuf_sum_ms = 0;
        uint32_t now_ms = free_done_ms;
        if (s_s64d_stream_start_ms == 0) {
            s_s64d_stream_start_ms = now_ms;
        }
        s_s64d_stream_frames++;
        s_s64d_wait_sum_ms += (uint32_t)(recv_done_ms - loop_start_ms);
        s_s64d_cb_sum_ms += (uint32_t)(op_done_ms - recv_done_ms);
        s_s64d_qbuf_sum_ms += (uint32_t)(free_done_ms - op_done_ms);
        uint32_t elapsed_ms = (uint32_t)(now_ms - s_s64d_stream_start_ms);
        if (elapsed_ms >= 3000U) {
            uint32_t fps_x10 = (s_s64d_stream_frames * 10000U) / elapsed_ms;
            ESP_LOGI(TAG,
                     "S64D camera stream fps=%u.%u frames=%u elapsed=%ums wait_avg=%ums cb_avg=%ums qbuf_avg=%ums",
                     (unsigned)(fps_x10 / 10U),
                     (unsigned)(fps_x10 % 10U),
                     (unsigned)s_s64d_stream_frames,
                     (unsigned)elapsed_ms,
                     (unsigned)(s_s64d_wait_sum_ms / s_s64d_stream_frames),
                     (unsigned)(s_s64d_cb_sum_ms / s_s64d_stream_frames),
                     (unsigned)(s_s64d_qbuf_sum_ms / s_s64d_stream_frames));
            s_s64d_stream_start_ms = now_ms;
            s_s64d_stream_frames = 0;
            s_s64d_wait_sum_ms = 0;
            s_s64d_cb_sum_ms = 0;
            s_s64d_qbuf_sum_ms = 0;
        }

        (void)LOS_TaskYield();
    }

    s_osptek_camera_video.video_task_delete = false;
    esp_err_t stop_ret = video_stream_stop(video_fd);
    s_osptek_camera_video.video_task_running = false;
    ESP_LOGI(TAG, "S64C video stream task exit fd=%d stop_ret=%s",
             video_fd, esp_err_to_name(stop_ret));
    return;
}

esp_err_t osptek_video_stream_task_start(int video_fd, int core_id, void *user_data)
{
    s_osptek_camera_video.video_task_core_id = core_id;
    s_osptek_camera_video.video_task_user_data = user_data;
    s_osptek_camera_video.video_task_delete = false;
    s_osptek_camera_video.video_task_running = true;

    video_stream_start(video_fd);

    UINT32 result = OhosLiteosCreateTask("video_stream",
                                         video_stream_task,
                                         (void *)(intptr_t)video_fd,
                                         VIDEO_TASK_PRIORITY,
                                         VIDEO_TASK_STACK_SIZE,
                                         &s_osptek_camera_video.video_stream_task_id);

    if (result != LOS_OK) {
        ESP_LOGE(TAG, "failed to create video stream task");
        s_osptek_camera_video.video_task_running = false;
        goto errout;
    }

    return ESP_OK;

errout:
    video_stream_stop(video_fd);
    return ESP_FAIL;
}

esp_err_t osptek_video_stream_task_restart(int video_fd)
{
    osptek_video_set_bufs(video_fd, s_osptek_camera_video.camera_buf_count, (const void **)s_osptek_camera_video.camera_buffer);

    esp_err_t ret = osptek_video_stream_task_start(video_fd, s_osptek_camera_video.video_task_core_id, s_osptek_camera_video.video_task_user_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to restart video stream task");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

esp_err_t osptek_video_stream_task_stop(int video_fd)
{
    ESP_LOGI(TAG, "S67F video stream task stop request fd=%d running=%d delete=%d",
             video_fd,
             (int)s_osptek_camera_video.video_task_running,
             (int)s_osptek_camera_video.video_task_delete);

    s_osptek_camera_video.video_task_delete = true;

    for (int wait_i = 0; wait_i < 60; ++wait_i) {
        if (!s_osptek_camera_video.video_task_running) {
            ESP_LOGI(TAG, "S67F video stream task stopped fd=%d wait=%d0ms",
                     video_fd, wait_i);
            return ESP_OK;
        }
        OhosLiteosDelayMs(10);
    }

    ESP_LOGW(TAG, "S67F video stream task stop timeout fd=%d running=%d",
             video_fd, (int)s_osptek_camera_video.video_task_running);
    return ESP_ERR_TIMEOUT;
}

esp_err_t osptek_video_register_frame_operation_cb(osptek_video_frame_operation_cb_t operation_cb)
{
    s_osptek_camera_video.user_camera_video_frame_operation_cb = operation_cb;

    return ESP_OK;
}
