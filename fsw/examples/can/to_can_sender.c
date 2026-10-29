#include "to_can_sender.h"
#include <stddef.h>
#include <string.h>

static int32_t TO_CanExchange(can_info_t *device, uint32_t radio_id,
                            const uint8_t *payload, uint8_t length,
                            uint16_t expected_offset)
{
    device->tx_frame=(struct can_frame){0};
    device->rx_frame=(struct can_frame){0};
    device->tx_frame.can_id=radio_id;
    device->tx_frame.can_dlc=length;
    memcpy(device->tx_frame.data,payload,length);
    int32_t rc = can_master_transaction(device);

    if (rc != CAN_SUCCESS)
    {
        OS_printf(
            "TO CAN transaction failed: cmd=0x%02X id=%u "
            "expected_offset=%u rc=%ld\n",
            payload[0], payload[1],
            expected_offset, (long)rc);

        return OS_ERROR;
    }
    const struct can_frame *reply = &device->rx_frame;

    const uint16_t next =
        ((uint16_t)reply->data[3] << 8) |
        reply->data[4];

    if (reply->can_id != radio_id ||
        reply->can_dlc != 5 ||
        reply->data[0] != payload[0] ||
        reply->data[1] != 0 ||
        reply->data[2] != payload[1] ||
        next != expected_offset)
    {
        OS_printf(
            "TO CAN ACK rejected: "
            "tx_cmd=0x%02X tx_id=%u "
            "rx_can_id=%lu expected_can_id=%lu "
            "dlc=%u rx_cmd=0x%02X status=%u rx_id=%u "
            "next=%u expected=%u\n",
            payload[0],
            payload[1],
            (unsigned long)reply->can_id,
            (unsigned long)radio_id,
            reply->can_dlc,
            reply->data[0],
            reply->data[1],
            reply->data[2],
            next,
            expected_offset);

        return OS_ERROR;
    }

    return OS_SUCCESS;
}

int32_t TO_CanSendCadu(can_info_t *device, uint32_t radio_id,
                     const uint8_t *data, uint16_t length)
{
    // Only TO's sending task may call this routine. Other CAN users can run
    // concurrently if the shared hwlib transaction API is serialized.
    static uint8_t transfer_id=0;
    if (device==NULL || data==NULL || length==0 || length>8192)
        return OS_ERROR;
    const uint8_t id=++transfer_id;
    uint8_t frame[8]={0x10,id,(uint8_t)(length>>8),(uint8_t)length};
    if (TO_CanExchange(device,radio_id,frame,4,0)!=OS_SUCCESS)
        return OS_ERROR;
    for (uint16_t offset=0; offset<length;)
    {
        const uint16_t left=length-offset;
        const uint8_t count=(uint8_t)(left>4 ? 4 : left);
        frame[0]=0x11; frame[1]=id;
        frame[2]=(uint8_t)(offset>>8); frame[3]=(uint8_t)offset;
        memcpy(frame+4,data+offset,count);
        if (TO_CanExchange(device,radio_id,frame,(uint8_t)(4+count),
                           (uint16_t)(offset+count))!=OS_SUCCESS)
            return OS_ERROR;
        offset=(uint16_t)(offset+count);
    }
    frame[0]=0x12; frame[1]=id;
    return TO_CanExchange(device,radio_id,frame,2,length);
}
