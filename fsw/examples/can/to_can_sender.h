#ifndef TO_CAN_SENDER_H
#define TO_CAN_SENDER_H

#include "hwlib.h"
#include <stdint.h>

int32_t TO_CanSendCadu(
    can_info_t *device,
    uint32_t radio_id,
    const uint8_t *data,
    uint16_t length);

#endif