#ifndef MESH_NOW_H
#define MESH_NOW_H

#include "Arduino.h"
#include "debug_mesh.h"
#include "comms_hal_interface.h"

#include <WiFi.h>
#include <esp_now.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>

const wifi_promiscuous_filter_t filter_mgmt = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT}; // this filter MGMT frames

typedef struct
{
    unsigned protocol : 2;
    unsigned type : 2;
    unsigned subtype : 4;
    uint8_t flags;
    uint16_t duration;
    uint8_t destination_address[6];
    uint8_t source_address[6];
    uint8_t broadcast_address[6];
    uint16_t sequence_control;

    uint8_t category_code;
    uint8_t organization_identifier[3]; // 0x18fe34
    uint8_t random_values[4];
    struct
    {
        uint8_t element_id;                 // 0xdd
        uint8_t lenght;                     //
        uint8_t organization_identifier[3]; // 0x18fe34
        uint8_t type;                       // 4
        uint8_t version;
        uint8_t body[0];
    } vendor_specific_content;
    uint8_t fcs[4];
} __attribute__((packed)) espnow_frame_format_t;

/*
#define STATION_MODE true
#if STATION_MODE
wifi_mode_t _mode = WIFI_MODE_STA;
wifi_interface_t _interface = WIFI_IF_STA;
#else
wifi_mode_t _mode = WIFI_MODE_AP;
wifi_interface_t _interface = WIFI_IF_AP;
#endif

*/

class MeshNowEsp : public CommsHalInterface
{
public:
    bool begin(uint8_t channel, wifi_interface_t phy_interface) override;
    void stop() override;
    int32_t getEspNowVersion();
    uint8_t getPrimaryChannel() override { return this->wifi_primary_channel; }
    wifi_second_chan_t getSecondaryChannel() { return this->wifi_secondary_channel; }

    bool setChannel(uint8_t primary, wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE) override;

    comms_send_error_t send(const uint8_t *dstAddress, const uint8_t *payload, size_t payload_len) override;
    comms_send_error_t sendBroadcast(const uint8_t *payload, size_t payload_len)
    {
        return send(ESPNOW_BROADCAST_ADDRESS, payload, payload_len);
    }
    void onDataRcvd(comms_hal_rcvd_data dataRcvd) override;
    void onDataSent(comms_hal_sent_data sentResult) override;
    uint8_t getAddressLength() override { return ESPNOW_ADDR_LEN; }
    uint8_t getMaxMessageLength() override { return ESPNOW_MAX_MESSAGE_LENGTH; }
    void enableTransmit(bool enable) override;

    void printtest() { Serial.println(this->wifi_primary_channel); }

protected:
    void initComms() override;
};

class Test
{
public:
    bool begin();
};

// extern MeshNowEsp mesh;

#endif