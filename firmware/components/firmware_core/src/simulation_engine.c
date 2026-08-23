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

typedef struct {
    signal_id_t signal_id;
    const char *name;
    uint32_t abnormal_ms;
    uint32_t recovery_ms;
} simulation_alarm_step_t;

static const simulation_alarm_step_t s_alarm_steps[] = {
    { SIGNAL_ID_ROOM_TEMP_01, "ROOM_TEMP_01", 15000U, 10000U },
    { SIGNAL_ID_ROOM_TEMP_02, "ROOM_TEMP_02", 15000U, 10000U },
    { SIGNAL_ID_ROOM_TEMP_03, "ROOM_TEMP_03", 15000U, 10000U },
    { SIGNAL_ID_ROOM_TEMP_04, "ROOM_TEMP_04", 15000U, 10000U },
    { SIGNAL_ID_AIR_TEMP_01, "AIR_TEMP_01", 15000U, 10000U },
    { SIGNAL_ID_AIR_RH_01, "AIR_RH_01", 15000U, 10000U },
    { SIGNAL_ID_DOOR_SAFE, "DOOR_SAFE", 10000U, 5000U },
    { SIGNAL_ID_DOOR_AUX, "DOOR_AUX", 10000U, 5000U },
    { SIGNAL_ID_LEAK_ALARM, "LEAK_ALARM", 10000U, 5000U },
    { SIGNAL_ID_LEAK_CABLE_FAULT, "LEAK_CABLE_FAULT", 10000U, 5000U },
    { SIGNAL_ID_COMPRESSOR_RUN, "COMPRESSOR_RUN", 10000U, 5000U },
    { SIGNAL_ID_COMPRESSOR_TRIP, "COMPRESSOR_TRIP", 10000U, 5000U },
    { SIGNAL_ID_EVAP_FAN_RUN, "EVAP_FAN_RUN", 10000U, 5000U },
    { SIGNAL_ID_POWER_FAILURE, "POWER_FAILURE", 10000U, 5000U },
    { SIGNAL_ID_COMPRESSOR_CURRENT, "COMPRESSOR_CURRENT", 15000U, 10000U },
    { SIGNAL_ID_BATTERY_VOLTAGE, "BATTERY_VOLTAGE", 25000U, 25000U },
};

static const size_t s_alarm_step_count =
    sizeof(s_alarm_steps) / sizeof(s_alarm_steps[0]);

static void simulation_reset_alarm_inputs(void)
{
    s_process_state.door_safe = false;
    s_process_state.door_aux = false;
    s_process_state.leak_alarm = false;
    s_process_state.leak_cable_fault = false;
    s_process_state.compressor_run = true;
    s_process_state.compressor_trip = false;
    s_process_state.evaporator_fan_run = true;
    s_process_state.power_failure = false;
}

static void simulation_apply_alarm_level(
    signal_id_t signal_id,
    uint64_t step_ms,
    uint32_t abnormal_ms
)
{
    const uint32_t third = abnormal_ms / 3U;
    const uint8_t level =
        step_ms < third ? 0U :
        step_ms < (uint64_t)(third * 2U) ? 1U : 2U;

    static const float temperature_values[] = {
        -11.0f, -7.0f, -3.0f
    };
    static const float humidity_values[] = {
        84.0f, 91.0f, 98.0f
    };
    static const float current_values[] = {
        34.0f, 48.0f, 68.0f
    };
    static const float battery_values[] = {
        23.5f, 22.5f, 20.5f
    };

    switch (signal_id) {
        case SIGNAL_ID_ROOM_TEMP_01:
        case SIGNAL_ID_ROOM_TEMP_02:
        case SIGNAL_ID_ROOM_TEMP_03:
        case SIGNAL_ID_ROOM_TEMP_04:
            s_process_state.room_temperature[
                signal_id - SIGNAL_ID_ROOM_TEMP_01
            ] = temperature_values[level];
            break;

        case SIGNAL_ID_AIR_TEMP_01:
            s_process_state.air_temperature =
                temperature_values[level];
            break;

        case SIGNAL_ID_AIR_RH_01:
            s_process_state.relative_humidity =
                humidity_values[level];
            break;

        case SIGNAL_ID_DOOR_SAFE:
            s_process_state.door_safe = true;
            break;

        case SIGNAL_ID_DOOR_AUX:
            s_process_state.door_aux = true;
            break;

        case SIGNAL_ID_LEAK_ALARM:
            s_process_state.leak_alarm = true;
            break;

        case SIGNAL_ID_LEAK_CABLE_FAULT:
            s_process_state.leak_cable_fault = true;
            break;

        case SIGNAL_ID_COMPRESSOR_RUN:
            s_process_state.compressor_run = false;
            s_process_state.compressor_current = 0.0f;
            break;

        case SIGNAL_ID_COMPRESSOR_TRIP:
            s_process_state.compressor_trip = true;
            break;

        case SIGNAL_ID_EVAP_FAN_RUN:
            s_process_state.evaporator_fan_run = false;
            break;

        case SIGNAL_ID_POWER_FAILURE:
            s_process_state.power_failure = true;
            break;

        case SIGNAL_ID_COMPRESSOR_CURRENT:
            s_process_state.compressor_current =
                current_values[level];
            break;

        case SIGNAL_ID_BATTERY_VOLTAGE:
            s_process_state.battery_voltage =
                battery_values[level];
            break;

        default:
            break;
    }
}

static void simulation_update_full_system(
    uint64_t uptime_ms
)
{
    const uint64_t baseline_ms = 5000ULL;
    const uint64_t all_active_hold_ms = 15000ULL;
    const uint64_t final_normal_ms = 5000ULL;

    uint64_t cycle_duration_ms =
        baseline_ms +
        all_active_hold_ms +
        final_normal_ms;

    for (size_t index = 0U;
         index < s_alarm_step_count;
         index++) {
        cycle_duration_ms +=
            s_alarm_steps[index].abnormal_ms +
            s_alarm_steps[index].recovery_ms;
    }

    const uint64_t elapsed_ms =
        uptime_ms - s_scenario_started_ms;
    const uint64_t cycle_index =
        elapsed_ms / cycle_duration_ms;
    const uint64_t cycle_ms =
        elapsed_ms % cycle_duration_ms;

    simulation_reset_alarm_inputs();
    simulation_update_normal(uptime_ms);

    uint8_t phase = 0U;
    const char *phase_name = "NORMAL_BASELINE";
    uint64_t cursor_ms = baseline_ms;
    bool phase_selected = false;

    /*
     * Activation stage:
     *
     * Previously activated signals remain at their critical value while
     * the next signal moves through warning, high and critical. This lets
     * the active-alarm count grow from 1 to SIGNAL_ID_COUNT instead of
     * closing each alarm before the following one starts.
     */
    for (size_t index = 0U;
         index < s_alarm_step_count;
         index++) {
        const simulation_alarm_step_t *step =
            &s_alarm_steps[index];

        const uint64_t abnormal_end_ms =
            cursor_ms + step->abnormal_ms;

        if (cycle_ms >= cursor_ms &&
            cycle_ms < abnormal_end_ms) {
            for (size_t active_index = 0U;
                 active_index < index;
                 active_index++) {
                const simulation_alarm_step_t *active_step =
                    &s_alarm_steps[active_index];

                simulation_apply_alarm_level(
                    active_step->signal_id,
                    active_step->abnormal_ms - 1U,
                    active_step->abnormal_ms
                );
            }

            phase = (uint8_t)(1U + index);
            phase_name = step->name;
            phase_selected = true;

            simulation_apply_alarm_level(
                step->signal_id,
                cycle_ms - cursor_ms,
                step->abnormal_ms
            );
            break;
        }

        cursor_ms = abnormal_end_ms;
    }

    /* Keep all 16 alarms critical long enough for Telemetry and the
     * dashboard polling cycle to observe the fully active system. */
    if (!phase_selected &&
        cycle_ms >= cursor_ms &&
        cycle_ms < (cursor_ms + all_active_hold_ms)) {
        for (size_t active_index = 0U;
             active_index < s_alarm_step_count;
             active_index++) {
            const simulation_alarm_step_t *active_step =
                &s_alarm_steps[active_index];

            simulation_apply_alarm_level(
                active_step->signal_id,
                active_step->abnormal_ms - 1U,
                active_step->abnormal_ms
            );
        }

        phase = (uint8_t)(1U + s_alarm_step_count);
        phase_name = "ALL_ALARMS_CRITICAL";
        phase_selected = true;
    }

    cursor_ms += all_active_hold_ms;

    /*
     * Recovery stage:
     *
     * Recover one signal at a time. Signals already processed by this
     * loop remain normal, while all later signals stay critical. The Alarm
     * Engine therefore publishes RETURNED and CLOSED progressively until
     * the active-alarm count reaches zero.
     */
    if (!phase_selected) {
        for (size_t index = 0U;
             index < s_alarm_step_count;
             index++) {
            const simulation_alarm_step_t *step =
                &s_alarm_steps[index];
            const uint64_t recovery_end_ms =
                cursor_ms + step->recovery_ms;

            if (cycle_ms >= cursor_ms &&
                cycle_ms < recovery_end_ms) {
                for (size_t active_index = index + 1U;
                     active_index < s_alarm_step_count;
                     active_index++) {
                    const simulation_alarm_step_t *active_step =
                        &s_alarm_steps[active_index];

                    simulation_apply_alarm_level(
                        active_step->signal_id,
                        active_step->abnormal_ms - 1U,
                        active_step->abnormal_ms
                    );
                }

                phase = (uint8_t)(
                    2U + s_alarm_step_count + index
                );
                phase_name = step->name;
                phase_selected = true;
                break;
            }

            cursor_ms = recovery_end_ms;
        }
    }

    if (!phase_selected && cycle_ms >= cursor_ms) {
        phase = (uint8_t)(2U + (s_alarm_step_count * 2U));
        phase_name = "NORMAL_BEFORE_REPEAT";
    }

    if (phase != s_test_phase) {
        s_test_phase = phase;

        ESP_LOGI(
            TAG,
            "Continuous simulation cycle=%llu phase=%u state=%s cycle_ms=%llu duration_ms=%llu",
            (unsigned long long)cycle_index,
            (unsigned)phase,
            phase_name,
            (unsigned long long)cycle_ms,
            (unsigned long long)cycle_duration_ms
        );
    }
}

static void simulation_update_storage_pipeline_test(
    uint64_t uptime_ms
)
{
    simulation_update_full_system(uptime_ms);
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
