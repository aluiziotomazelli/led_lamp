#pragma once
#include "led_effects.h" // For color_t
#include <stdint.h>

typedef struct {
	// Configuration
	uint16_t num_zones;
	uint16_t leds_per_zone;

	/**
	 * @brief float flicker_speed;
	 * Controla a velocidade geral da cintilação.
	 * Valores mais altos criam uma cintilação mais rápida e nervosa, como uma
	 * vela exposta a uma corrente de ar. Valores mais baixos resultam em uma
	 * mudança de luz mais lenta e suave.
	 * 
	 * Valor na UI: Um número inteiro de 1 a 100.
	 * Valor no Algoritmo (flicker_speed): Um float.
	 * Correlação: O valor da UI é dividido por 20.0.
	 */
	float flicker_speed;

	/**
	 * @brief float dip_probability;
	 * Define a probabilidade (chance) de ocorrer uma "queda" súbita e acentuada
	 * no brilho. Isso simula a instabilidade da chama, como se ela quase se
	 * apagasse por um instante. Um valor de 0.01 significa 1% de chance a cada
	 * atualização.
	 *
	 * Valor na UI: Um número inteiro de 0 a 100.
	 * Valor no Algoritmo (dip_probability): Um float bem pequeno (ex: 0.0 a 0.1).
	 * Correlação: O valor da UI é dividido por 1000.0.
	 */
	float dip_probability;

	/**
	 * @brief float recovery_rate;
	 * Determina a rapidez com que o brilho da vela retorna ao seu nível normal
	 * após uma cintilação ou uma queda. É um fator de "suavização". Valores
	 * mais altos fazem com que a luz se recupere mais rápido, enquanto valores
	 * mais baixos criam uma transição mais gradual e suave.
	 */
	float recovery_rate;

	/**
	 * @brief float min_brightness;
	 * O nível de brilho mínimo absoluto (em porcentagem, ex: 20.0 para 20%) que
	 * a chama pode atingir. Isso evita que a "chama" se apague completamente
	 * durante as cintilações, mantendo o efeito mais realista.
	 */
	float min_brightness;

	/**
	 * @brief float max_brightness;
	 * O nível de brilho máximo absoluto (em porcentagem, ex: 100.0 para 100%)
	 * que a chama pode atingir. Isso impede que a cintilação se torne
	 * excessivamente e artificialmente brilhante.
	 */
	float max_brightness;

	/**
	 * @brief float base_brightness;
	 * O nível de brilho médio (em porcentagem) em torno do qual a cintilação
	 * ocorre. É o brilho "padrão" ou "de repouso" da vela. A luz irá variar
	 * acima e abaixo deste valor.
	 */
	float base_brightness;

	/**
	 * @brief float flicker_intensity;
	 * Controla a "força" ou a amplitude da cintilação.
	 * Um valor mais alto cria uma diferença maior entre os picos de brilho e as
	 * quedas, resultando em uma cintilação mais dramática. Um valor mais baixo
	 * torna o efeito mais sutil e calmo.
	 * 
	 * Valor na UI: Um número inteiro de 0 a 100.
	 * Valor no Algoritmo (flicker_intensity): Um float de 0.0 a 1.0.
	 * Correlação: O valor da UI é dividido por 100.0.
	 */
	float flicker_intensity;

	// Color config
	uint16_t base_hue;
	uint16_t min_hue;
	uint16_t max_hue;
	uint16_t base_sat;
	uint16_t min_sat;
	uint16_t max_sat;
} candle_config_t;

typedef struct {
	candle_config_t config;
	float *zone_brightness;
	float global_brightness;
} candle_effect_t;

// Initialize effect
candle_effect_t *candle_effect_init(const candle_config_t *config);

// Update effect
void candle_effect_update(candle_effect_t *effect, float delta_time,
						  color_t *pixels, uint16_t num_pixels);

// Cleanup
void candle_effect_deinit(candle_effect_t *effect);
