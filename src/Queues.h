#pragma once

#include "settings.h"

extern QueueHandle_t gVolumeQueue;
extern QueueHandle_t gTrackQueue;
extern QueueHandle_t gTrackControlQueue;
extern QueueHandle_t gRfidCardQueue;

#ifdef CARD_SERVER_ENABLED
    extern QueueHandle_t gCheckCardServerRfidQueue;
#endif

void Queues_Init(void);
