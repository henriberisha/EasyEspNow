#ifndef MESH_NOW_H
#define MESH_NOW_H

#include "Arduino.h"
#include "easy_debug.h"
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

enum CountPeers
{
	TOTAL_NUM = 0,
	ENCRYPTED_NUM = 1,
	UNENCRYPTED_NUM = 2
};

class EasyEspNow : public CommsHalInterface
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

	/**
	 * @brief Deletes peer with provided MAC address
	 * @param peer_addr_to_delete Peer to delete
	 * @return
	 * 	- `true` if success deleting peer
	 *
	 *  - `false` if failed deleting peer
	 */
	bool deletePeer(const uint8_t *peer_addr_to_delete); // this should delete the peer given by the mac

	/**
	 * @brief Deletes the oldest peer by referencing peers in the `peer_list_t`.
	 * Which peer is deleted is determined by the value of `time_peer_added` of the peer in `peer_list_t`
	 * @param keep_broadcast_addr A boolean value that determines if the Broadcast peer should be deleted or no
	 *
	 * 	- `true`: Broadcast address should be kept and not deleted
	 *
	 *  - `false`: Broadcast address should not be kept and can be deleted
	 * @return
	 * 	- `nullptr` if nothing can be deleted or,
	 *
	 *  - `uint8_t *` pointer to the deleted peer MAC address
	 */
	uint8_t *deletePeer(bool keep_broadcast_addr = true); // this should delete the oldest peer

	peer_t *getPeer(const uint8_t *peer_addr_to_get, esp_now_peer_info_t &peer_info);
	// needed a modify peer TODO

	/**
	 * @brief Checks if peer exists or no
	 * @note A wrapper which makes a call to ESP-NOW function: `esp_now_is_peer_exist(...)`
	 * @param peer_addr Peer address to check for existance
	 * @return
	 * 	- `true` if peer exists
	 *
	 *  - `false` if peer does not exist
	 */
	bool peerExists(const uint8_t *peer_addr);

	/**
	 * @brief Counts the number of peers
	 * @note A wrapper which makes a call to ESP-NOW function: `esp_now_get_peer_num(...)`
	 * @param count_type Argument that has enum type: `CountPeers`
	 *
	 *  - `TOTAL_NUM`: total count of peers
	 *
	 * 	- `ENCRYPTED_NUM`: count of encrypted peers
	 *
	 * 	- `UNENCRYPTED_NUM`: count of unencrypted peers
	 * @return
	 * 	- `-1` if it is unable to get the count or some error occurs
	 *
	 *  - `count` if success getting the count
	 */
	int countPeers(CountPeers count_type);

	/**
	 * @brief Prints the peer list that `peer_list_t` structure keeps as reference to the ESP-NOW peers
	 */
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

extern EasyEspNow easyEspNow;

#endif