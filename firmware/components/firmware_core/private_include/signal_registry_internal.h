#ifndef SIGNAL_REGISTRY_INTERNAL_H
#define SIGNAL_REGISTRY_INTERNAL_H

#include <stddef.h>

#include "../../components/firmware_core/include/signal_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const signal_definition_t g_signal_definitions[];
extern const size_t g_signal_definition_count;

#ifdef __cplusplus
}
#endif

#endif