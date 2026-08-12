/*
 * signal_registry.c
 *
 *  Created on: Aug 4, 2026
 *      Author: Mohamed Aldreamly
 */


#include "signal_registry.h"

#include <math.h>
#include <stddef.h>

#include "esp_log.h"
#include "signal_registry_internal.h"

static const char *TAG = "SIGNAL_REGISTRY";

#define MODBUS_BUS_ADAM       0U
#define MODBUS_BUS_VAISALA    1U

#define ADAM_4115_ADDRESS     1U
#define ADAM_4051_ADDRESS     2U
#define ADAM_4017_ADDRESS     3U
#define HMP110_ADDRESS        240U

//#define SIM_REGISTER_TBD      0U
#define MODBUS_REGISTER_UNDEFINED UINT16_MAX

const signal_definition_t g_signal_definitions[] = {
    {
        .signal_id = SIGNAL_ID_ROOM_TEMP_01,
        .name = "ROOM_TEMP_01",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_DEG_C,
        .source = {
            .kind = SIGNAL_SOURCE_MODBUS_REGISTER,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4115_ADDRESS,
            .channel = 0,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 4,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = -50.0f,
        .valid_max = 50.0f,
        .sample_period_ms = 2000,
        .stale_after_ms = 15000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_ROOM_TEMP_02,
        .name = "ROOM_TEMP_02",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_DEG_C,
        .source = {
            .kind = SIGNAL_SOURCE_MODBUS_REGISTER,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4115_ADDRESS,
            .channel = 1,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 4,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = -50.0f,
        .valid_max = 50.0f,
        .sample_period_ms = 2000,
        .stale_after_ms = 15000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_ROOM_TEMP_03,
        .name = "ROOM_TEMP_03",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_DEG_C,
        .source = {
            .kind = SIGNAL_SOURCE_MODBUS_REGISTER,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4115_ADDRESS,
            .channel = 2,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 4,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = -50.0f,
        .valid_max = 50.0f,
        .sample_period_ms = 2000,
        .stale_after_ms = 15000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_ROOM_TEMP_04,
        .name = "ROOM_TEMP_04",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_DEG_C,
        .source = {
            .kind = SIGNAL_SOURCE_MODBUS_REGISTER,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4115_ADDRESS,
            .channel = 3,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 4,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = -50.0f,
        .valid_max = 50.0f,
        .sample_period_ms = 2000,
        .stale_after_ms = 15000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_AIR_TEMP_01,
        .name = "AIR_TEMP_01",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_DEG_C,
        .source = {
            .kind = SIGNAL_SOURCE_MODBUS_REGISTER,
            .bus_id = MODBUS_BUS_VAISALA,
            .device_address = HMP110_ADDRESS,
            .channel = 0,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 3,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = -40.0f,
        .valid_max = 80.0f,
        .sample_period_ms = 2000,
        .stale_after_ms = 15000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_AIR_RH_01,
        .name = "AIR_RH_01",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_PERCENT_RH,
        .source = {
            .kind = SIGNAL_SOURCE_MODBUS_REGISTER,
            .bus_id = MODBUS_BUS_VAISALA,
            .device_address = HMP110_ADDRESS,
            .channel = 1,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 3,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 100.0f,
        .sample_period_ms = 2000,
        .stale_after_ms = 15000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_DOOR_SAFE,
        .name = "DOOR_SAFE",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 0,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_DOOR_AUX,
        .name = "DOOR_AUX",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 1,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_LEAK_ALARM,
        .name = "LEAK_ALARM",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 2,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_LEAK_CABLE_FAULT,
        .name = "LEAK_CABLE_FAULT",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 3,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_COMPRESSOR_RUN,
        .name = "COMPRESSOR_RUN",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 4,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_COMPRESSOR_TRIP,
        .name = "COMPRESSOR_TRIP",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 5,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_EVAP_FAN_RUN,
        .name = "EVAP_FAN_RUN",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 6,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_POWER_FAILURE,
        .name = "POWER_FAILURE",
        .data_type = SIGNAL_DATA_TYPE_BOOL,
        .unit = SIGNAL_UNIT_NONE,
        .source = {
            .kind = SIGNAL_SOURCE_DIGITAL_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4051_ADDRESS,
            .channel = 7,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 2,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 1.0f,
        .sample_period_ms = 100,
        .stale_after_ms = 1000,
        .filter_type = SIGNAL_FILTER_NONE,
        .filter_window_size = 1,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_COMPRESSOR_CURRENT,
        .name = "COMPRESSOR_CURRENT",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_AMPERE,
        .source = {
            .kind = SIGNAL_SOURCE_ANALOG_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4017_ADDRESS,
            .channel = 0,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 4,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 100.0f,
        .sample_period_ms = 1000,
        .stale_after_ms = 5000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 5,
        .enabled = true,
    },
    {
        .signal_id = SIGNAL_ID_BATTERY_VOLTAGE,
        .name = "BATTERY_VOLTAGE",
        .data_type = SIGNAL_DATA_TYPE_FLOAT32,
        .unit = SIGNAL_UNIT_VOLT,
        .source = {
            .kind = SIGNAL_SOURCE_ANALOG_INPUT,
            .bus_id = MODBUS_BUS_ADAM,
            .device_address = ADAM_4017_ADDRESS,
            .channel = 1,
            .register_address = MODBUS_REGISTER_UNDEFINED,
            .function_code = 4,
        },
        .gain = 1.0f,
        .offset = 0.0f,
        .valid_min = 0.0f,
        .valid_max = 30.0f,
        .sample_period_ms = 5000,
        .stale_after_ms = 20000,
        .filter_type = SIGNAL_FILTER_MOVING_AVERAGE,
        .filter_window_size = 3,
        .enabled = true,
    },
};

const size_t g_signal_definition_count =
    sizeof(g_signal_definitions) / sizeof(g_signal_definitions[0]);

esp_err_t signal_registry_init(void)
{
    esp_err_t result = signal_registry_validate();

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Signal registry validation failed: %s",
                 esp_err_to_name(result));
        return result;
    }

    ESP_LOGI(TAG, "Signal registry initialized with %u signals",
             (unsigned int)g_signal_definition_count);

    return ESP_OK;
}

esp_err_t signal_registry_get(
    signal_id_t id,
    const signal_definition_t **out
)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out = NULL;

    for (size_t index = 0; index < g_signal_definition_count; index++) {
        if (g_signal_definitions[index].signal_id == id) {
            *out = &g_signal_definitions[index];
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

size_t signal_registry_count(void)
{
    return g_signal_definition_count;
}

bool signal_registry_is_enabled(signal_id_t id)
{
    const signal_definition_t *definition = NULL;

    if (signal_registry_get(id, &definition) != ESP_OK) {
        return false;
    }

    return definition->enabled;
}

const char *signal_registry_name(signal_id_t id)
{
    const signal_definition_t *definition = NULL;

    if (signal_registry_get(id, &definition) != ESP_OK) {
        return "INVALID";
    }

    return definition->name;
}

esp_err_t signal_registry_validate(void)
{
    if (g_signal_definition_count == 0) {
        ESP_LOGE(TAG, "Signal registry is empty");
        return ESP_ERR_INVALID_STATE;
    }

    for (size_t index = 0; index < g_signal_definition_count; index++) {
        const signal_definition_t *definition =
            &g_signal_definitions[index];

        if (definition->signal_id >= SIGNAL_ID_COUNT) {
            ESP_LOGE(TAG, "Invalid signal ID at index %u",
                     (unsigned int)index);
            return ESP_ERR_INVALID_ARG;
        }

        if (definition->name == NULL || definition->name[0] == '\0') {
            ESP_LOGE(TAG, "Missing name for signal index %u",
                     (unsigned int)index);
            return ESP_ERR_INVALID_ARG;
        }

        if (!isfinite(definition->gain) ||
            !isfinite(definition->offset)) {
            ESP_LOGE(TAG, "Invalid scaling for %s",
                     definition->name);
            return ESP_ERR_INVALID_ARG;
        }

        if (definition->valid_min > definition->valid_max) {
            ESP_LOGE(TAG, "Invalid range for %s",
                     definition->name);
            return ESP_ERR_INVALID_ARG;
        }

        if (definition->sample_period_ms == 0) {
            ESP_LOGE(TAG, "Sample period is zero for %s",
                     definition->name);
            return ESP_ERR_INVALID_ARG;
        }

        if (definition->stale_after_ms <
            definition->sample_period_ms) {
            ESP_LOGE(TAG,
                     "Stale timeout is shorter than sample period for %s",
                     definition->name);
            return ESP_ERR_INVALID_ARG;
        }

        if (definition->filter_window_size == 0) {
            ESP_LOGE(TAG, "Filter window is zero for %s",
                     definition->name);
            return ESP_ERR_INVALID_ARG;
        }

        for (size_t other = index + 1;
             other < g_signal_definition_count;
             other++) {
            if (definition->signal_id ==
                g_signal_definitions[other].signal_id) {
                ESP_LOGE(TAG,
                         "Duplicate signal ID: %s",
                         definition->name);
                return ESP_ERR_INVALID_STATE;
            }
        }
    }

    return ESP_OK;
}

