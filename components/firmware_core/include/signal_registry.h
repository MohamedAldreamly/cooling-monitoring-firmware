/*
 * signal_registry.h
 *
 *  Created on: Aug 4, 2026
 *      Author: Ayman Abu Kareem
 */

#ifndef SIGNAL_REGISTRY_H
#define SIGNAL_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../components/firmware_core/include/app_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIGNAL_FILTER_NONE = 0,
    SIGNAL_FILTER_MOVING_AVERAGE,
    SIGNAL_FILTER_MEDIAN
} signal_filter_type_t;

typedef enum {
    SIGNAL_SOURCE_SIMULATOR = 0,
    SIGNAL_SOURCE_MODBUS_REGISTER,
    SIGNAL_SOURCE_DIGITAL_INPUT,
    SIGNAL_SOURCE_ANALOG_INPUT,
    SIGNAL_SOURCE_DERIVED
} signal_source_kind_t;

typedef struct {
    signal_source_kind_t kind;

    uint8_t bus_id;
    uint8_t device_address;
    uint16_t channel;
    uint16_t register_address;
    uint8_t function_code;
} signal_source_t;

typedef struct {
    signal_id_t signal_id;

    const char *name;
    signal_data_type_t data_type;
    signal_unit_t unit;

    signal_source_t source;

    float gain;
    float offset;

    float valid_min;
    float valid_max;

    uint32_t sample_period_ms;
    uint32_t stale_after_ms;

    signal_filter_type_t filter_type;
    uint8_t filter_window_size;

    bool enabled;
} signal_definition_t;

/**
 * @brief Initializes the signal registry.
 *
 * @return ESP_OK on success.
 */
esp_err_t signal_registry_init(void);

/**
 * @brief Returns a signal definition by ID.
 *
 * @param id Signal identifier.
 * @param out Pointer to output definition.
 *
 * @return ESP_OK if found.
 * @return ESP_ERR_INVALID_ARG if out is NULL.
 * @return ESP_ERR_NOT_FOUND if signal ID does not exist.
 */
esp_err_t signal_registry_get(
    signal_id_t id,
    const signal_definition_t **out
);

/**
 * @brief Returns the number of registered signals.
 */
size_t signal_registry_count(void);

/**
 * @brief Returns whether the signal exists and is enabled.
 */
bool signal_registry_is_enabled(signal_id_t id);

/**
 * @brief Validates all registry entries.
 *
 * Checks duplicate IDs, invalid ranges, timing values,
 * data types, and stale timeout policy.
 */
esp_err_t signal_registry_validate(void);

/**
 * @brief Returns the signal name.
 */
const char *signal_registry_name(signal_id_t id);

#ifdef __cplusplus
}
#endif

#endif
