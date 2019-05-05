/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include "wifi_provision_internal.h"

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#define AWSS_PRESS_TIMEOUT_MS  (60000)
#define AHA_MONITOR_TIMEOUT_MS  (1 * 60 * 1000)

extern int switch_ap_done;
static uint8_t awss_stopped = 1;
static uint8_t g_user_press = 0;
static void *press_timer = NULL;

static void awss_press_timeout(void);

int awss_success_notify(void)
{
    g_user_press = 0;
    awss_press_timeout();

    awss_cmp_local_init(AWSS_LC_INIT_SUC);
    awss_suc_notify_stop();
    awss_suc_notify();
    awss_start_connectap_monitor();
    AWSS_DISP_STATIS();
    return 0;
}

char awss_aha_connect_to_router()
{
    int iter = 0;
    char dest_ap = 0;
    char ssid[PLATFORM_MAX_SSID_LEN + 1] = {0};
    int count = AHA_MONITOR_TIMEOUT_MS / 50;
    for (iter = 0; iter < count; iter++) {
        memset(ssid, 0, sizeof(ssid));
        HAL_Wifi_Get_Ap_Info(ssid, NULL, NULL);
        if (HAL_Sys_Net_Is_Ready() &&
            strlen(ssid) > 0 && strcmp(ssid, DEFAULT_SSID)) {  /* not AHA */
            dest_ap = 1;
            break;
        }
        if (awss_stopped) {
            break;
        }
        HAL_SleepMs(50);
    }
    return dest_ap;
}

int awss_start(void)
{
    if (awss_stopped == 0) {
        awss_debug("awss already running\n");
        return -1;
    }

    awss_stopped = 0;
    awss_event_post(IOTX_AWSS_START);
    produce_random(aes_random, sizeof(aes_random));

    do {
        __awss_start();
#if defined(AWSS_SUPPORT_AHA)
        do {
            char ssid[PLATFORM_MAX_SSID_LEN + 1] = {0};
            if (awss_stopped) {
                break;
            }

            if (switch_ap_done) {
                break;
            }

            HAL_Wifi_Get_Ap_Info(ssid, NULL, NULL);
            if (strlen(ssid) > 0 && strcmp(ssid, DEFAULT_SSID)) { /* not AHA */
                break;
            }

            if (HAL_Sys_Net_Is_Ready()) {
                char dest_ap = 0;

                awss_cmp_local_init(AWSS_LC_INIT_PAP);
                dest_ap = awss_aha_connect_to_router();

                awss_cmp_local_deinit(0);

                if (switch_ap_done || awss_stopped) {
                    break;
                }

                if (dest_ap == 1) {
                    break;
                }
            }
            awss_event_post(IOTX_AWSS_ENABLE_TIMEOUT);
            __awss_start();
        } while (1);
#endif
        if (awss_stopped) {
            break;
        }

        if (HAL_Sys_Net_Is_Ready()) {
            break;
        }
    } while (1);

    if (awss_stopped) {
        return -1;
    }

    awss_success_notify();
    awss_stopped = 1;

    return 0;
}

int awss_stop(void)
{
    awss_stopped = 1;
    awss_stop_connectap_monitor();
    g_user_press = 0;
    awss_press_timeout();

    __awss_stop();

    return 0;
}

static void awss_press_timeout(void)
{
    awss_stop_timer(press_timer);
    press_timer = NULL;
    if (g_user_press) {
        awss_event_post(IOTX_AWSS_ENABLE_TIMEOUT);
    }
    g_user_press = 0;
}

int awss_config_press(void)
{
    int timeout = HAL_Awss_Get_Timeout_Interval_Ms();

    awss_trace("enable awss\r\n");

    g_user_press = 1;

    awss_event_post(IOTX_AWSS_ENABLE);

    if (press_timer == NULL) {
        press_timer = HAL_Timer_Create("press", (void (*)(void *))awss_press_timeout, NULL);
    }
    if (press_timer == NULL) {
        return -1;
    }

    HAL_Timer_Stop(press_timer);

    if (timeout < AWSS_PRESS_TIMEOUT_MS) {
        timeout = AWSS_PRESS_TIMEOUT_MS;
    }
    HAL_Timer_Start(press_timer, timeout);

    return 0;
}

uint8_t awss_get_config_press(void)
{
    return g_user_press;
}

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
}
#endif
