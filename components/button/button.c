#include "button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "system_events.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
static const char *TAG = "Button";

// Função para mapear button_click_type_t para event_type_t
static event_type_t map_button_to_system_event(button_click_type_t btn_type) {
	switch (btn_type) {
	case BUTTON_CLICK:     		  return EVENT_BUTTON_CLICK;
	case BUTTON_DOUBLE_CLICK:  	  return EVENT_BUTTON_DOUBLE_CLICK;
	case BUTTON_LONG_CLICK:    	  return EVENT_BUTTON_LONG_CLICK;
	case BUTTON_VERY_LONG_CLICK:  return EVENT_BUTTON_VERY_LONG_CLICK;
	case BUTTON_TIMEOUT:          return EVENT_BUTTON_TIMEOUT;
	case BUTTON_ERROR:            return EVENT_BUTTON_ERROR;
	default:                      return EVENT_NONE; // fallback
	}
}

// Estado do botão
typedef enum {
	BUTTON_WAIT_FOR_PRESS,
	BUTTON_DEBOUNCE_PRESS,
	BUTTON_WAIT_FOR_RELEASE,
	BUTTON_DEBOUNCE_RELEASE,
	BUTTON_WAIT_FOR_DOUBLE,
	BUTTON_TIMEOUT_WAIT_FOR_RELEASE
} button_state_t;

// Estrutura interna completa
struct button_s {
	gpio_num_t pin;
	button_state_t state;
	uint32_t last_time_ms;
	uint32_t press_start_time_ms;
	bool first_click;

	uint32_t debounce_press_ms;
	uint32_t debounce_release_ms;
	uint32_t double_click_ms;
	uint32_t long_click_ms;
	uint32_t very_long_click_ms;
	uint32_t timeout_ms;

	//	QueueHandle_t queue;
	TaskHandle_t task_handle;
};

// Utilitário de tempo
static uint32_t get_current_time_ms() { return esp_timer_get_time() / 1000; }

// Função principal de detecção (igual antes, mas com ponteiro)
static button_click_type_t button_get_click(button_t *btn) {
	//	uint32_t press_start_time = 0;
	uint32_t now = get_current_time_ms();

	switch (btn->state) {
	case BUTTON_WAIT_FOR_PRESS:
		if (gpio_get_level(btn->pin) == 0) {
			btn->press_start_time_ms = now;
			btn->state = BUTTON_DEBOUNCE_PRESS;
			ESP_LOGD("FSM", "STARTING DEBOUNCE");
		}
		break;

	case BUTTON_DEBOUNCE_PRESS:
		if (now - btn->press_start_time_ms > btn->debounce_press_ms) {
			btn->state = BUTTON_WAIT_FOR_RELEASE;
			ESP_LOGD("FSM", "WAIT_FOR_RELEASE");
		}
		break;

	case BUTTON_WAIT_FOR_RELEASE:
		if (gpio_get_level(btn->pin) == 1) {
			uint32_t duration = now - btn->press_start_time_ms;

			if (duration > btn->very_long_click_ms) {
				btn->state = BUTTON_WAIT_FOR_PRESS;
				return BUTTON_VERY_LONG_CLICK;
			} else if (duration > btn->long_click_ms) {
				btn->state = BUTTON_WAIT_FOR_PRESS;
				return BUTTON_LONG_CLICK;
			} else {
				btn->last_time_ms = now;
				btn->state = BUTTON_DEBOUNCE_RELEASE;
			}
		} else if (now - btn->press_start_time_ms > btn->timeout_ms) {
			btn->state = BUTTON_TIMEOUT_WAIT_FOR_RELEASE;
			//			fallback_start_time = now;
			btn->last_time_ms = now;
			ESP_LOGD("FSM", "TIMEOUT WAIT FOR RELEASE");
			//			return BUTTON_TIMEOUT;
		}
		break;

	case BUTTON_DEBOUNCE_RELEASE:
		if (now - btn->last_time_ms > btn->debounce_release_ms) {
			btn->state = BUTTON_WAIT_FOR_DOUBLE;
			ESP_LOGD("FSM", "DEBOUCE RELEASED");
		}
		break;

	case BUTTON_WAIT_FOR_DOUBLE:
		if (gpio_get_level(btn->pin) == 0 && !btn->first_click) {
			// Detectou segundo clique
			btn->last_time_ms = now;
			btn->first_click = true; // Marca que teve segundo clique
			btn->state = BUTTON_DEBOUNCE_PRESS;
		} else if (now - btn->last_time_ms > btn->double_click_ms) {
			btn->state = BUTTON_WAIT_FOR_PRESS;
			if (btn->first_click) {
				btn->first_click = false;
				return BUTTON_DOUBLE_CLICK;
			} else {
				return BUTTON_CLICK;
			}
		}
		break;

	case BUTTON_TIMEOUT_WAIT_FOR_RELEASE:
		if (gpio_get_level(btn->pin) == 1) {
			if (now - btn->last_time_ms > btn->debounce_release_ms) {
				btn->last_time_ms = now;
				btn->state = BUTTON_WAIT_FOR_PRESS;
				ESP_LOGD("FSM", "TIMEOUT RELEASED");
				return BUTTON_TIMEOUT;
			}
		} else {
			btn->last_time_ms = now;
			if (now - btn->press_start_time_ms > 2 * btn->timeout_ms) {
				btn->state = BUTTON_WAIT_FOR_PRESS;
				ESP_LOGD("FSM", "BUTTON ERROR");
				return BUTTON_ERROR;
			}
		}
		break;
	}

	return BUTTON_NONE_CLICK;
}

// Task individual de cada botão
static void button_task(void *param) {
	button_t *btn = (button_t *)param;
//	button_event_t event;
	bool processing = false;

	while (1) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		if (!processing) {
			processing = true;
			gpio_intr_disable(btn->pin);
			ESP_LOGD(TAG, "Processing started");
		}

		do {
			button_click_type_t click = button_get_click(btn);

			if (click != BUTTON_NONE_CLICK) {
				//				event.type = click;
				system_event_t sys_event = {
					.type = map_button_to_system_event(click),
					.data.button = {
						.button_id = btn->pin // ou btn->id se tiver
					}
                };

                if (system_event_send(&sys_event)) {
					ESP_LOGD(TAG, "Button %d: click %d send", btn->pin, click);
                } else {
					ESP_LOGD(TAG, "Failed to send, button: %d: click %d", btn->pin, click);
                }
				// Optional cleanup
				// ulTaskNotifyTake(pdTRUE, 0);

				// Re-enable ISR before clearing state (avoids race)
				processing = false;
				gpio_intr_enable(btn->pin);
				break;
			}
			vTaskDelay(10 / portTICK_PERIOD_MS);
		} while (processing);
	}
}

static void IRAM_ATTR button_isr_handler(void *arg) {
	button_t *btn = (button_t *)arg;

	// Notifica a task do botão que algo ocorreu
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	ESP_DRAM_LOGD(TAG, "ISR triggered for button on pin %d", btn->pin);
	xTaskNotifyFromISR(btn->task_handle, 0, eNoAction,
					   &xHigherPriorityTaskWoken);

	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}

// Criação do botão
button_t *button_create(gpio_num_t pin) {
	button_t *btn = calloc(1, sizeof(button_t));
	if (!btn)
		return NULL;

	btn->pin = pin;
	btn->state = BUTTON_WAIT_FOR_PRESS;
	btn->press_start_time_ms = 0;
	btn->last_time_ms = 0;
	btn->first_click = false;

	btn->debounce_press_ms = DEBOUNCE_PRESS_MS;
	btn->debounce_release_ms = DEBOUNCE_RELEASE_MS;
	btn->double_click_ms = DOUBLE_CLICK_MS;
	btn->long_click_ms = LONG_CLICK_MS;
	btn->very_long_click_ms = VERY_LONG_CLICK_MS;
	btn->timeout_ms = VERY_LONG_CLICK_MS * 2;

	//	btn->queue = xQueueCreate(5, sizeof(button_event_t));
	//	if (!btn->queue) {
	//		free(btn);
	//		return NULL;
	//	}

	gpio_config_t io_conf = {.pin_bit_mask = 1ULL << pin,
							 .mode = GPIO_MODE_INPUT,
							 .pull_up_en = GPIO_PULLUP_ENABLE,
							 .pull_down_en = GPIO_PULLDOWN_DISABLE,
							 .intr_type = GPIO_INTR_NEGEDGE};
	gpio_config(&io_conf);

	// Install ISR service with error handling
	esp_err_t err = gpio_install_isr_service(0);
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "ISR service installed successfully");
	} else if (err == ESP_ERR_INVALID_STATE) {
		ESP_LOGW(TAG,
				 "ISR service already installed, reusing existing service");
	} else {
		ESP_LOGE(TAG, "Failed to install ISR service: %s",
				 esp_err_to_name(err));
		free(btn);				  // Para malloc/calloc

		return NULL;
	}

	gpio_isr_handler_add(pin, button_isr_handler, btn);

	BaseType_t res = xTaskCreate(button_task, "button_task", 2048, btn, 10,
								 &btn->task_handle);
	if (res != pdPASS) {
		free(btn);
		return NULL;
	}

	ESP_LOGI(TAG, "Botão criado no pino %d", pin);
	return btn;
}

void button_set_debounce(button_t *btn, uint16_t debounce_press_ms,
						 uint16_t debounce_release_ms) {
	if (btn) {
		btn->debounce_press_ms = debounce_press_ms;
		btn->debounce_release_ms = debounce_release_ms;
		ESP_LOGI(TAG,
				 "Tempos de debounce ajustados: press %d ms, release %d ms",
				 debounce_press_ms, debounce_release_ms);
	}
}

void button_set_click_times(button_t *btn, uint16_t double_click_ms,
							uint16_t long_click_ms,
							uint16_t very_long_click_ms) {
	if (btn) {
		btn->double_click_ms = double_click_ms;
		btn->long_click_ms = long_click_ms;
		btn->very_long_click_ms = very_long_click_ms;
		btn->timeout_ms =
			very_long_click_ms * 2; // Timeout é o dobro do muito longo
		ESP_LOGI(TAG,
				 "Tempos de clique ajustados: double %d ms, long %d ms, very "
				 "long %d ms",
				 double_click_ms, long_click_ms, very_long_click_ms);
	}
}

void button_delete(button_t *btn) {
	if (btn) {
		if (btn->task_handle) {
			vTaskDelete(btn->task_handle);
			ESP_LOGI(TAG, "Task do botão no pino %d deletada", btn->pin);
		} else {
			ESP_LOGW(TAG, "Task do botão já deletada ou não inicializada");
		}
		gpio_isr_handler_remove(btn->pin);
		ESP_LOGI(TAG, "Botão no pino %d deletado", btn->pin);
		free(btn);
	}
}
