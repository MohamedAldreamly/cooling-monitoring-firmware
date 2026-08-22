#ifndef PIPELINE_TEST_MONITOR_H
#define PIPELINE_TEST_MONITOR_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Captures pipeline baselines and starts the E2E result monitor.
 *
 * The monitor is observational only. It never creates records and never
 * consumes an application queue.
 */
esp_err_t pipeline_test_monitor_start(void);

#ifdef __cplusplus
}
#endif

#endif
