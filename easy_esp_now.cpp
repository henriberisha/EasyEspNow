#include "easy_esp_now.h"

EasyEspNow easyEspNow;

constexpr auto TAG = "MESH_NOW_ESP";

bool EasyEspNow::begin(uint8_t channel, wifi_interface_t phy_interface)
{
    wifi_mode_t mode;
    err = esp_wifi_get_mode(&mode);
    if (err == ESP_OK)
    {
        if (mode == WIFI_MODE_NULL || mode == WIFI_MODE_MAX)
        {
            ERROR(TAG, "WiFi is not initialized. WIFI_MODE_NULL or WIFI_MODE_MAX");
            VERBOSE(TAG, "WiFi needs to be initialized to begin the mesh, with the correct mode. Try function: WiFi.mode(...) in the setup of your main sketch");
            return false;
        }
        else
        {
            MONITOR(TAG, "WiFi is initialized.");
        }
    }
    else
    {
        ERROR(TAG, "WiFi is not initialized. Error: %s\n", esp_err_to_name(err));
        return false;
    }

    if ((mode == WIFI_MODE_STA && phy_interface != WIFI_IF_STA) ||
        (mode == WIFI_MODE_AP && phy_interface != WIFI_IF_AP))
    {
        ERROR(TAG, "WiFi mode and WiFi physical interface mismatch");
        VERBOSE(TAG, "WiFi mode and WiFi physical interface must match: WIFI_MODE_STA <-> WIFI_IF_STA; WIFI_MODE_AP <-> WIFI_IF_AP");
        return false;
    }

    // Get current wifi channel the radio is on
    err = esp_wifi_get_channel(&wifi_primary_channel, &wifi_secondary_channel);
    if (err == ESP_OK)
    {
        MONITOR(TAG, "WiFi is on default channel: %d", wifi_primary_channel);
    }
    else
    {
        MONITOR(TAG, "Failed to get WiFi channel with error: %s\n", esp_err_to_name(err));
        return false;
    }

    if (channel < MIN_WIFI_CHANNEL || channel > MAX_WIFI_CHANNEL)
    {
        ERROR(TAG, "WiFi channel selection out of range. You selected: %d", channel);
        VERBOSE(TAG, "WiFi channel out of range, select channel [0...14]. Channel = 0 => device will keep the channel that the radio is on");
        return false;
    }
    else if (channel == 0)
    {
        MONITOR(TAG, "WiFi channel selection = %d. WiFi radio will remain on default channel: %d\n", channel, wifi_primary_channel);
    }
    else
    {
        if (this->setChannel(channel) == false)
        {
            ERROR(TAG, "Failed to set desired WiFi channel");
            return false;
        }
    }

    // initcomms here

    if (initComms() == false)
        return false;

    this->wifi_mode = mode;
    this->wifi_phy_interface = phy_interface;

    return true;
}

bool EasyEspNow::setChannel(uint8_t primary_channel, wifi_second_chan_t second)
{
    esp_err_t ret = esp_wifi_set_channel(primary_channel, second);
    if (ret == ESP_OK)
    {
        MONITOR(TAG, "WiFi channel was successfully set to: %d", primary_channel);
        wifi_primary_channel = primary_channel;
        return true;
    }
    else
    {
        MONITOR(TAG, "Failed to set WiFi channel");
        VERBOSE(TAG, "Failed to set WiFi channel with error: %s\n", esp_err_to_name(ret));
        return false;
    }
}

void EasyEspNow::stop()
{
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
}

int32_t EasyEspNow::getEspNowVersion()
{
    uint32_t esp_now_version;
    err = esp_now_get_version(&esp_now_version);
    if (err != ESP_OK)
    {
        ERROR(TAG, "Failed to get ESP-NOW version with error: %s", esp_err_to_name(err));
        return -1;
    }
    else
    {
        MONITOR(TAG, "Success getting ESP-NOW version");
        return (int32_t)esp_now_version;
    }
}

comms_send_error_t EasyEspNow::send(const uint8_t *dstAddress, const uint8_t *payload, size_t payload_len)
{
}

// void EasyEspNow::onDataRcvd(comms_hal_rcvd_data dataRcvd){}

// void EasyEspNow::onDataSent(comms_hal_sent_data sentResult){}

void EasyEspNow::enableTransmit(bool enable)
{
}

bool EasyEspNow::initComms()
{
    // Init ESP-NOW here
    err = esp_now_init();
    if (err == ESP_OK)
    {
        MONITOR(TAG, "Success initializing ESP-NOW");
    }
    else
    {
        MONITOR(TAG, "Failed to initialize ESP-NOW");
        ERROR(TAG, "Failed to initialize ESP-NOW with error: %s", esp_err_to_name(err));
        return false;
    }

    // Register low-level rx cb
    err = esp_now_register_recv_cb(rx_cb);
    if (err == ESP_OK)
    {
        MONITOR(TAG, "Success registering low level ESP-NOW RX callback");
    }
    else
    {
        MONITOR(TAG, "Failed registering low level ESP-NOW RX callback");
        ERROR(TAG, "Failed registering low level ESP-NOW RX callback with error: %s", esp_err_to_name(err));
        return false;
    }

    // Register low-level tx cb
    err = esp_now_register_send_cb(tx_cb);
    if (err == ESP_OK)
    {
        MONITOR(TAG, "Success registering low level ESP-NOW TX callback");
    }
    else
    {
        MONITOR(TAG, "Failed registering low level ESP-NOW TX callback");
        ERROR(TAG, "Failed registering low level ESP-NOW TX callback with error: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

// Register cb
void EasyEspNow::onDataReceived(frame_rcvd_data frame_rcvd_cb)
{
    DEBUG(TAG, "Registering custom onReceive Callback Function");
    dataReceived = frame_rcvd_cb;
}

void EasyEspNow::rx_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    DEBUG(TAG, "Calling ESP-NOW low level RX cb");

    espnow_frame_format_t *esp_now_packet = (espnow_frame_format_t *)(data - sizeof(espnow_frame_format_t));
    wifi_promiscuous_pkt_t *promiscuous_pkt = (wifi_promiscuous_pkt_t *)(data - sizeof(wifi_pkt_rx_ctrl_t) - sizeof(espnow_frame_format_t));
    wifi_pkt_rx_ctrl_t *rx_ctrl = &promiscuous_pkt->rx_ctrl;

    espnow_frame_recv_info_t frame_promisc_info = {.radio_header = rx_ctrl, .esp_now_frame = esp_now_packet};

    if (easyEspNow.dataReceived != nullptr)
    {
        easyEspNow.dataReceived(mac_addr, data, data_len, &frame_promisc_info);
    }
}

void EasyEspNow::onDataSent(frame_sent_data frame_sent_cb)
{
    DEBUG(TAG, "Registering custom onSent Callback function");
    dataSent = frame_sent_cb;
}

void EasyEspNow::tx_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    DEBUG(TAG, "Calling ESP-NOW low level TX cb");

    if (easyEspNow.dataSent != nullptr)
    {
        easyEspNow.dataSent(mac_addr, status);
    }
}

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

        MONITOR(TAG, "Successfully added peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_addr_to_add), peer_list.peer_number);
        return true;
    }
    else
    {
        ERROR(TAG, "Failed to add peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_addr_to_add), esp_err_to_name(err));
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

        MONITOR(TAG, "Successfully deleted peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_addr_to_delete), peer_list.peer_number);
        return true;
    }
    else
    {
        ERROR(TAG, "Failed to delete peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_addr_to_delete), esp_err_to_name(err));
        return false;
    }
}

// here probably add argument to keep broadcast address TODO
uint8_t *EasyEspNow::deletePeer()
{
    uint8_t oldest_index = 0;
    uint32_t oldest_peer_time = peer_list.peer[0].time_peer_added;

    for (int i = 0; i < peer_list.peer_number; i++)
    {
        // Time, is saved in millis, time increases, so older peers will have smaller time value as they were adder earlier
        if (peer_list.peer[i].time_peer_added < oldest_peer_time)
        {
            oldest_peer_time = peer_list.peer[i].time_peer_added;
            oldest_index = i;
        }
    }

    uint8_t *peer_mac_to_delete;
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
        MONITOR(TAG, "Successfully deleted peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_mac_to_delete), peer_list.peer_number);
        return peer_mac_to_delete;
    }
    else
    {
        ERROR(TAG, "Failed to delete peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_mac_to_delete), esp_err_to_name(err));
        return nullptr;
    }
}

peer_t *EasyEspNow::getPeer(const uint8_t *peer_addr_to_get, esp_now_peer_info_t &peer_info)
{
    peer_t *peer;

    err = esp_now_get_peer(peer_addr_to_get, &peer_info);
    if (err == ESP_OK)
    {
        for (int i = 0; i < peer_list.peer_number; i++)
        {
            if (memcmp(peer_list.peer[i].mac, peer_addr_to_get, MAC_ADDR_LEN) == 0)
            {
                MONITOR(TAG, "Success getting peer: [" EASYMACSTR "]. Total peers = %d", EASYMAC2STR(peer_addr_to_get), peer_list.peer_number);
                return peer_list.peer;
            }
        }
    }
    else
    {
        ERROR(TAG, "Failed to get peer: [" EASYMACSTR "] with error: %s\n", EASYMAC2STR(peer_addr_to_get), esp_err_to_name(err));
        return nullptr;
    }
}

bool EasyEspNow::peerExists(const uint8_t* peer_addr){
    return esp_now_is_peer_exist(peer_addr);
}

int EasyEspNow::countPeers(CountPeers count_type){
    esp_now_peer_num_t num;
    
    err = esp_now_get_peer_num(&num);
    if(err==ESP_OK){
        switch(count_type){
            case TOTAL_NUM:
                return num.total_num;
            case ENCRYPTED_NUM:
                return num.encrypt_num;
            case UNENCRYPTED_NUM:
                return num.total_num - num.encrypt_num;
            default:
                ERROR(TAG, "Invalid option chosen for peer count type!");
                return -1;
        }
    }
    else{
        ERROR(TAG, "Failed to get peer count with error: %s", esp_err_to_name(err));
        return -1;
    }
}

void EasyEspNow::printPeerList()
{
    Serial.printf("Number of peers %d\n", peer_list.peer_number);
    for (int i = 0; i < peer_list.peer_number; i++)
    {
        Serial.printf("Peer [" EASYMACSTR "] is %d ms old\n", MAC2STR(peer_list.peer[i].mac), millis() - peer_list.peer[i].time_peer_added);
    }
}