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

typedef struct
{
	uint8_t dst_address[MAC_ADDR_LEN];	   /**< Destination MAC*/
	uint8_t payload_data[MAX_DATA_LENGTH]; /**< Payload Content*/
	size_t payload_len;					   /**< Payload length*/
} tx_queue_item_t;

class EasyEspNow : public CommsHalInterface
{
public:
	/* ==========> Easy ESP-NOW Core Functions <========== */

	bool begin(uint8_t channel, wifi_interface_t phy_interface, int tx_q_size = 1, bool synch_send = true) override;

	/**
	 * @brief stops ESP-NOW and TX task
	 * @note deinit ESP-NOW by calling `esp_now_deinit()`
	 */
	void stop() override;

	easy_send_error_t send(const uint8_t *dstAddress, const uint8_t *payload, size_t payload_len) override;

	easy_send_error_t sendBroadcast(const uint8_t *payload, size_t payload_len)
	{
		return send(ESPNOW_BROADCAST_ADDRESS, payload, payload_len);
	}

	void enableTXTask(bool enable) override;

	/**
	 * @brief Function to check readiness to send data in the TX Queue
	 * @note Can be ready to send data to queue whenever there is space in the queue. Good to use when we do not want to drop packets. Can be used in conjunction with `waitForTXQueueToBeEmptied()`
	 * @return
	 * 	- `true` if TX queue i not full
	 *
	 *  - `false` if TX queue is full and there is no more space for new TX items
	 */
	bool readyToSendData();

	/**
	 * @brief Function to block until TX Queue has been exhausted
	 * @note This is good to use when we do not want to drop packets, can be used in conjunction with `readyToSendData()`
	 */
	void waitForTXQueueToBeEmptied();

	void sendTest(int data);

	void onDataReceived(frame_rcvd_data frame_rcvd_cb) override;

	void onDataSent(frame_sent_data frame_sent_cb) override;

	/* ==========> Peer Management Functions <========== */

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

	/* ==========> Miscellaneous Functions <========== */

	/**
	 * @brief Function to return send error `easy_send_error_t` as string
	 * @param send_error error code of type `easy_send_error_t`
	 * @return error code from enum to string
	 */
	const char *easySendErrorToName(easy_send_error_t send_error);

	/**
	 * @brief Autoselects the WiFi interface (needed for peer's `ifidx` info) from the chosen WiFi mode
	 * @note If there is a mismatch between the mode and interface, ESP NOW will not send
	 *
	 * - WIFI_MODE_STA  --->  WIFI_IF_STA
	 *
	 * - WIFI_MODE_AP  --->  WIFI_IF_AP
	 *
	 * - WIFI_MODE_APSTA  --->  WIFI_IF_AP or WIFI_IF_STA (will work either or, does not matter much)
	 *
	 * - WIFI_MODE_NULL or WIFI_MODE_MAX  --->  useless for ESP-NOW
	 * @param mode WiFi mode that is chosen
	 * @param apstaMOD_to_apIF bool value dto choose which WiFi interface to return when mode is `WIFI_MODE_APSTA`
	 *
	 * - `true` user wants interface to be `WIFI_IF_AP` when mode is `WIFI_MODE_APSTA`
	 *
	 * - `false` user wants interface to be `WIFI_IF_STA` when mode is `WIFI_MODE_APSTA`
	 * @return WiFi interface to be used for peer's info structure
	 */
	wifi_interface_t autoselect_if_from_mode(wifi_mode_t mode, bool apstaMOD_to_apIF = true);

	char *easyMac2Char(const uint8_t *some_mac, size_t len = MAC_ADDR_LEN, bool upper_case = true);

	void easyPrintMac2Char(const uint8_t *some_mac, size_t len = MAC_ADDR_LEN, bool upper_case = true);

	/**
	 * @brief Get ESP-NOW version
	 * @return
	 * - -1 if error getting the version
	 *
	 * - version
	 */
	int32_t getEspNowVersion();

	/**
	 * @brief Get Primary channel. This will be the channel that ESP-NOW begins
	 * @return channel that WiFi modem is on and being used by ESP-NOW
	 */
	uint8_t getPrimaryChannel() override { return this->wifi_primary_channel; }

	/**
	 * @brief Get Seconday channel.
	 * @return scondry channel that WiFi modem is on and being used by ESP-NOW
	 */
	wifi_second_chan_t getSecondaryChannel() { return this->wifi_secondary_channel; }

	/**
	 * @brief Get MAC address length. It will be 6
	 * @note MAC address has 6 octets
	 * @return MAC_ADDR_LEN
	 */
	uint8_t getAddressLength() override { return MAC_ADDR_LEN; }

	/**
	 * @brief Get maximum data length that ESp-NOW can send at one time. It will be 250
	 * @note Determined by ESP-NOW API
	 * @return MAX_DATA_LENGTH
	 */
	uint8_t getMaxMessageLength() override { return MAX_DATA_LENGTH; }

	/**
	 * @brief Get MAC address of this device.
	 * @note The mac address will be dependent to the chosen WiFi interface (`wifi_interface_t`).
	 *
	 *  - `WIFI_IF_STA` --> this interface has its own unique MAC
	 *
	 *  - `WIFI_IF_AP`  --> this interface has its own unique MAC
	 * @return
	 * 	- `nullptr` if MAC address has not been able to be set during the call of begin(...) function,
	 *
	 *  - `uint8_t *` pointer to the MAC address location in the memory
	 */
	uint8_t *getDeviceMACAddress();

	/**
	 * @brief Generates a random MAC address with specified address type.
	 *
	 * This function generates a random MAC address and modifies the first byte
	 * based on the `local` and `unicast` parameters. The MAC address can be
	 * configured to be either locally administered or globally unique, and
	 * either unicast or multicast.
	 *
	 * @param local Determines whether the MAC address is locally administered (true) or globally unique (false).
	 *
	 * - If true, the MAC address will have the local bit (bit 1 of the first byte) set to 1.
	 *
	 * - If false, the MAC address will have the local bit cleared to 0.
	 *
	 * @param unicast Determines whether the MAC address is unicast (true) or multicast (false).
	 *
	 * - If true, the MAC address will have the unicast/multicast bit (bit 0 of the first byte) cleared to 0.
	 *
	 * - If false, the MAC address will have the unicast/multicast bit set to 1.
	 *
	 * @return uint8_t* A pointer to a statically allocated array of 6 bytes representing the generated MAC address.
	 *
	 * - The MAC address format is a 6-byte array, with each byte generated randomly.
	 *
	 * - The first byte is modified based on the `local` and `unicast` parameters to meet the
	 *  requirements for local/global and unicast/multicast addressing.
	 *
	 * @note The returned MAC address is stored in a static array, so subsequent calls to this function will
	 *       overwrite the previous MAC address. If you need to retain the address, copy it into a separate array.
	 */
	uint8_t *generateRandomMAC(bool local = true, bool unicast = true);

protected:
	uint8_t zero_mac[MAC_ADDR_LEN] = {0};
	uint8_t my_mac_address[MAC_ADDR_LEN] = {0};

	int tx_queue_size;
	bool synchronous_send;
	bool wait_for_send_confirmation;

	TaskHandle_t txTaskHandle;
	QueueHandle_t txQueue;

	TaskHandle_t txTask_handle; // this is for testing only, will be deleted later
	QueueHandle_t tx_queue;		// this is for testing only, will be deleted later

	int success_process = 0;
	int dropped = 0;

	peer_list_t peer_list;

	/* ==========> Helper Functions for the Core Functions <========== */
	bool initComms() override;

	bool setChannel(uint8_t primary, wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE);

	static void rx_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len);

	static void tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status);

	static void easyEspNowTxQueueTask(void *pvParameters);

	static void processTxQueueTask(void *pvParameters); // this is for testing only, will be deleted later
};

extern EasyEspNow easyEspNow;

#endif