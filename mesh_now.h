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
    // void onDataRcvd(comms_hal_rcvd_data dataRcvd) override;
    // void onDataSent(comms_hal_sent_data sentResult) override;
    void onDataReceived(frame_rcvd_data frame_rcvd_cb) override;
    void onDataSent(frame_sent_data frame_sent_cb) override;
    uint8_t getAddressLength() override { return ESPNOW_ADDR_LEN; }
    uint8_t getMaxMessageLength() override { return ESPNOW_MAX_MESSAGE_LENGTH; }
    void enableTransmit(bool enable) override;

    void printtest() { Serial.println(this->wifi_primary_channel); }

protected:
    bool initComms() override;
    static void rx_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    static void tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
};

extern MeshNowEsp meshNowEsp;

#endif