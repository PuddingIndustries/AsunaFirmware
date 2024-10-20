#ifndef LORA_MODEM_H
#define LORA_MODEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    LORA_MODEM_BW_125 = 0, /* 125kHz Bandwidth */
    LORA_MODEM_BW_250,     /* 250kHz Bandwidth */
    LORA_MODEM_BW_500,     /* 500kHz Bandwidth */
    LORA_MODEM_BW_INVALID,
} lora_modem_bw_t;

typedef enum {
    LORA_MODEM_SF_5 = 0, /* Spreading Factor 5 */
    LORA_MODEM_SF_6,     /* Spreading Factor 6 */
    LORA_MODEM_SF_7,     /* Spreading Factor 7 */
    LORA_MODEM_SF_8,     /* Spreading Factor 8 */
    LORA_MODEM_SF_9,     /* Spreading Factor 9 */
    LORA_MODEM_SF_10,    /* Spreading Factor 10 */
    LORA_MODEM_SF_11,    /* Spreading Factor 11 */
    LORA_MODEM_SF_12,    /* Spreading Factor 12 */
    LORA_MODEM_SF_INVALID,
} lora_modem_sf_t;

typedef enum {
    LORA_MODEM_CR_1 = 0, /* Coding Rate 4/5 */
    LORA_MODEM_CR_2,     /* Coding Rate 4/6 */
    LORA_MODEM_CR_3,     /* Coding Rate 4/7 */
    LORA_MODEM_CR_4,     /* Coding Rate 4/8 */
    LORA_MODEM_CR_INVALID,
} lora_modem_cr_t;

typedef struct {
    uint32_t        frequency;
    lora_modem_bw_t bandwidth;
    lora_modem_sf_t spreading_factor;
    lora_modem_cr_t coding_rate;
    bool            ldr_optimization;
} lora_modem_config_t;

int lora_modem_init(const void *context);
int lora_modem_set_config(const void *context, const lora_modem_config_t *config);
int lora_modem_transmit(const void *context, const uint8_t *data, size_t length);

#endif  // LORA_MODEM_H