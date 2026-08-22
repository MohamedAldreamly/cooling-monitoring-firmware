#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "../../components/firmware_core/include/app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIMULATION_SCENARIO_NORMAL = 0,
    SIMULATION_SCENARIO_FULL_SYSTEM,
    SIMULATION_SCENARIO_SENSOR_FAILURE,
    SIMULATION_SCENARIO_WATER_LEAK,
    SIMULATION_SCENARIO_POWER_FAILURE,
    SIMULATION_SCENARIO_STORAGE_PIPELINE_TEST
} simulation_scenario_t;

typedef struct {
    uint32_t generated_samples;
    uint32_t queue_failures;
    uint32_t communication_faults_generated;
    uint32_t sensor_faults_generated;
    uint32_t current_cycle;

    simulation_scenario_t active_scenario;
    bool running;
} simulation_engine_status_t;

/**
 * @brief Initializes the simulation state.
 */
esp_err_t simulation_engine_init(simulation_scenario_t scenario);

/**
 * @brief Starts the FreeRTOS simulation task.
 */
esp_err_t simulation_engine_start(void);

/**
 * @brief Stops generation of new samples.
 */
esp_err_t simulation_engine_stop(void);

/**
 * @brief Changes the active scenario.
 */
esp_err_t simulation_engine_set_scenario(
    simulation_scenario_t scenario
);

/**
 * @brief Returns current engine status.
 */
esp_err_t simulation_engine_get_status(
    simulation_engine_status_t *out
);

/**
 * @brief Returns whether the simulation task is running.
 */
bool simulation_engine_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
