#include "lora_modem.h"
/* HAL */
#include "llcc68_hal.h"

llcc68_hal_status_t llcc68_hal_reset(const void *context) {
    const lora_modem_t *modem = (lora_modem_t *)context;

    modem->ops.pin(modem->handle, LORA_MODEM_PIN_RESET, false);
    modem->ops.delay(modem->handle, 1);
    modem->ops.pin(modem->handle, LORA_MODEM_PIN_RESET, true);
    modem->ops.delay(modem->handle, 1);

    return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_wakeup(const void *context) {
    const lora_modem_t *modem = (lora_modem_t *)context;

    modem->ops.pin(modem->handle, LORA_MODEM_PIN_CS, false);
    modem->ops.delay(modem->handle, 1);
    modem->ops.pin(modem->handle, LORA_MODEM_PIN_CS, true);
    modem->ops.delay(modem->handle, 1);

    return LLCC68_HAL_STATUS_OK;
}

llcc68_hal_status_t llcc68_hal_read(const void *context, const uint8_t *command, const uint16_t command_length,
                                    uint8_t *data, const uint16_t data_length) {
    llcc68_hal_status_t ret   = LLCC68_HAL_STATUS_OK;
    const lora_modem_t *modem = (lora_modem_t *)context;

    modem->ops.wait_busy(modem->handle);

    modem->ops.pin(modem->handle, LORA_MODEM_PIN_CS, false);

    lora_modem_spi_transfer_t xfer;

    if (command_length) {
        xfer.tx_data = command;
        xfer.rx_data = NULL;
        xfer.length  = command_length;

        if (modem->ops.spi(modem->handle, &xfer) != 0) {
            ret = LLCC68_HAL_STATUS_ERROR;
            goto release_cs_exit;
        }
    }

    if (data_length) {
        xfer.tx_data = NULL;
        xfer.rx_data = data;
        xfer.length  = data_length;

        if (modem->ops.spi(modem->handle, &xfer) != 0) {
            ret = LLCC68_HAL_STATUS_ERROR;
        }
    }

release_cs_exit:
    modem->ops.pin(modem->handle, LORA_MODEM_PIN_CS, true);

    return ret;
}

llcc68_hal_status_t llcc68_hal_write(const void *context, const uint8_t *command, const uint16_t command_length,
                                     const uint8_t *data, const uint16_t data_length) {
    llcc68_hal_status_t ret   = LLCC68_HAL_STATUS_OK;
    const lora_modem_t *modem = (lora_modem_t *)context;

    modem->ops.wait_busy(modem->handle);

    modem->ops.pin(modem->handle, LORA_MODEM_PIN_CS, false);

    lora_modem_spi_transfer_t xfer;

    if (command_length) {
        xfer.tx_data = command;
        xfer.rx_data = NULL;
        xfer.length  = command_length;

        ;
        if (modem->ops.spi(modem->handle, &xfer)) {
            ret = LLCC68_HAL_STATUS_ERROR;
            goto release_cs_exit;
        }
    }

    if (data_length) {
        xfer.tx_data = data;
        xfer.length  = data_length;

        if (modem->ops.spi(modem->handle, &xfer) != 0) {
            ret = LLCC68_HAL_STATUS_ERROR;
        }
    }

release_cs_exit:
    modem->ops.pin(modem->handle, LORA_MODEM_PIN_CS, true);

    return ret;
}
