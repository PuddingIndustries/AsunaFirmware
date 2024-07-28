#ifndef LLCC68_HAL_INIT_H
#define LLCC68_HAL_INIT_H

#include <stddef.h>

#include "llcc68_hal.h"

typedef enum {
    LLCC68_HAL_PIN_CS,
    LLCC68_HAL_PIN_RESET,
} llcc68_hal_pin_t;

typedef struct {
    const uint8_t *tx_data;
    uint8_t       *rx_data;

    size_t length;
} llcc68_hal_spi_transfer_t;

typedef llcc68_hal_status_t (*llcc68_hal_spi_ops_t)(void *handle, llcc68_hal_spi_transfer_t *xfer);
typedef llcc68_hal_status_t (*llcc68_hal_pin_ops_t)(void *handle, llcc68_hal_pin_t pin, bool value);
typedef llcc68_hal_status_t (*llcc68_hal_wait_busy_t)(void *handle);
typedef llcc68_hal_status_t (*llcc68_hal_delay_t)(void *handle, uint32_t msec);

typedef struct {
    llcc68_hal_spi_ops_t   spi_ops;
    llcc68_hal_pin_ops_t   pin_ops;
    llcc68_hal_wait_busy_t wait_busy;
    llcc68_hal_delay_t     delay;

    void *handle;
} llcc68_hal_context_t;

#endif  // LLCC68_HAL_INIT_H
