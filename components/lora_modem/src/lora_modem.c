#include "lora_modem.h"

#include "llcc68.h"

#define LORA_MODEM_ERROR_CHECK(x)         \
    {                                     \
        status = x;                       \
        if (status != LLCC68_STATUS_OK) { \
            return -1;                    \
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

int lora_modem_init(const void *context) {
    llcc68_status_t status;

    LORA_MODEM_ERROR_CHECK(llcc68_reset(context));
    LORA_MODEM_ERROR_CHECK(llcc68_init_retention_list(context));
    LORA_MODEM_ERROR_CHECK(llcc68_set_reg_mode(context, LLCC68_REG_MODE_DCDC));
    LORA_MODEM_ERROR_CHECK(llcc68_set_dio2_as_rf_sw_ctrl(context, true));
    LORA_MODEM_ERROR_CHECK(llcc68_set_dio3_as_tcxo_ctrl(context, LLCC68_TCXO_CTRL_3_3V, 500));
    LORA_MODEM_ERROR_CHECK(llcc68_cal_img_in_mhz(context, 470, 510));

    return 0;
}

int lora_modem_set_config(const void *context, const lora_modem_config_t *config) {
    llcc68_status_t status;

    llcc68_mod_params_lora_t params = {
        .bw   = s_lora_modem_bw_table[config->bandwidth],
        .sf   = s_lora_modem_sf_table[config->spreading_factor],
        .cr   = s_lora_modem_cr_table[config->coding_rate],
        .ldro = config->ldr_optimization,
    };

    LORA_MODEM_ERROR_CHECK(llcc68_set_lora_mod_params(context, &params));
    LORA_MODEM_ERROR_CHECK(llcc68_set_rf_freq(context, config->frequency));

    return 0;
}

int lora_modem_transmit(const void *context, const uint8_t *data, size_t length) {
    return 0;
}