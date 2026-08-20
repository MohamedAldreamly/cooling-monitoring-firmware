#include "simulation_engine.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "app_queues.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "signal_registry.h"
#include "simulation_engine_internal.h"

static const char *TAG = "SIM_ENGINE";

#define SIMULATION_TASK_NAME              "simulation"
#define SIMULATION_TASK_STACK_SIZE        4096U
#define SIMULATION_TASK_PRIORITY          5U
#define SIMULATION_LOOP_PERIOD_MS         20U
#define SIMULATION_QUEUE_TIMEOUT_MS       10U

static TaskHandle_t s_simulation_task_handle = NULL;

static simulation_engine_status_t s_status;
static simulation_process_state_t s_process_state;

static simulation_schedule_entry_t
    s_schedule[SIGNAL_ID_COUNT];

static bool s_initialized = false;
static bool s_stop_requested = false;
static uint64_t s_scenario_started_ms = 0U;
static uint8_t s_test_phase = UINT8_MAX;

static uint64_t simulation_now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static float simulation_wave(
    uint64_t uptime_ms,
    float amplitude,
    float period_seconds
)
{
    const float time_seconds = (float)uptime_ms / 1000.0f;
    const float angle =
        (2.0f * 3.14159265358979323846f * time_seconds) /
        period_seconds;

    return amplitude * sinf(angle);
}

static void simulation_reset_process_state(void)
{
    memset(&s_process_state, 0, sizeof(s_process_state));

    s_process_state.room_temperature[0] = -18.0f;
    s_process_state.room_temperature[1] = -17.7f;
    s_process_state.room_temperature[2] = -18.3f;
    s_process_state.room_temperature[3] = -17.9f;

    s_process_state.air_temperature = -17.8f;
    s_process_state.relative_humidity = 72.0f;

    s_process_state.compressor_run = true;
    s_process_state.compressor_trip = false;
    s_process_state.evaporator_fan_run = true;

    s_process_state.compressor_current = 20.0f;
    s_process_state.battery_voltage = 26.4f;

    s_process_state.door_safe = false;
    s_process_state.door_aux = false;
    s_process_state.leak_alarm = false;
    s_process_state.leak_cable_fault = false;
    s_process_state.power_failure = false;
}

static void simulation_update_normal(
    uint64_t uptime_ms
)
{
    s_process_state.room_temperature[0] =
        -18.0f + simulation_wave(uptime_ms, 0.25f, 40.0f);

    s_process_state.room_temperature[1] =
        -17.7f + simulation_wave(uptime_ms, 0.20f, 45.0f);

    s_process_state.room_temperature[2] =
        -18.3f + simulation_wave(uptime_ms, 0.18f, 50.0f);

    s_process_state.room_temperature[3] =
        -17.9f + simulation_wave(uptime_ms, 0.22f, 55.0f);

    s_process_state.air_temperature =
        -17.8f + simulation_wave(uptime_ms, 0.30f, 60.0f);

    s_process_state.relative_humidity =
        72.0f + simulation_wave(uptime_ms, 1.5f, 70.0f);

    if (s_process_state.compressor_run) {
        s_process_state.compressor_current =
            20.0f + simulation_wave(uptime_ms, 1.2f, 15.0f);
    } else {
        s_process_state.compressor_current = 0.2f;
    }

    if (!s_process_state.power_failure) {
        s_process_state.battery_voltage =
            26.4f + simulation_wave(uptime_ms, 0.08f, 90.0f);
    }
}

static void simulation_update_full_system(
    uint64_t uptime_ms
)
{
    const uint64_t scenario_ms = uptime_ms % 240000ULL;

    simulation_update_normal(uptime_ms);

    /*
     * 0-30 sec: Normal operation
     * 30-50 sec: Door open
     * 50-90 sec: Door closed, temperature recovery
     * 90-120 sec: Compressor trip
     * 120-160 sec: Normal recovery
     * 160-190 sec: Water leak
     * 190-220 sec: Power failure
     * 220-240 sec: Recovery
     */

    if ((scenario_ms >= 30000ULL) &&
        (scenario_ms < 50000ULL)) {

        s_process_state.door_safe = true;
        s_process_state.door_aux = true;

        const float rise =
            (float)(scenario_ms - 30000ULL) / 10000.0f;

        for (size_t i = 0; i < 4; i++) {
            s_process_state.room_temperature[i] += rise;
        }

        s_process_state.air_temperature += rise;
        s_process_state.relative_humidity += 4.0f;
    } else {
        s_process_state.door_safe = false;
        s_process_state.door_aux = false;
    }

    if ((scenario_ms >= 90000ULL) &&
        (scenario_ms < 120000ULL)) {

        s_process_state.compressor_run = false;
        s_process_state.compressor_trip = true;
        s_process_state.compressor_current = 0.0f;

        const float rise =
            (float)(scenario_ms - 90000ULL) / 15000.0f;

        for (size_t i = 0; i < 4; i++) {
            s_process_state.room_temperature[i] += rise;
        }
    } else {
        s_process_state.compressor_run = true;
        s_process_state.compressor_trip = false;
    }

    s_process_state.leak_alarm =
        (scenario_ms >= 160000ULL) &&
        (scenario_ms < 190000ULL);

    s_process_state.power_failure =
        (scenario_ms >= 190000ULL) &&
        (scenario_ms < 220000ULL);

    if (s_process_state.power_failure) {
        const float discharge =
            (float)(scenario_ms - 190000ULL) / 30000.0f;

        s_process_state.battery_voltage =
            26.4f - (2.8f * discharge);
    }
}

static void simulation_update_storage_pipeline_test(
    uint64_t uptime_ms
)
{
    /*
     * Repeating E2E storage/cloud test.
     *
     * One cycle lasts 25 seconds:
     *   0-3 s   : normal baseline
     *   3-18 s  : ROOM_TEMP_01 = -5 C
     *             (> -12 C for 15 s, exceeding the existing
     *              10-second alarm activation delay)
     *   18-25 s : ROOM_TEMP_01 = -18 C
     *             (< -14 C for 7 s, exceeding the existing
     *              5-second return delay)
     *
     * The cycle repeats forever. This deliberately produces repeated
     * ALARM ACTIVE / ALARM CLEARED events, which is useful for testing
     * journal accumulation while the cloud path is unavailable.
     */
    const uint64_t elapsed_ms =
        uptime_ms - s_scenario_started_ms;

    const uint64_t cycle_duration_ms =
        25000ULL;

    const uint64_t cycle_index =
        elapsed_ms / cycle_duration_ms;

    const uint64_t cycle_ms =
        elapsed_ms % cycle_duration_ms;

    simulation_update_normal(uptime_ms);

    uint8_t phase = 0U;

    if (cycle_ms < 3000ULL) {
        s_process_state.room_temperature[0] = -18.0f;
        phase = 0U;
    } else if (cycle_ms < 18000ULL) {
        s_process_state.room_temperature[0] = -5.0f;
        phase = 1U;
    } else {
        s_process_state.room_temperature[0] = -18.0f;
        phase = 2U;
    }

    if (phase != s_test_phase) {
        s_test_phase = phase;

        const char *phase_name =
            phase == 0U ? "NORMAL_BASELINE" :
            phase == 1U ? "HIGH_TEMP_TRIGGER" :
                          "NORMAL_RECOVERY";

        ESP_LOGI(
            TAG,
            "E2E cycle=%llu phase=%s ROOM_TEMP_01=%.1f C cycle_ms=%llu total_elapsed=%llu ms",
            (unsigned long long)cycle_index,
            phase_name,
            (double)s_process_state.room_temperature[0],
            (unsigned long long)cycle_ms,
            (unsigned long long)elapsed_ms
        );
    }
}

static void simulation_update_process_state(
    uint64_t uptime_ms
)
{
    switch (s_status.active_scenario) {
        case SIMULATION_SCENARIO_NORMAL:
            simulation_update_normal(uptime_ms);
            break;

        case SIMULATION_SCENARIO_FULL_SYSTEM:
            simulation_update_full_system(uptime_ms);
            break;

        case SIMULATION_SCENARIO_WATER_LEAK:
            simulation_update_normal(uptime_ms);
            s_process_state.leak_alarm = true;
            break;

        case SIMULATION_SCENARIO_POWER_FAILURE:
            simulation_update_normal(uptime_ms);
            s_process_state.power_failure = true;
            s_process_state.battery_voltage = 24.5f;
            break;

        case SIMULATION_SCENARIO_SENSOR_FAILURE:
            simulation_update_normal(uptime_ms);
            break;

        case SIMULATION_SCENARIO_STORAGE_PIPELINE_TEST:
            simulation_update_storage_pipeline_test(uptime_ms);
            break;

        default:
            simulation_update_normal(uptime_ms);
            break;
    }
}

static void simulation_set_float_value(
    raw_sample_t *sample,
    float value
)
{
    sample->raw_data_type = SIGNAL_DATA_TYPE_FLOAT32;
    sample->raw_value.float32 = value;
}

static void simulation_set_bool_value(
    raw_sample_t *sample,
    bool value
)
{
    sample->raw_data_type = SIGNAL_DATA_TYPE_BOOL;
    sample->raw_value.boolean = value;
}

static esp_err_t simulation_build_sample(
    signal_id_t signal_id,
    uint64_t uptime_ms,
    raw_sample_t *out
)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const signal_definition_t *definition = NULL;

    esp_err_t result =
        signal_registry_get(signal_id, &definition);

    if (result != ESP_OK || definition == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    memset(out, 0, sizeof(*out));

    out->signal_id = signal_id;
    out->source_type = RAW_SOURCE_SIMULATOR;

    out->source_device_id =
        definition->source.device_address;

    out->source_channel =
        definition->source.channel;

    out->source_register =
        definition->source.register_address;

    out->uptime_ms = uptime_ms;

    /*
     * UTC is unavailable in the initial simulator stage.
     * It remains zero until time_manager is implemented.
     */
    out->observed_at_ms = 0;

    out->communication_ok = true;
    out->crc_ok = true;
    out->source_fault = false;

    switch (signal_id) {
        case SIGNAL_ID_ROOM_TEMP_01:
            simulation_set_float_value(
                out,
                s_process_state.room_temperature[0]
            );
            break;

        case SIGNAL_ID_ROOM_TEMP_02:
            simulation_set_float_value(
                out,
                s_process_state.room_temperature[1]
            );
            break;

        case SIGNAL_ID_ROOM_TEMP_03:
            simulation_set_float_value(
                out,
                s_process_state.room_temperature[2]
            );
            break;

        case SIGNAL_ID_ROOM_TEMP_04:
            simulation_set_float_value(
                out,
                s_process_state.room_temperature[3]
            );
            break;

        case SIGNAL_ID_AIR_TEMP_01:
            simulation_set_float_value(
                out,
                s_process_state.air_temperature
            );
            break;

        case SIGNAL_ID_AIR_RH_01:
            simulation_set_float_value(
                out,
                s_process_state.relative_humidity
            );
            break;

        case SIGNAL_ID_DOOR_SAFE:
            simulation_set_bool_value(
                out,
                s_process_state.door_safe
            );
            break;

        case SIGNAL_ID_DOOR_AUX:
            simulation_set_bool_value(
                out,
                s_process_state.door_aux
            );
            break;

        case SIGNAL_ID_LEAK_ALARM:
            simulation_set_bool_value(
                out,
                s_process_state.leak_alarm
            );
            break;

        case SIGNAL_ID_LEAK_CABLE_FAULT:
            simulation_set_bool_value(
                out,
                s_process_state.leak_cable_fault
            );
            break;

        case SIGNAL_ID_COMPRESSOR_RUN:
            simulation_set_bool_value(
                out,
                s_process_state.compressor_run
            );
            break;

        case SIGNAL_ID_COMPRESSOR_TRIP:
            simulation_set_bool_value(
                out,
                s_process_state.compressor_trip
            );
            break;

        case SIGNAL_ID_EVAP_FAN_RUN:
            simulation_set_bool_value(
                out,
                s_process_state.evaporator_fan_run
            );
            break;

        case SIGNAL_ID_POWER_FAILURE:
            simulation_set_bool_value(
                out,
                s_process_state.power_failure
            );
            break;

        case SIGNAL_ID_COMPRESSOR_CURRENT:
            simulation_set_float_value(
                out,
                s_process_state.compressor_current
            );
            break;

        case SIGNAL_ID_BATTERY_VOLTAGE:
            simulation_set_float_value(
                out,
                s_process_state.battery_voltage
            );
            break;

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    /*
     * Sensor-failure scenario:
     * ROOM_TEMP_03 behaves as a failed sensor while the remaining
     * signals continue normally.
     */
    if ((s_status.active_scenario ==
         SIMULATION_SCENARIO_SENSOR_FAILURE) &&
        (signal_id == SIGNAL_ID_ROOM_TEMP_03)) {

        out->source_fault = true;
        s_status.sensor_faults_generated++;
    }

    /*
     * Full-system scenario:
     * Simulate communication loss for ROOM_TEMP_03
     * between 130 and 150 seconds.
     */
    if (s_status.active_scenario ==
        SIMULATION_SCENARIO_FULL_SYSTEM) {

        const uint64_t scenario_ms =
            uptime_ms % 240000ULL;

        if ((scenario_ms >= 130000ULL) &&
            (scenario_ms < 150000ULL) &&
            (signal_id == SIGNAL_ID_ROOM_TEMP_03)) {

            out->communication_ok = false;
            out->crc_ok = false;
            s_status.communication_faults_generated++;
        }
    }

    return ESP_OK;
}

static esp_err_t simulation_schedule_init(
    uint64_t now_ms
)
{
    memset(s_schedule, 0, sizeof(s_schedule));

    for (signal_id_t id = 0;
         id < SIGNAL_ID_COUNT;
         id++) {

        const signal_definition_t *definition = NULL;

        if (signal_registry_get(id, &definition) != ESP_OK) {
            continue;
        }

        if (!definition->enabled) {
            continue;
        }

        s_schedule[id].signal_id = id;
        s_schedule[id].next_sample_uptime_ms = now_ms;
        s_schedule[id].initialized = true;
    }

    return ESP_OK;
}

static void simulation_process_due_signals(
    uint64_t now_ms
)
{
    for (signal_id_t id = 0;
         id < SIGNAL_ID_COUNT;
         id++) {

        simulation_schedule_entry_t *entry =
            &s_schedule[id];

        if (!entry->initialized) {
            continue;
        }

        const signal_definition_t *definition = NULL;

        if (signal_registry_get(
                entry->signal_id,
                &definition
            ) != ESP_OK) {

            continue;
        }

        if (now_ms < entry->next_sample_uptime_ms) {
            continue;
        }

        raw_sample_t sample;

        esp_err_t build_result =
            simulation_build_sample(
                entry->signal_id,
                now_ms,
                &sample
            );

        if (build_result == ESP_OK) {
            esp_err_t queue_result =
                app_queues_send_raw_sample(
                    &sample,
                    pdMS_TO_TICKS(
                        SIMULATION_QUEUE_TIMEOUT_MS
                    )
                );

            if (queue_result == ESP_OK) {
                s_status.generated_samples++;
            } else {
                s_status.queue_failures++;

                ESP_LOGW(
                    TAG,
                    "Raw sample queue full: signal=%s",
                    signal_registry_name(
                        entry->signal_id
                    )
                );
            }
        }

        /*
         * Preserve the periodic schedule instead of setting
         * next_sample = now + period, which would accumulate drift.
         */
        do {
            entry->next_sample_uptime_ms +=
                definition->sample_period_ms;
        } while (
            entry->next_sample_uptime_ms <= now_ms
        );
    }
}

static void simulation_task(void *argument)
{
    (void)argument;

    const uint64_t start_ms = simulation_now_ms();

    s_scenario_started_ms = start_ms;
    s_test_phase = UINT8_MAX;

    simulation_schedule_init(start_ms);

    s_status.running = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Simulation task started, scenario=%d",
        (int)s_status.active_scenario
    );

    while (!s_stop_requested) {
        const uint64_t now_ms = simulation_now_ms();

        simulation_update_process_state(now_ms);
        simulation_process_due_signals(now_ms);

        s_status.current_cycle++;

        vTaskDelay(
            pdMS_TO_TICKS(
                SIMULATION_LOOP_PERIOD_MS
            )
        );
    }

    s_status.running = false;
    s_simulation_task_handle = NULL;

    ESP_LOGI(TAG, "Simulation task stopped");

    vTaskDelete(NULL);
}

esp_err_t simulation_engine_init(
    simulation_scenario_t scenario
)
{
    if (scenario > SIMULATION_SCENARIO_STORAGE_PIPELINE_TEST) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_status, 0, sizeof(s_status));
    memset(s_schedule, 0, sizeof(s_schedule));

    simulation_reset_process_state();

    s_status.active_scenario = scenario;
    s_scenario_started_ms = simulation_now_ms();
    s_test_phase = UINT8_MAX;
    s_initialized = true;
    s_stop_requested = false;

    ESP_LOGI(
        TAG,
        "Simulation engine initialized, scenario=%d",
        (int)scenario
    );

    return ESP_OK;
}

esp_err_t simulation_engine_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!app_queues_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_simulation_task_handle != NULL ||
        s_status.running) {

        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t task_result = xTaskCreate(
        simulation_task,
        SIMULATION_TASK_NAME,
        SIMULATION_TASK_STACK_SIZE,
        NULL,
        SIMULATION_TASK_PRIORITY,
        &s_simulation_task_handle
    );

    if (task_result != pdPASS) {
        s_simulation_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t simulation_engine_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_status.running &&
        s_simulation_task_handle == NULL) {

        return ESP_OK;
    }

    s_stop_requested = true;
    return ESP_OK;
}

esp_err_t simulation_engine_set_scenario(
    simulation_scenario_t scenario
)
{
    if (scenario > SIMULATION_SCENARIO_STORAGE_PIPELINE_TEST) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_status.active_scenario = scenario;
    simulation_reset_process_state();
    s_scenario_started_ms = simulation_now_ms();
    s_test_phase = UINT8_MAX;

    ESP_LOGI(
        TAG,
        "Simulation scenario changed to %d",
        (int)scenario
    );

    return ESP_OK;
}

esp_err_t simulation_engine_get_status(
    simulation_engine_status_t *out
)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    *out = s_status;
    return ESP_OK;
}

bool simulation_engine_is_running(void)
{
    return s_status.running;
}
