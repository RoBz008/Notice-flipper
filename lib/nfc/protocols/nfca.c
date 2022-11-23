#include "nfca.h"
#include <string.h>
#include <stdio.h>
#include <furi.h>

#define NFCA_CMD_RATS (0xE0U)

#define NFCA_CRC_INIT (0x6363)

#define NFCA_F_SIG    (13560000.0)                    /* [Hz] NFC frequency */
#define NFCA_F_SUB    (NFCA_F_SIG / 16)               /* [Hz] NFC subcarrier frequency fs/16 (847500 Hz) */
#define T_SUB         (1000000000000.0f / NFCA_F_SUB) /* [ps] subcarrier period = 1/NFCA_F_SUB (1.18 µs) */
#define T_SUB_PHASE   (T_SUB / 2)                     /* [ps] a single subcarrier phase (590 µs) */

#define NFCA_SIGNAL_MAX_EDGES (1350)

typedef struct {
    uint8_t cmd;
    uint8_t param;
} nfca_cmd_rats;

static uint8_t nfca_default_ats[] = {0x05, 0x78, 0x80, 0x80, 0x00};

static uint8_t nfca_sleep_req[] = {0x50, 0x00};

uint16_t nfca_get_crc16(uint8_t* buff, uint16_t len) {
    uint16_t crc = NFCA_CRC_INIT;
    uint8_t byte = 0;

    for(uint8_t i = 0; i < len; i++) {
        byte = buff[i];
        byte ^= (uint8_t)(crc & 0xff);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ (((uint16_t)byte) << 8) ^ (((uint16_t)byte) << 3) ^
              (((uint16_t)byte) >> 4);
    }

    return crc;
}

void nfca_append_crc16(uint8_t* buff, uint16_t len) {
    uint16_t crc = nfca_get_crc16(buff, len);
    buff[len] = (uint8_t)crc;
    buff[len + 1] = (uint8_t)(crc >> 8);
}

bool nfca_emulation_handler(
    uint8_t* buff_rx,
    uint16_t buff_rx_len,
    uint8_t* buff_tx,
    uint16_t* buff_tx_len) {
    bool sleep = false;
    uint8_t rx_bytes = buff_rx_len / 8;

    if(rx_bytes == sizeof(nfca_sleep_req) && !memcmp(buff_rx, nfca_sleep_req, rx_bytes)) {
        sleep = true;
    } else if(rx_bytes == sizeof(nfca_cmd_rats) && buff_rx[0] == NFCA_CMD_RATS) {
        memcpy(buff_tx, nfca_default_ats, sizeof(nfca_default_ats));
        *buff_tx_len = sizeof(nfca_default_ats) * 8;
    }

    return sleep;
}

static void nfca_add_byte(NfcaSignal* nfca_signal, uint8_t byte, bool parity) {
    for(uint8_t i = 0; i < 8; i++) {
        if(byte & (1 << i)) {
            digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_d);
        } else {
            digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_e);
        }
    }
    if(parity) {
        digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_d);
    } else {
        digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_e);
    }
}

static void nfca_add_modulation(DigitalSignal* signal, size_t phases) {
    for(size_t i = 0; i < phases; i++) {
        bool modulated = (i % 2 == 0);
        digital_signal_add_pulse(signal, DIGITAL_SIGNAL_PS(T_SUB_PHASE), modulated);
    }
}

static void nfca_add_silence(DigitalSignal* signal, size_t phases) {
    digital_signal_add_pulse(signal, DIGITAL_SIGNAL_PS(phases*T_SUB_PHASE), false);
}

NfcaSignal* nfca_signal_alloc() {
    NfcaSignal* nfca_signal = malloc(sizeof(NfcaSignal));

    /* ISO14443-2 defines 3 sequences for type A communication */
    nfca_signal->seq_d = digital_signal_alloc(10);
    nfca_signal->seq_e = digital_signal_alloc(10);
    nfca_signal->seq_f = digital_signal_alloc(10);

    /* SEQ D has the first half modulated, used as SOF */
    nfca_add_modulation(nfca_signal->seq_d, 8);
    nfca_add_silence(nfca_signal->seq_d, 8);

    /* SEQ E has the second half modulated */
    nfca_add_silence(nfca_signal->seq_e, 8);
    nfca_add_modulation(nfca_signal->seq_e, 8);

    /* SEQ F is just no modulation, used as EOF */
    nfca_add_silence(nfca_signal->seq_f, 16);
    nfca_signal->tx_signal = digital_signal_alloc(NFCA_SIGNAL_MAX_EDGES);

    return nfca_signal;
}

void nfca_signal_free(NfcaSignal* nfca_signal) {
    furi_assert(nfca_signal);

    digital_signal_free(nfca_signal->seq_d);
    digital_signal_free(nfca_signal->seq_e);
    digital_signal_free(nfca_signal->seq_f);
    digital_signal_free(nfca_signal->tx_signal);
    free(nfca_signal);
}

void nfca_signal_encode(NfcaSignal* nfca_signal, uint8_t* data, uint16_t bits, uint8_t* parity) {
    furi_assert(nfca_signal);
    furi_assert(data);
    furi_assert(parity);

    nfca_signal->tx_signal->edge_cnt = 0;
    nfca_signal->tx_signal->start_level = true;

    // Start of frame
    digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_f);
    digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_f);
    digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_f);
    digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_d);

    if(bits < 8) {
        for(size_t i = 0; i < bits; i++) {
            if(FURI_BIT(data[0], i)) {
                digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_d);
            } else {
                digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_e);
            }
        }
    } else {
        for(size_t i = 0; i < bits / 8; i++) {
            nfca_add_byte(nfca_signal, data[i], parity[i / 8] & (1 << (7 - (i & 0x07))));
        }
    }
    digital_signal_append(nfca_signal->tx_signal, nfca_signal->seq_f);
}
