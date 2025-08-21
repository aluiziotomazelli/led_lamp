#pragma once

#include <math.h>
#include <stdint.h>

typedef enum {
    // Black Body Radiators
    // @{
    /// 1900 Kelvin
    Candle=0xFF9329 /* 1900 K, 255, 147, 41 */,
    /// 2600 Kelvin
    Tungsten40W=0xFFC58F /* 2600 K, 255, 197, 143 */,
    /// 2850 Kelvin
    Tungsten100W=0xFFD6AA /* 2850 K, 255, 214, 170 */,
    /// 3200 Kelvin
    Halogen=0xFFF1E0 /* 3200 K, 255, 241, 224 */,
    /// 5200 Kelvin
    CarbonArc=0xFFFAF4 /* 5200 K, 255, 250, 244 */,
    /// 5400 Kelvin
    HighNoonSun=0xFFFFFB /* 5400 K, 255, 255, 251 */,
    /// 6000 Kelvin
    DirectSunlight=0xFFFFFF /* 6000 K, 255, 255, 255 */,
    /// 7000 Kelvin
    OvercastSky=0xC9E2FF /* 7000 K, 201, 226, 255 */,
    /// 20000 Kelvin
    ClearBlueSky=0x409CFF /* 20000 K, 64, 156, 255 */,
    // @}

    // Gaseous Light Sources
    // @{
    /// Warm (yellower) flourescent light bulbs
    WarmFluorescent=0xFFF4E5 /* 0 K, 255, 244, 229 */,
    /// Standard flourescent light bulbs
    StandardFluorescent=0xF4FFFA /* 0 K, 244, 255, 250 */,
    /// Cool white (bluer) flourescent light bulbs
    CoolWhiteFluorescent=0xD4EBFF /* 0 K, 212, 235, 255 */,
    /// Full spectrum flourescent light bulbs
    FullSpectrumFluorescent=0xFFF4F2 /* 0 K, 255, 244, 242 */,
    /// Grow light flourescent light bulbs
    GrowLightFluorescent=0xFFEFF7 /* 0 K, 255, 239, 247 */,
    /// Black light flourescent light bulbs
    BlackLightFluorescent=0xA700FF /* 0 K, 167, 0, 255 */,
    /// Mercury vapor light bulbs
    MercuryVapor=0xD8F7FF /* 0 K, 216, 247, 255 */,
    /// Sodium vapor light bulbs
    SodiumVapor=0xFFD1B2 /* 0 K, 255, 209, 178 */,
    /// Metal-halide light bulbs
    MetalHalide=0xF2FCFF /* 0 K, 242, 252, 255 */,
    /// High-pressure sodium light bulbs
    HighPressureSodium=0xFFB74C /* 0 K, 255, 183, 76 */,
    // @}

    /// Uncorrected temperature (0xFFFFFF)
    UncorrectedTemperature=0xFFFFFF /* 255, 255, 255 */
} ColorTemperature;



static inline uint8_t gamma8(uint8_t x) {
    return (uint8_t)(powf(x / 255.0f, 2.2f) * 255.0f + 0.5f);
}

// Escala proporcional sem perder canais (video scaling)
static inline uint8_t scale8_video(uint8_t i, uint8_t scale) {
    return ((uint16_t)i * (uint16_t)scale + 255) >> 8;
}

// Multiplicação com arredondamento (evita viés para baixo)
static inline uint8_t scale8(uint8_t i, uint8_t scale) {
    return ((uint16_t)i * (uint16_t)scale) >> 8;
}

static inline void hsv_to_rgb_spectrum(uint8_t hue, uint8_t sat, uint8_t val,
                         uint8_t *r, uint8_t *g, uint8_t *b)
{
    // Saturação zero => branco puro
    if (sat == 0) {
        *r = *g = *b = val;
        return;
    }

    // Setor de 0 a 5 (cada um = 256/6 ≈ 42.67)
    uint8_t sector = hue / 43;          // 0..5
    uint8_t offset = (hue % 43) * 6;    // 0..258 (interpolação)

    uint8_t inv_sat = 255 - sat;
    uint8_t brightness_floor = scale8_video(val, inv_sat);
    uint8_t color_amplitude = val - brightness_floor;

    // Escala da interpolação no setor
    uint8_t rampup   = offset;          // 0..255 (aprox.)
    uint8_t rampdown = 255 - rampup;

    uint8_t rampup_adj   = scale8_video(rampup,   color_amplitude);
    uint8_t rampdown_adj = scale8_video(rampdown, color_amplitude);

    switch (sector) {
        case 0: // R -> G
            *r = color_amplitude + brightness_floor;
            *g = rampup_adj + brightness_floor;
            *b = brightness_floor;
            break;
        case 1: // G -> R
            *r = rampdown_adj + brightness_floor;
            *g = color_amplitude + brightness_floor;
            *b = brightness_floor;
            break;
        case 2: // G -> B
            *r = brightness_floor;
            *g = color_amplitude + brightness_floor;
            *b = rampup_adj + brightness_floor;
            break;
        case 3: // B -> G
            *r = brightness_floor;
            *g = rampdown_adj + brightness_floor;
            *b = color_amplitude + brightness_floor;
            break;
        case 4: // B -> R
            *r = rampup_adj + brightness_floor;
            *g = brightness_floor;
            *b = color_amplitude + brightness_floor;
            break;
        default: // R -> B
            *r = color_amplitude + brightness_floor;
            *g = brightness_floor;
            *b = rampdown_adj + brightness_floor;
            break;
    }
}

static inline void hsv_to_rgb_spectrum_deg(uint16_t hue_deg, uint8_t sat, uint8_t val,
                             uint8_t *r, uint8_t *g, uint8_t *b)
{
    // Saturação zero => branco puro
    if (sat == 0) {
        *r = *g = *b = val;
        return;
    }

    // Garante que hue esteja no intervalo 0–359
    hue_deg %= 360;

    // Cada setor tem 60 graus (6 setores)
    uint8_t sector = hue_deg / 60;          // 0..5
    uint8_t offset_deg = hue_deg % 60;      // 0..59

    // Converte offset_deg para escala 0–255 para interpolação
    uint8_t rampup   = (offset_deg * 255 + 30) / 60;  // arredondamento
    uint8_t rampdown = 255 - rampup;

    uint8_t inv_sat = 255 - sat;
    uint8_t brightness_floor = scale8_video(val, inv_sat);
    uint8_t color_amplitude  = val - brightness_floor;

    uint8_t rampup_adj   = scale8_video(rampup,   color_amplitude);
    uint8_t rampdown_adj = scale8_video(rampdown, color_amplitude);

    switch (sector) {
        case 0: // R -> G
            *r = color_amplitude + brightness_floor;
            *g = rampup_adj + brightness_floor;
            *b = brightness_floor;
            break;
        case 1: // G -> R
            *r = rampdown_adj + brightness_floor;
            *g = color_amplitude + brightness_floor;
            *b = brightness_floor;
            break;
        case 2: // G -> B
            *r = brightness_floor;
            *g = color_amplitude + brightness_floor;
            *b = rampup_adj + brightness_floor;
            break;
        case 3: // B -> G
            *r = brightness_floor;
            *g = rampdown_adj + brightness_floor;
            *b = color_amplitude + brightness_floor;
            break;
        case 4: // B -> R
            *r = rampup_adj + brightness_floor;
            *g = brightness_floor;
            *b = color_amplitude + brightness_floor;
            break;
        default: // R -> B
            *r = color_amplitude + brightness_floor;
            *g = brightness_floor;
            *b = rampdown_adj + brightness_floor;
            break;
    }
}

static inline void hsv_to_rgb_rainbow_deg(uint16_t hue_deg, uint8_t sat, uint8_t val,
                            uint8_t *r, uint8_t *g, uint8_t *b)
{
    // Saturação zero => branco puro
    if (sat == 0) {
        *r = *g = *b = val;
        return;
    }

    // Garante hue no intervalo 0–359
    hue_deg %= 360;

    // Ajustes perceptuais (iguais ao FastLED rainbow)
    // Hue é multiplicado para se adaptar ao mapeamento rainbow interno (256 steps)
    uint16_t hue8 = (hue_deg * 256 + 180) / 360; // arredonda
    uint8_t offset8 = hue8; // 0..255

    // Boost no amarelo — FastLED usa correção do setor verde-amarelo
    if (offset8 >= 171 && offset8 <= 213) { // faixa aproximada do amarelo
        offset8 = scale8(offset8, 250); // boost
    }

    // Setor (0–5), cada um ~42.6 steps
    uint8_t sector = offset8 / 43;
    uint8_t offset = (offset8 % 43) * 6; // 0..258

    uint8_t inv_sat = 255 - sat;
    uint8_t brightness_floor = scale8_video(val, inv_sat);
    uint8_t color_amplitude  = val - brightness_floor;

    uint8_t rampup   = offset;
    uint8_t rampdown = 255 - offset;

    uint8_t rampup_adj   = scale8_video(rampup,   color_amplitude);
    uint8_t rampdown_adj = scale8_video(rampdown, color_amplitude);

    switch (sector) {
        case 0: // R -> G
            *r = color_amplitude + brightness_floor;
            *g = rampup_adj + brightness_floor;
            *b = brightness_floor;
            break;
        case 1: // G -> R
            *r = rampdown_adj + brightness_floor;
            *g = color_amplitude + brightness_floor;
            *b = brightness_floor;
            break;
        case 2: // G -> B
            *r = brightness_floor;
            *g = color_amplitude + brightness_floor;
            *b = rampup_adj + brightness_floor;
            break;
        case 3: // B -> G
            *r = brightness_floor;
            *g = rampdown_adj + brightness_floor;
            *b = color_amplitude + brightness_floor;
            break;
        case 4: // B -> R
            *r = rampup_adj + brightness_floor;
            *g = brightness_floor;
            *b = color_amplitude + brightness_floor;
            break;
        default: // R -> B
            *r = color_amplitude + brightness_floor;
            *g = brightness_floor;
            *b = rampdown_adj + brightness_floor;
            break;
    }
}