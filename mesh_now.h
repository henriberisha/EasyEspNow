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

static uint8_t ESPNOW_BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t MIN_WIFI_CHANNEL = 0; // if channel would be 0, then set the channel to the default/ or the channel that the radio is actually on
static const uint8_t MAX_WIFI_CHANNEL = 14;
static const uint8_t MAC_ADDR_LEN = ESP_NOW_ETH_ALEN; ///< @brief Address length
static const uint8_t LMK_LENGTH = ESP_NOW_KEY_LEN;
static const uint8_t MAX_TOTAL_PEER_NUM = ESP_NOW_MAX_TOTAL_PEER_NUM;
static const uint8_t MAX_ENCRYPT_PEER_NUM = ESP_NOW_MAX_ENCRYPT_PEER_NUM;
static const uint8_t MAX_DATA_LENGTH = ESP_NOW_MAX_DATA_LEN;

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

typedef struct
{
    uint8_t mac[MAC_ADDR_LEN];
    uint32_t time_peer_added;
} peer_t;

typedef struct
{
    uint8_t peer_number;
    peer_t peer[MAX_TOTAL_PEER_NUM];
} peer_list_t;

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
    uint8_t getAddressLength() override { return MAC_ADDR_LEN; }
    uint8_t getMaxMessageLength() override { return MAX_DATA_LENGTH; }
    void enableTransmit(bool enable) override;

    bool addPeer(const uint8_t *peer_addr_to_add);
    bool deletePeer(const uint8_t *peer_addr_to_delete); // this should delete the peer given by the mac
    uint8_t *deletePeer();                               // this should delete the oldest peer
    peer_t *getPeer(const uint8_t *peer_addr_to_get, esp_now_peer_info_t *peer_info);
    // needed a modify peer TODO
    bool peerExists(const uint8_t *peer_addr);

    void printPeerList();

    void printtest()
    {
        Serial.println(this->wifi_primary_channel);
    }

protected:
    peer_list_t peer_list;
    bool initComms() override;
    static void rx_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    static void tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
};

extern MeshNowEsp meshNowEsp;

#endif