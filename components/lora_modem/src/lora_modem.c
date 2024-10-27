#include "lora_modem.h"

#include "llcc68.h"

#define LORA_MODEM_ERROR_CHECK(step, x)   \
    {                                     \
        status = x;                       \
        if (status != LLCC68_STATUS_OK) { \
            return -step;                 \
        }                                 \
    }

static const llcc68_lora_bw_t s_lora_modem_bw_table[] = {
    [LORA_MODEM_BW_125] = LLCC68_LORA_BW_125,
    [LORA_MODEM_BW_250] = LLCC68_LORA_BW_250,
    [LORA_MODEM_BW_500] = LLCC68_LORA_BW_500,
};

static const llcc68_lora_sf_t s_lora_modem_sf_table[] = {
    [LORA_MODEM_SF_5] = LLCC68_LORA_SF5,   [LORA_MODEM_SF_6] = LLCC68_LORA_SF6, [LORA_MODEM_SF_7] = LLCC68_LORA_SF7,
    [LORA_MODEM_SF_8] = LLCC68_LORA_SF8,   [LORA_MODEM_SF_9] = LLCC68_LORA_SF9, [LORA_MODEM_SF_10] = LLCC68_LORA_SF10,
    [LORA_MODEM_SF_11] = LLCC68_LORA_SF11,
};

static const llcc68_lora_cr_t s_lora_modem_cr_table[] = {
    [LORA_MODEM_CR_1] = LLCC68_LORA_CR_4_5,
    [LORA_MODEM_CR_2] = LLCC68_LORA_CR_4_6,
    [LORA_MODEM_CR_3] = LLCC68_LORA_CR_4_7,
    [LORA_MODEM_CR_4] = LLCC68_LORA_CR_4_8,
};

static const uint8_t s_lora_modem_sync_word_table[][2] = {
    [LORA_MODEM_NETWORK_PUBLIC]  = {0x34, 0x44},
    [LORA_MODEM_NETWORK_PRIVATE] = {0x14, 0x24},
};

int lora_modem_init(const lora_modem_t *modem) {
    llcc68_status_t status;

    LORA_MODEM_ERROR_CHECK(1, llcc68_reset(modem));
    LORA_MODEM_ERROR_CHECK(2, llcc68_init_retention_list(modem));
    LORA_MODEM_ERROR_CHECK(3, llcc68_set_reg_mode(modem, LLCC68_REG_MODE_DCDC));
    LORA_MODEM_ERROR_CHECK(4, llcc68_set_dio2_as_rf_sw_ctrl(modem, true));
    LORA_MODEM_ERROR_CHECK(5, llcc68_set_rx_tx_fallback_mode(modem, LLCC68_FALLBACK_STDBY_XOSC));
    LORA_MODEM_ERROR_CHECK(6, llcc68_set_pkt_type(modem, LLCC68_PKT_TYPE_LORA));
    LORA_MODEM_ERROR_CHECK(7, llcc68_cal(modem, LLCC68_CAL_ALL));
    LORA_MODEM_ERROR_CHECK(8, llcc68_cal_img_in_mhz(modem, 868, 915));

    const uint16_t irq_mask = LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE;

    LORA_MODEM_ERROR_CHECK(9, llcc68_set_dio_irq_params(modem, irq_mask, irq_mask, 0x00, 0x00));

    return 0;
}

int lora_modem_set_config(const lora_modem_t *modem, const lora_modem_config_t *config) {
    llcc68_status_t status;

    const llcc68_mod_params_lora_t params = {
        .bw   = s_lora_modem_bw_table[config->bandwidth],
        .sf   = s_lora_modem_sf_table[config->spreading_factor],
        .cr   = s_lora_modem_cr_table[config->coding_rate],
        .ldro = config->ldr_optimization,
    };

    const llcc68_pa_cfg_params_t pa_params = {
        .pa_duty_cycle = 0x04,
        .hp_max        = 0x07,
        .device_sel    = 0x00,
        .pa_lut        = 0x01,
    };

    const uint8_t *sync_word = s_lora_modem_sync_word_table[config->network_type];

    LORA_MODEM_ERROR_CHECK(1, llcc68_cfg_tx_clamp(modem));
    LORA_MODEM_ERROR_CHECK(2, llcc68_set_tx_params(modem, config->power, LLCC68_RAMP_40_US));
    LORA_MODEM_ERROR_CHECK(3, llcc68_set_lora_mod_params(modem, &params));
    LORA_MODEM_ERROR_CHECK(4, llcc68_set_rf_freq(modem, config->frequency));
    LORA_MODEM_ERROR_CHECK(5, llcc68_set_pa_cfg(modem, &pa_params));
    LORA_MODEM_ERROR_CHECK(6, llcc68_write_register(modem, 0x740, sync_word, 2U));

    return 0;
}

int lora_modem_transmit(const lora_modem_t *modem, const uint8_t *data, size_t length) {
    llcc68_status_t status;

    const llcc68_pkt_params_lora_t pkt_params = {
        .header_type          = LLCC68_LORA_PKT_EXPLICIT,
        .preamble_len_in_symb = 12,
        .pld_len_in_bytes     = length,
        .crc_is_on            = true,
    };

    LORA_MODEM_ERROR_CHECK(1, llcc68_set_lora_pkt_params(modem, &pkt_params));
    LORA_MODEM_ERROR_CHECK(2, llcc68_set_buffer_base_address(modem, 0, 0xFF));
    LORA_MODEM_ERROR_CHECK(3, llcc68_write_buffer(modem, 0, data, length));
    LORA_MODEM_ERROR_CHECK(4, llcc68_set_tx(modem, 0));

    return 0;
}

void lora_modem_handle_interrupt(const lora_modem_t *modem) {
    llcc68_irq_mask_t irq_mask;
    llcc68_get_and_clear_irq_status(modem, &irq_mask);

    if (modem->cb == NULL) {
        return;
    }

    if (irq_mask & LLCC68_IRQ_TX_DONE) {
        modem->cb(modem->handle, LORA_MODEM_CB_EVENT_TX_DONE);
    }

    if (irq_mask & LLCC68_IRQ_RX_DONE) {
        modem->cb(modem->handle, LORA_MODEM_CB_EVENT_RX_DONE);
    }
}