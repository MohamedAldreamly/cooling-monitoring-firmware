#ifndef CONNECTIVITY_TEST_CONTROLLER_H
#define CONNECTIVITY_TEST_CONTROLLER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts a one-shot E2E connectivity test:
 *
 *        30 seconds online
 *        60 seconds forced offline
 *        then restores Wi-Fi and leaves it online.
 */
esp_err_t connectivity_test_controller_start(void);

#ifdef __cplusplus
}
#endif

#endif
