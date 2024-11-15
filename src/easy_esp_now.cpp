#ifdef ESP32

#include "easy_esp_now.h"

EasyEspNow easyEspNow;

constexpr auto TAG_CORE = "EASY_ESP_NOW";
constexpr auto TAG_PEERS = "PEER_MANAGER";
constexpr auto TAG_MISC = "MISCELLANEOUS";
constexpr auto TAG_HELPER = "HELPER";

/* ==========> Easy ESP-NOW Core Functions <========== */

bool EasyEspNow::begin(uint8_t channel, wifi_interface_t phy_interface, int tx_q_size, bool synch_send)
{
	wifi_mode_t mode;
	err = esp_wifi_get_mode(&mode);
	if (err == ESP_OK)
	{
		if (mode == WIFI_MODE_NULL || mode == WIFI_MODE_MAX)
		{
			ERROR(TAG_CORE, "WiFi is not initialized. WIFI_MODE_NULL or WIFI_MODE_MAX");
			VERBOSE(TAG_CORE, "WiFi needs to be initialized to begin the mesh, with the correct mode. Try function: WiFi.mode(...) in the setup of your main sketch");
			return false;
		}
		else
		{
			MONITOR(TAG_CORE, "WiFi is initialized.");
		}
	}
	else
	{
		ERROR(TAG_CORE, "WiFi is not initialized. Error: %s\n", esp_err_to_name(err));
		return false;
	}

	if ((mode == WIFI_MODE_STA && phy_interface != WIFI_IF_STA) ||
		(mode == WIFI_MODE_AP && phy_interface != WIFI_IF_AP))
	{
		ERROR(TAG_CORE, "WiFi mode and WiFi physical interface mismatch");
		VERBOSE(TAG_CORE, "WiFi mode and WiFi physical interface must match: WIFI_MODE_STA <-> WIFI_IF_STA; WIFI_MODE_AP <-> WIFI_IF_AP");
		return false;
	}

	// Get current wifi channel the radio is on
	err = esp_wifi_get_channel(&wifi_primary_channel, &wifi_secondary_channel);
	if (err == ESP_OK)
	{
		MONITOR(TAG_CORE, "WiFi is on default channel: %d", wifi_primary_channel);
	}
	else
	{
		MONITOR(TAG_CORE, "Failed to get WiFi channel with error: %s\n", esp_err_to_name(err));
		return false;
	}

	if (channel < MIN_WIFI_CHANNEL || channel > MAX_WIFI_CHANNEL)
	{
		ERROR(TAG_CORE, "WiFi channel selection out of range. You selected: %d", channel);
		VERBOSE(TAG_CORE, "WiFi channel out of range, select channel [0...14]. Channel = 0 => device will keep the channel that the radio is on");
		return false;
	}
	else if (channel == 0)
	{
		MONITOR(TAG_CORE, "WiFi channel selection = %d. WiFi radio will remain on default channel: %d", channel, wifi_primary_channel);
	}
	else
	{
		if (this->setChannel(channel) == false)
		{
			ERROR(TAG_CORE, "Failed to set desired WiFi channel");
			return false;
		}
	}

	// initcomms here

	if (tx_q_size < 1)
	{
		ERROR(TAG_CORE, "TX Queue size is set to invalid number: %d. Must be greater than 0", tx_q_size);
		return false;
	}

	this->tx_queue_size = tx_q_size;
	this->synchronous_send = synch_send;

	if (initComms() == false)
		return false;

	this->wifi_mode = mode;
	this->wifi_phy_interface = phy_interface;

	err = esp_wifi_get_mac(this->wifi_phy_interface, this->my_mac_address);
	if (err == ESP_OK)
	{
		INFO(TAG_CORE, "This device's MAC is avaiable. Success getting device's self MAC address for the chosen WiFi interface.");
	}
	else
	{
		WARNING(TAG_CORE, "This device's MAC is not avaiable. Failed getting device's self MAC address with error: %s", esp_err_to_name(err));
	}

	return true;
}

void EasyEspNow::stop()
{
	MONITOR(TAG_CORE, "----------> STOPPING ESP-NOW");
	vTaskDelete(txTaskHandle);
	vQueueDelete(txQueue);
	esp_now_unregister_recv_cb();
	esp_now_unregister_send_cb();
	esp_now_deinit();
	MONITOR(TAG_CORE, "<---------- ESP-NOW STOPPED");
}

easy_send_error_t EasyEspNow::send(const uint8_t *dstAddress, const uint8_t *payload, size_t payload_len)
{
	if (!payload || !payload_len)
	{
		ERROR(TAG_CORE, "Parameters Error");
		return EASY_SEND_PARAM_ERROR;
	}

	if (payload_len < 1 || payload_len > MAX_DATA_LENGTH)
	{
		ERROR(TAG_CORE, "Length: %d. Payload length must be between [Min, Max]: [%d ... %d] bytes", payload_len, 1, MAX_DATA_LENGTH);
		return EASY_SEND_PAYLOAD_LENGTH_ERROR;
	}

	int enqueued_tx_messages = uxQueueMessagesWaiting(txQueue);
	DEBUG(TAG_CORE, "TX Queue Status (Enqueued | Capacity) -> %d | %d\n", enqueued_tx_messages, easyEspNow.tx_queue_size);

	// in synch mode wait here until the message in the queue is removed and sent
	if (this->synchronous_send)
	{
		while (uxQueueMessagesWaiting(txQueue) == tx_queue_size)
		{
			WARNING(TAG_CORE, "Synchronous send mode. Waiting for free space in TX Queue");
			taskYIELD();
		}
	}
	else
	{
		if (enqueued_tx_messages == tx_queue_size)
		{
			WARNING(TAG_CORE, "TX Queue full. Can not add message to queue. Dropping message...");
			return EASY_SEND_QUEUE_FULL_ERROR;
		}
	}

	tx_queue_item_t item_to_enqueue;

	// in case dst address in null, put [0x00, 0x00, 0x00, 0x00, 0x00, 0x00] as destination
	// this will tell to send the message to all the peers in the list
	if (!dstAddress)
		memcpy(item_to_enqueue.dst_address, zero_mac, ESP_NOW_ETH_ALEN);
	else
		memcpy(item_to_enqueue.dst_address, dstAddress, ESP_NOW_ETH_ALEN);

	memcpy(item_to_enqueue.payload_data, payload, payload_len);
	item_to_enqueue.payload_len = payload_len;

	// portMAX_DELAY -> will wait indefinitely
	// pdMS_TO_TICKS -> will have a timeout
	if (xQueueSend(txQueue, &item_to_enqueue, pdMS_TO_TICKS(10)) == pdTRUE)
	{
		MONITOR(TAG_CORE, "Success to enqueue TX message");
		return EASY_SEND_OK;
	}
	else
	{
		WARNING(TAG_CORE, "Failed to enqueue item");
		return EASY_SEND_MSG_ENQUEUE_ERROR;
	}
}

void EasyEspNow::enableTXTask(bool enable)
{
	if (!txTaskHandle)
	{
		WARNING(TAG_CORE, "Unable to resume or suspend TX Task because it has not been created yet!");
		return;
	}

	if (enable && tx_task_resumed == false)
	{
		INFO(TAG_CORE, "Resuming TX Task ...");
		vTaskResume(txTaskHandle);
		tx_task_resumed = true;
	}

	else if (enable == false && tx_task_resumed == true)
	{
		INFO(TAG_CORE, "Suspending TX Task ...");
		vTaskSuspend(txTaskHandle);
		tx_task_resumed = false;
	}

	return;
}

bool EasyEspNow::readyToSendData()
{
	return uxQueueMessagesWaiting(txQueue) < tx_queue_size;
}

void EasyEspNow::waitForTXQueueToBeEmptied()
{
	if (easyEspNow.txQueue == NULL)
	{
		WARNING(TAG_CORE, "TX Queue can't be emptied because it has not been initialized...");
		return;
	}

	WARNING(TAG_CORE, "Waiting for TX Queue to be emptied...");
	// if the task is suspended no need to continue blocking, otherwise will be stuck here
	while (uxQueueMessagesWaiting(easyEspNow.txQueue) > 0 && tx_task_resumed == true)
	{
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	return;
}

void EasyEspNow::onDataReceived(frame_rcvd_data frame_rcvd_cb)
{
	DEBUG(TAG_CORE, "Registering custom onReceive Callback Function");
	dataReceived = frame_rcvd_cb;
}

void EasyEspNow::onDataSent(frame_sent_data frame_sent_cb)
{
	DEBUG(TAG_CORE, "Registering custom onSent Callback function");
	dataSent = frame_sent_cb;
}

/* ==========> Peer Management Functions <========== */

bool EasyEspNow::addPeer(const uint8_t *peer_addr_to_add)
{
	// peer can be in a different interface from the home (this station) and still receive the message.
	esp_now_peer_info_t peer_info;
	memcpy(peer_info.peer_addr, peer_addr_to_add, MAC_ADDR_LEN);
	peer_info.ifidx = wifi_phy_interface; // this does not really matter to set it the same as the peer. This is relevant to the home station WiFi mode and interface. ESP_ERR_ESPNOW_IF
	peer_info.channel = wifi_primary_channel;
	peer_info.encrypt = false;

	err = esp_now_add_peer(&peer_info);
	if (err == ESP_OK)
	{
		memcpy(peer_list.peer[peer_list.peer_number].mac, peer_addr_to_add, MAC_ADDR_LEN);
		peer_list.peer[peer_list.peer_number].time_peer_added = millis();
		peer_list.peer_number++;

		MONITOR(TAG_PEERS, "Successfully added peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_addr_to_add), peer_list.peer_number);
		return true;
	}
	else
	{
		ERROR(TAG_PEERS, "Failed to add peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_addr_to_add), esp_err_to_name(err));
		return false;
	}
}

bool EasyEspNow::deletePeer(const uint8_t *peer_addr_to_delete)
{
	err = esp_now_del_peer(peer_addr_to_delete);
	if (err == ESP_OK)
	{
		for (int i = 0; i < peer_list.peer_number; i++)
		{
			if (memcmp(peer_list.peer[i].mac, peer_addr_to_delete, MAC_ADDR_LEN) == 0)
			{
				// Peer found, shift subsequent peers to fill the gap
				for (int j = i; j < peer_list.peer_number - 1; j++)
				{
					peer_list.peer[j] = peer_list.peer[j + 1];
				}
				// Decrease the peer count
				peer_list.peer_number--;
				break;
			}
		}

		MONITOR(TAG_PEERS, "Successfully deleted peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_addr_to_delete), peer_list.peer_number);
		return true;
	}
	else
	{
		ERROR(TAG_PEERS, "Failed to delete peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_addr_to_delete), esp_err_to_name(err));
		return false;
	}
}

uint8_t *EasyEspNow::deletePeer(bool keep_broadcast_addr)
{
	if (peer_list.peer_number == 0)
	{
		WARNING(TAG_PEERS, "Peer List is empty. No peer can be deleted!");
		return nullptr;
	}

	int8_t oldest_index = -1;				// Initialize it with invalid index
	uint32_t oldest_peer_time = UINT32_MAX; // Start with max value

	for (int i = 0; i < peer_list.peer_number; i++)
	{
		//  Time, is saved in millis, time increases, so older peers will have smaller time value as they were adder earlier
		if (keep_broadcast_addr && memcmp(peer_list.peer[i].mac, ESPNOW_BROADCAST_ADDRESS, MAC_ADDR_LEN) == 0)
		{
			DEBUG(TAG_PEERS, "Broadcast MAC detected");
			continue;
		}
		else
		{
			if (peer_list.peer[i].time_peer_added < oldest_peer_time)
			{
				oldest_peer_time = peer_list.peer[i].time_peer_added;
				oldest_index = i;
			}
		}
	}

	DEBUG(TAG_PEERS, "Oldest index: %d", oldest_index);

	if (oldest_index == -1)
	{
		WARNING(TAG_PEERS, "No valid peer found to delete!");
		return nullptr; // No valid peer to delete
	}

	uint8_t *peer_mac_to_delete = (uint8_t *)malloc(MAC_ADDR_LEN);
	memcpy(peer_mac_to_delete, peer_list.peer[oldest_index].mac, MAC_ADDR_LEN);

	err = esp_now_del_peer(peer_mac_to_delete);
	if (err == ESP_OK)
	{
		// Peer found, shift subsequent peers to fill the gap
		for (int j = oldest_index; j < peer_list.peer_number - 1; j++)
		{
			peer_list.peer[j] = peer_list.peer[j + 1];
		}
		// Decrease the peer count
		peer_list.peer_number--;
		MONITOR(TAG_PEERS, "Successfully deleted peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_mac_to_delete), peer_list.peer_number);
		return peer_mac_to_delete;
	}
	else
	{
		ERROR(TAG_PEERS, "Failed to delete peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_mac_to_delete), esp_err_to_name(err));
		return nullptr;
	}
}

peer_t EasyEspNow::getPeer(const uint8_t *peer_addr_to_get, esp_now_peer_info_t &peer_info)
{
	peer_t peer = {}; // for an invalid peer
	err = esp_now_get_peer(peer_addr_to_get, &peer_info);
	if (err == ESP_OK)
	{
		for (int i = 0; i < peer_list.peer_number; i++)
		{
			if (memcmp(peer_list.peer[i].mac, peer_addr_to_get, MAC_ADDR_LEN) == 0)
			{
				DEBUG(TAG_PEERS, "Success getting peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_addr_to_get), peer_list.peer_number);
				return peer_list.peer[i];
			}
		}
	}
	else
	{
		ERROR(TAG_PEERS, "Failed to get peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_addr_to_get), esp_err_to_name(err));
		return peer; // return invalid peer
	}
}

bool EasyEspNow::peerExists(const uint8_t *peer_addr)
{
	return esp_now_is_peer_exist(peer_addr);
}

bool EasyEspNow::updateLastSeenPeer(const uint8_t *peer_addr)
{
	if (peerExists(peer_addr))
	{
		for (int i = 0; i < peer_list.peer_number; i++)
		{
			if (memcmp(peer_list.peer[i].mac, peer_addr, MAC_ADDR_LEN) == 0)
			{
				uint32_t last_seen = millis();
				peer_list.peer[i].time_peer_added = last_seen;
				INFO(TAG_PEERS, "Peer[#%d] with MAC: " EASYMACSTR " was updated to last seen: %d ms", i + 1, EASYMAC2STR(peer_addr), last_seen);
				return true;
			}
		}
	}
	WARNING(TAG_PEERS, "Not possible to update last seen for MAC: " EASYMACSTR ". Maybe it does not exists as a peer!", EASYMAC2STR(peer_addr));
	return false;
}

int EasyEspNow::countPeers(CountPeers count_type)
{
	esp_now_peer_num_t num;

	err = esp_now_get_peer_num(&num);
	if (err == ESP_OK)
	{
		switch (count_type)
		{
		case TOTAL_NUM:
			return num.total_num;
		case ENCRYPTED_NUM:
			return num.encrypt_num;
		case UNENCRYPTED_NUM:
			return num.total_num - num.encrypt_num;
		default:
			ERROR(TAG_PEERS, "Invalid option chosen for peer count type!");
			return -1;
		}
	}
	else
	{
		ERROR(TAG_PEERS, "Failed to get peer count with error: %s", esp_err_to_name(err));
		return -1;
	}
}

void EasyEspNow::printPeerList()
{
	Serial.printf("\n\nPrinting Peer List! Number of peers %d\n", peer_list.peer_number);
	for (int i = 0; i < peer_list.peer_number; i++)
	{
		Serial.printf("Peer [" EASYMACSTR "] with timestamp %lu is %d ms old\n", MAC2STR(peer_list.peer[i].mac), peer_list.peer[i].time_peer_added, millis() - peer_list.peer[i].time_peer_added);
	}
	Serial.printf("\n\n");
}

/* ==========> Miscellaneous Functions <========== */

const char *EasyEspNow::easySendErrorToName(easy_send_error_t send_error)
{
	switch (send_error)
	{
	case EASY_SEND_OK:
		return "EASY_SEND_OK";
	case EASY_SEND_PARAM_ERROR:
		return "EASY_SEND_PARAM_ERROR";
	case EASY_SEND_PAYLOAD_LENGTH_ERROR:
		return "EASY_SEND_PAYLOAD_LENGTH_ERROR";
	case EASY_SEND_QUEUE_FULL_ERROR:
		return "EASY_SEND_QUEUE_FULL_ERROR";
	case EASY_SEND_MSG_ENQUEUE_ERROR:
		return "EASY_SEND_MSG_ENQUEUE_ERROR";
	case EASY_SEND_CONFIRM_ERROR:
		return "EASY_SEND_CONFIRM_ERROR";
	default:
		return "UNKNOWN_ERROR";
	}
}

wifi_interface_t EasyEspNow::autoselect_if_from_mode(wifi_mode_t mode, bool apstaMOD_to_apIF)
{
	if (mode == WIFI_MODE_NULL || mode == WIFI_MODE_MAX)
	{
		WARNING(TAG_MISC, "Useless WiFi modes, defaulting WiFi interface to: WIFI_IF_STA");
		return WIFI_IF_STA;
	}
	else if (mode == WIFI_MODE_STA)
	{
		INFO(TAG_MISC, "WiFi mode chosen: WIFI_MODE_STA  ==>  Auto Selecting WiFi interface: WIFI_IF_STA");
		return WIFI_IF_STA;
	}
	else if (mode == WIFI_MODE_AP)
	{
		INFO(TAG_MISC, "WiFi mode chosen: WIFI_MODE_AP  ==>  Auto Selecting WiFi interface: WIFI_IF_AP");
		return WIFI_IF_AP;
	}
	else if (mode == WIFI_MODE_APSTA)
	{
		if (apstaMOD_to_apIF == true)
		{
			INFO(TAG_MISC, "WiFi mode chosen: WIFI_MODE_APSTA & apstaMOD_to_apIF = true  ==>  Auto Selecting WiFi interface: WIFI_IF_AP");
			return WIFI_IF_AP;
		}
		else if (apstaMOD_to_apIF == false)
		{
			INFO(TAG_MISC, "WiFi mode chosen: WIFI_MODE_APSTA & apstaMOD_to_apIF = false  ==>  Auto Selecting WiFi interface: WIFI_IF_STA");
			return WIFI_IF_STA;
		}
	}
}

char *EasyEspNow::easyMac2Char(const uint8_t *some_mac, size_t len, bool upper_case)
{
	const uint8_t default_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	const char *format;
	if (upper_case)
		format = "%02X:%02X:%02X:%02X:%02X:%02X";
	else
		format = "%02x:%02x:%02x:%02x:%02x:%02x";

	if (!some_mac || len != 6)
	{
		some_mac = default_mac;
		// WARNING(TAG_MISC, "MAC argument is either null or has a length different from 6.  Defaulting to MAC: [00:00:00:00:00:00]");
	}

	static char mac_2_char[18] = {0};
	sprintf(mac_2_char, format, some_mac[0], some_mac[1], some_mac[2], some_mac[3], some_mac[4], some_mac[5]);
	return mac_2_char;
}

void EasyEspNow::easyPrintMac2Char(const uint8_t *some_mac, size_t len, bool upper_case)
{
	const uint8_t default_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	const char *format;
	if (upper_case)
		format = "%02X:%02X:%02X:%02X:%02X:%02X";
	else
		format = "%02x:%02x:%02x:%02x:%02x:%02x";

	if (!some_mac || len != 6)
	{
		some_mac = default_mac;
		// WARNING(TAG_MISC, "MAC argument is either null or has a length different from 6.  Defaulting to MAC: [00:00:00:00:00:00]");
		return;
	}

	Serial.printf(format, some_mac[0], some_mac[1], some_mac[2], some_mac[3], some_mac[4], some_mac[5]);
}

int32_t EasyEspNow::getEspNowVersion()
{
	uint32_t esp_now_version;
	err = esp_now_get_version(&esp_now_version);
	if (err != ESP_OK)
	{
		ERROR(TAG_MISC, "Failed to get ESP-NOW version with error: %s", esp_err_to_name(err));
		return -1;
	}
	else
	{
		MONITOR(TAG_MISC, "Success getting ESP-NOW version");
		return (int32_t)esp_now_version;
	}
}

uint8_t *EasyEspNow::getDeviceMACAddress()
{
	if (memcmp(my_mac_address, zero_mac, MAC_ADDR_LEN) == 0)
	{
		WARNING(TAG_MISC, "This device's MAC is not avaiable");
		return nullptr;
	}

	return this->my_mac_address;
}

uint8_t *EasyEspNow::generateRandomMAC(bool local, bool unicast)
{
	static uint8_t mac[6]; // Static array to hold the MAC address

	for (int i = 0; i < 6; i++)
	{
		mac[i] = random(0, 256); // Generate random byte for each part of the MAC
	}

	// Set or clear the local bit (bit 1 of the first byte) based on the local flag
	if (local)
		mac[0] |= 0x02; // Set to indicate locally administered
	else
		mac[0] &= 0xFD; // Clear to indicate globally unique

	// Set or clear the unicast/multicast bit (bit 0 of the first byte) based on the unicast flag
	if (unicast)
		mac[0] &= 0xFE; // Clear to indicate unicast
	else
		mac[0] |= 0x01; // Set to indicate multicast

	return mac; // Return the pointer to the MAC address array
}

bool EasyEspNow::switchChannel(uint8_t primary, wifi_second_chan_t second)
{
	if (primary < 1 || primary > 14)
	{
		WARNING(TAG_MISC, "Can't switch channel. Invalid value[%d] provided. Must be within the range [1..14]", primary);
		return false;
	}

	// This will set the WiFi channel for the home (this station)
	if (setChannel(primary, second))
	{
		bool no_errors = true;
		// iterate through all the peers and change the channel for each of them
		for (uint8_t i = 0; i < peer_list.peer_number; i++)
		{
			uint32_t last_seen = peer_list.peer[i].time_peer_added;
			uint8_t mac[MAC_ADDR_LEN] = {0};
			memcpy(mac, peer_list.peer[i].mac, MAC_ADDR_LEN);
			// Retrieve and modify peer info structures
			esp_now_peer_info_t fetchedPeer;

			esp_err_t get_peer_err, mod_peer_err;

			get_peer_err = esp_now_get_peer(mac, &fetchedPeer);
			if (get_peer_err == ESP_OK)
			{
				fetchedPeer.channel = primary; // New channel for peer
				mod_peer_err = esp_now_mod_peer(&fetchedPeer);
				if (mod_peer_err != ESP_OK)
				{
					MONITOR(TAG_MISC, "Failed to modify channel[%d] for peer[#%d] with MAC: " EASYMACSTR ". Error: %s", primary, i + 1, EASYMAC2STR(mac), esp_err_to_name(mod_peer_err));
					no_errors = false;
				}
				else
				{
					MONITOR(TAG_MISC, "Successfully modified to channel[%d] for peer[#%d] with MAC: " EASYMACSTR, primary, i + 1, EASYMAC2STR(mac));
				}
			}
			else
			{
				MONITOR(TAG_MISC, "Failed to retrieve info for peer[#%d] with MAC: " EASYMACSTR ". Error: %s", i + 1, EASYMAC2STR(mac), esp_err_to_name(get_peer_err));
				no_errors = false;
			}
		}
		return no_errors;
	}
	else
		return false;
}

/* ==========> Helper Functions for the Core Functions <========== */

bool EasyEspNow::initComms()
{
	// Init ESP-NOW here
	err = esp_now_init();
	if (err == ESP_OK)
	{
		MONITOR(TAG_HELPER, "Success initializing ESP-NOW");
	}
	else
	{
		MONITOR(TAG_HELPER, "Failed to initialize ESP-NOW");
		ERROR(TAG_HELPER, "Failed to initialize ESP-NOW with error: %s", esp_err_to_name(err));
		return false;
	}

	// Register low-level rx cb
	err = esp_now_register_recv_cb(rx_cb);
	if (err == ESP_OK)
	{
		MONITOR(TAG_HELPER, "Success registering low level ESP-NOW RX callback");
	}
	else
	{
		MONITOR(TAG_HELPER, "Failed registering low level ESP-NOW RX callback");
		ERROR(TAG_HELPER, "Failed registering low level ESP-NOW RX callback with error: %s", esp_err_to_name(err));
		return false;
	}

	// Register low-level tx cb
	err = esp_now_register_send_cb(tx_cb);
	if (err == ESP_OK)
	{
		MONITOR(TAG_HELPER, "Success registering low level ESP-NOW TX callback");
	}
	else
	{
		MONITOR(TAG_HELPER, "Failed registering low level ESP-NOW TX callback");
		ERROR(TAG_HELPER, "Failed registering low level ESP-NOW TX callback with error: %s", esp_err_to_name(err));
		return false;
	}

	if (this->synchronous_send == true)
		tx_queue_size = 1; // may be redundant but set TX Queue size to 1 when synchronous send mode

	// tx_queue = xQueueCreate(tx_queue_size, sizeof(int));
	// xTaskCreateUniversal(processTxQueueTask, "espnow_loop", 8 * 1024, NULL, 1, &txTask_handle, CONFIG_ARDUINO_RUNNING_CORE);

	txQueue = xQueueCreate(tx_queue_size, sizeof(tx_queue_item_t));
	// Check if the queue was created successfully
	if (txQueue == NULL)
	{
		ERROR(TAG_HELPER, "Failed to create TX Queue");
		// Handle the error, possibly halt or retry queue creation
		return false;
	}
	else
	{
		MONITOR(TAG_HELPER, "Successfully created TX Queue");
	}

	BaseType_t task_creation_result = xTaskCreateUniversal(easyEspNowTxQueueTask, "send_esp_now", 8 * 1024, NULL, 1, &txTaskHandle, CONFIG_ARDUINO_RUNNING_CORE);
	if (task_creation_result != pdPASS)
	{
		// Task creation failed
		ERROR(TAG_HELPER, "TX Task creation failed! Error: %ld", task_creation_result);
		return false;
	}
	else
	{
		// Task creation succeeded
		MONITOR(TAG_HELPER, "TX Task creation successful");
	}

	MONITOR(TAG_HELPER, "TX Synchronous Send mode is set to: [ %s ]. TX Queue Size is set to: [ %d ]", this->synchronous_send ? "TRUE" : "FALSE", this->tx_queue_size);

	return true;
}

bool EasyEspNow::setChannel(uint8_t primary_channel, wifi_second_chan_t second)
{
	esp_err_t ret = esp_wifi_set_channel(primary_channel, second);
	if (ret == ESP_OK)
	{
		MONITOR(TAG_HELPER, "WiFi channel was successfully set to: %d", primary_channel);
		wifi_primary_channel = primary_channel;
		wifi_secondary_channel = second;
		return true;
	}
	else
	{
		MONITOR(TAG_HELPER, "Failed to set WiFi channel");
		VERBOSE(TAG_HELPER, "Failed to set WiFi channel with error: %s\n", esp_err_to_name(ret));
		return false;
	}
}

void EasyEspNow::rx_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
	DEBUG(TAG_HELPER, "Calling ESP-NOW low level RX cb");

	/** Why This Works:
	 * In promiscuous mode, the received ESP-NOW data is part of a larger 802.11 packet (Management -> Action Frame ).
	 * When the data pointer is passed to the callback, it only points to the payload portion of the packet.
	 * By manipulating the pointer (shifting it back), you're able to access the surrounding metadata,
	 * such as the frame headers and control information that are part of the full 802.11 packet.
	 */

	espnow_frame_format_t *esp_now_packet = (espnow_frame_format_t *)(data - sizeof(espnow_frame_format_t));
	wifi_promiscuous_pkt_t *promiscuous_pkt = (wifi_promiscuous_pkt_t *)(data - sizeof(wifi_pkt_rx_ctrl_t) - sizeof(espnow_frame_format_t));
	wifi_pkt_rx_ctrl_t *rx_ctrl = &promiscuous_pkt->rx_ctrl;

	espnow_frame_recv_info_t frame_promisc_info = {.radio_header = rx_ctrl, .esp_now_frame = esp_now_packet};

	if (easyEspNow.dataReceived != nullptr)
	{
		easyEspNow.dataReceived(mac_addr, data, data_len, &frame_promisc_info);
	}
}

void EasyEspNow::tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	DEBUG(TAG_HELPER, "Calling ESP-NOW low level TX cb");

	if (easyEspNow.dataSent != nullptr)
	{
		easyEspNow.dataSent(mac_addr, status);
	}
}

void EasyEspNow::easyEspNowTxQueueTask(void *pvParameters)
{
	tx_queue_item_t item_to_dequeue;
	while (true)
	{
		// Wait for data from the queue
		if (xQueueReceive(easyEspNow.txQueue, &item_to_dequeue, pdMS_TO_TICKS(10)) == pdTRUE)
		{
			if (memcmp(item_to_dequeue.dst_address, easyEspNow.zero_mac, MAC_ADDR_LEN) == 0)
			{
				WARNING(TAG_HELPER, "Destination address is NULL, sending data to all unicast peers that are added to the peer list");
				easyEspNow.err = esp_now_send(NULL, item_to_dequeue.payload_data, item_to_dequeue.payload_len);
			}
			else
				easyEspNow.err = esp_now_send(item_to_dequeue.dst_address, item_to_dequeue.payload_data, item_to_dequeue.payload_len);

			if (easyEspNow.err == ESP_OK)
			{
				DEBUG(TAG_HELPER, "Succeeded in calling \"esp_now_send(...)\"");
			}
			else
			{
				ERROR(TAG_HELPER, "Failed in calling \"esp_now_send(...)\" with error: %s", esp_err_to_name(easyEspNow.err));
			}

			// add some delay to not overwhelm 'esp_now_send' method
			// otherwise may get error: 'ESP_ERR_ESPNOW_NO_MEM'
			// during debug set this higher than 13 to simulate delay
			// TX exhaust rate
			vTaskDelay(pdMS_TO_TICKS(13));
		}
	}
}

#endif // ESP32