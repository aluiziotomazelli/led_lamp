#include "system_events.h"

// Queue global do sistema
static QueueHandle_t system_queue = NULL;

void system_events_init(void) {
    // Queue com 10 eventos - ajuste se precisar
    system_queue = xQueueCreate(10, sizeof(system_event_t));
}

bool system_event_send(const system_event_t *event) {
    if (system_queue == NULL) return false;
    
    // Timeout curto para não travar
    return (xQueueSend(system_queue, event, pdMS_TO_TICKS(10)) == pdTRUE);
}

bool system_event_receive(system_event_t *event, uint32_t timeout_ms) {
    if (system_queue == NULL) return false;
    
    TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xQueueReceive(system_queue, event, timeout_ticks) == pdTRUE);
}