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

typedef enum {
    LORA_MODEM_NETWORK_PUBLIC,  /* Public network, Sync word 0x3444 */
    LORA_MODEM_NETWORK_PRIVATE, /* Private network, sync word 0x1424 */
} lora_modem_network_type_t;

typedef enum {
    LORA_MODEM_PIN_CS,
    LORA_MODEM_PIN_RESET,
} lora_modem_pin_t;

typedef struct {
    const uint8_t *tx_data;
    uint8_t       *rx_data;

    size_t length;
} lora_modem_spi_transfer_t;

typedef void (*lora_modem_cb_tx_done_fn_t)(void *handle);
typedef void (*lora_modem_cb_rx_done_fn_t)(void *handle, uint8_t *data, size_t len);

typedef int (*lora_modem_ops_spi_fn_t)(void *handle, lora_modem_spi_transfer_t *transfer);
typedef int (*lora_modem_ops_pin_fn_t)(void *handle, lora_modem_pin_t pin, bool value);
typedef int (*lora_modem_ops_wait_busy_fn_t)(void *handle);
typedef int (*lora_modem_delay_fn_t)(void *handle, uint32_t delay_ms);

typedef struct {
    lora_modem_ops_spi_fn_t       spi;
    lora_modem_ops_pin_fn_t       pin;
    lora_modem_ops_wait_busy_fn_t wait_busy;
    lora_modem_delay_fn_t         delay;
} lora_modem_ops_t;

typedef struct {
    lora_modem_cb_rx_done_fn_t rx_done;
    lora_modem_cb_tx_done_fn_t tx_done;
} lora_modem_cb_t;

typedef struct {
    uint32_t frequency;
    uint8_t  power;

    lora_modem_network_type_t network_type;

    lora_modem_bw_t bandwidth;
    lora_modem_sf_t spreading_factor;
    lora_modem_cr_t coding_rate;
    bool            ldr_optimization;
} lora_modem_config_t;

typedef struct {
    lora_modem_cb_t  cb;
    lora_modem_ops_t ops;

    void *handle;
} lora_modem_t;

int  lora_modem_init(const lora_modem_t *modem);
int  lora_modem_set_config(const lora_modem_t *modem, const lora_modem_config_t *config);
int  lora_modem_transmit(const lora_modem_t *modem, const uint8_t *data, size_t length);
void lora_modem_handle_interrupt(const lora_modem_t *modem);

#endif  // LORA_MODEM_H