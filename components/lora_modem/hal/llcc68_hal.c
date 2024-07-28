#include "llcc68_hal_init.h"

llcc68_hal_status_t llcc68_hal_reset(const void *context) {
    const llcc68_hal_context_t *ctx = (llcc68_hal_context_t *)context;

    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_RESET, false);
    ctx->delay(ctx->handle, 1);
    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_RESET, true);
    ctx->delay(ctx->handle, 1);

    return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_wakeup(const void *context) {
    const llcc68_hal_context_t *ctx = (llcc68_hal_context_t *)context;

    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_CS, false);
    ctx->delay(ctx->handle, 1);
    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_CS, true);
    ctx->delay(ctx->handle, 1);

    return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_read(const void *context, const uint8_t *command, const uint16_t command_length,
                                    uint8_t *data, const uint16_t data_length) {
    llcc68_hal_status_t         ret;
    const llcc68_hal_context_t *ctx = (llcc68_hal_context_t *)context;

    ctx->wait_busy(ctx->handle);

    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_CS, false);

    llcc68_hal_spi_transfer_t xfer = {
        .tx_data = command,
        .rx_data = NULL,

        .length = command_length,
    };

    ret = ctx->spi_ops(ctx->handle, &xfer);
    if (ret != LLCC68_HAL_STATUS_OK) {
        goto release_cs_exit;
    }

    xfer.tx_data = NULL;
    xfer.rx_data = data;
    xfer.length  = data_length;

    ret = ctx->spi_ops(ctx->handle, &xfer);

release_cs_exit:
    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_CS, true);

    return ret;
}

llcc68_hal_status_t llcc68_hal_write(const void *context, const uint8_t *command, const uint16_t command_length,
                                     const uint8_t *data, const uint16_t data_length) {
    llcc68_hal_status_t         ret;
    const llcc68_hal_context_t *ctx = (llcc68_hal_context_t *)context;

    ctx->wait_busy(ctx->handle);

    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_CS, false);

    llcc68_hal_spi_transfer_t xfer = {
        .tx_data = command,
        .rx_data = NULL,

        .length = command_length,
    };

    ret = ctx->spi_ops(ctx->handle, &xfer);
    if (ret != LLCC68_HAL_STATUS_OK) {
        goto release_cs_exit;
    }

    xfer.tx_data = data;
    xfer.length  = data_length;

    ret = ctx->spi_ops(ctx->handle, &xfer);

release_cs_exit:
    ctx->pin_ops(ctx->handle, LLCC68_HAL_PIN_CS, true);

    return ret;
}
