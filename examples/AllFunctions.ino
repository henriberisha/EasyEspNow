#include <Arduino.h> // depending on your situation this may need to be included
#if defined ESP32
#include <WiFi.h>     // no need because EasyEspNow includes it
#include <esp_wifi.h> // no need because EasyEspNow includes it
/* Need to choose WiFi mode in ESP32 */
wifi_mode_t wifi_mode = WIFI_MODE_STA;
// wifi_mode_t wifi_mode = WIFI_MODE_AP;
// wifi_mode_t wifi_mode = WIFI_MODE_APSTA;
#else
#error "Unsupported platform"
#endif // ESP32

#include <EasyEspNow.h>

uint8_t channel = 7;
int CURRENT_LOG_LEVEL = LOG_VERBOSE;     // need to set some log level, otherwise will have issues
constexpr auto MAIN_TAG = "MAIN_SKETCH"; // need to set a tag

bool demo_enable_disable_TX_task = true;
bool TX_task_disabled = false;

// this could be the MAC of one of your devices, replace with the correct one
uint8_t some_peer_device[] = {0xCD, 0x56, 0x47, 0xFC, 0xAF, 0xB3};
uint8_t some_other_peer_device[] = {0xCF, 0x56, 0x47, 0xFC, 0xAF, 0xB3};

// This is a very basic message to send. In your case you may need to send more complex data such as structs
String message = "Hello, world! From EasyEspNow";
int count = 1;

void onFrameReceived_cb(const uint8_t *senderAddr, const uint8_t *data, int len, espnow_frame_recv_info_t *frame)
{
    char sender_mac_char[18] = {0};
    sprintf(sender_mac_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            senderAddr[0], senderAddr[1], senderAddr[2], senderAddr[3], senderAddr[4], senderAddr[5]);

    uint8_t frame_type = frame->esp_now_frame->type;
    uint8_t frame_subtype = frame->esp_now_frame->subtype;

    char sender_mac_from_frame_char[18] = {0};
    sprintf(sender_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            frame->esp_now_frame->source_address[0], frame->esp_now_frame->source_address[1], frame->esp_now_frame->source_address[2],
            frame->esp_now_frame->source_address[3], frame->esp_now_frame->source_address[4], frame->esp_now_frame->source_address[5]);

    char destination_mac_from_frame_char[18] = {0};
    sprintf(destination_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            frame->esp_now_frame->destination_address[0], frame->esp_now_frame->destination_address[1], frame->esp_now_frame->destination_address[2],
            frame->esp_now_frame->destination_address[3], frame->esp_now_frame->destination_address[4], frame->esp_now_frame->destination_address[5]);

    char broadcast_mac_from_frame_char[18] = {0};
    sprintf(broadcast_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            frame->esp_now_frame->broadcast_address[0], frame->esp_now_frame->broadcast_address[1], frame->esp_now_frame->broadcast_address[2],
            frame->esp_now_frame->broadcast_address[3], frame->esp_now_frame->broadcast_address[4], frame->esp_now_frame->broadcast_address[5]);

    unsigned int channel = frame->radio_header->channel;
    signed int rssi = frame->radio_header->rssi;

    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format
    // Length: The Length is the total length of Organization Identifier, Type, Version and Body.
    // hence length of Body = total Length - length of Organization Identifier - length of Type - length of Version
    uint body_length = frame->esp_now_frame->vendor_specific_content.length -
                       sizeof(frame->esp_now_frame->vendor_specific_content.organization_identifier) -
                       sizeof(frame->esp_now_frame->vendor_specific_content.type) -
                       sizeof(frame->esp_now_frame->vendor_specific_content.version);

    Serial.printf("Comms Received: SENDER_MAC: %s, SENDER_MAC_FROM_FRAME: %s, DEST_MAC: %s, BROADCAST_MAC: %s\n"
                  "RSSI: %d, CHANNEL: %d, TYPE: %d, SUBTYPE: %d\n",
                  sender_mac_char, sender_mac_from_frame_char, destination_mac_from_frame_char, broadcast_mac_from_frame_char,
                  rssi, channel, frame_type, frame_subtype);

    Serial.printf("Data body length: %d | %d\n", body_length, len); // here print both to make sure they are the same and the formula works
    Serial.printf("Data Message: %.*s\n\n", len, data);

    /* Here you should further process your RX messages as needed */
}

void OnFrameSent_cb(const uint8_t *mac_addr, uint8_t status)
{
    // Delivery success does not neccessarily that the other end received the message. Just means that this sender was able to transmit the message.
    // In order to have a proper delivery assurance, type of ACK system needs to be build.
    char mac_char[18] = {0};
    sprintf(mac_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    Serial.printf("Last Packet Send Status: %s to destination %s\n\n", status == ESP_NOW_SEND_SUCCESS ? "Delivery SUCCESS" : "Delivery FAIL", mac_char);
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("\n\nHello World!"); // serial prints will not be affected from the log level you set
    MONITOR(MAIN_TAG, "Hello World from EasyEspNow");

    WiFi.mode(wifi_mode);
    WiFi.disconnect(false, true); // use this if you do not need to be on any WiFi network

    MONITOR(MAIN_TAG, "Starting WiFi channel: %d.", WiFi.channel());
    MONITOR(MAIN_TAG, "ESP-NOW Version: %d.", easyEspNow.getEspNowVersion());
    MONITOR(MAIN_TAG, "What is the length of a MAC address (How many octets?): %d.", easyEspNow.getAddressLength());
    MONITOR(MAIN_TAG, "What are the minimum and maximum lengths of data that ESP-NOW can send? MIN = %d byte and MAX = %d bytes", 1, easyEspNow.getMaxMessageLength());

    wifi_interface_t wifi_interface = easyEspNow.autoselect_if_from_mode(wifi_mode);

    /* begin in synch send */
    bool begin_esp_now = easyEspNow.begin(channel, wifi_interface, 10, false);
    /* begin in asynch send */
    // bool begin_esp_now = easyEspNow.begin(channel, wifi_interface, 7, false);
    if (begin_esp_now)
        MONITOR(MAIN_TAG, "Success to begin EasyEspNow!! Primary Channel: %d. Secondary Channel: %d", easyEspNow.getPrimaryChannel(), easyEspNow.getSecondaryChannel()); // secondary will display `wifi_second_chan_t` enum value
    else
        MONITOR(MAIN_TAG, "Fail to begin EasyEspNow!!");

    // this station's MAC
    uint8_t *my_mac = easyEspNow.getDeviceMACAddress();
    if (my_mac)
    {
        /* Different ways to print a MAC address here */
        easyEspNow.easyPrintMac2Char(my_mac, MAC_ADDR_LEN); // just print upper case MAC
        Serial.println();
        easyEspNow.easyPrintMac2Char(my_mac, MAC_ADDR_LEN, false); // just print lower case MAC
        Serial.println();
        Serial.printf("Upper case Mac is %s\n", easyEspNow.easyMac2Char(my_mac, MAC_ADDR_LEN));          // just convert to char upper case MAC
        Serial.printf("Lower case Mac is %s\n", easyEspNow.easyMac2Char(my_mac, MAC_ADDR_LEN, false));   // just convert to char lower case MAC
        MONITOR(MAIN_TAG, "Upper case Mac is %s", easyEspNow.easyMac2Char(my_mac, MAC_ADDR_LEN));        // just convert to char upper case MAC
        MONITOR(MAIN_TAG, "Lower case Mac is %s", easyEspNow.easyMac2Char(my_mac, MAC_ADDR_LEN, false)); // just convert to char lower case MAC
        /* If trying to convert them both in one line, the result is all lower case, at this time i am not sure why that happens. Avoid doing the below statement */
        // MONITOR(MAIN_TAG, "Upper case Mac is %s and Lower case Mac is %s", easyEspNow.easyMac2Char(my_mac, MAC_ADDR_LEN), easyEspNow.easyMac2Char(my_mac, MAC_ADDR_LEN, false)); // both

        MONITOR(MAIN_TAG, "Using macro definitions, MAC: " EASYMACSTR " will be upper case as defined in the EASYMACSTR string format macro!", EASYMAC2STR(my_mac)); // using macro
    }

    else
        MONITOR(MAIN_TAG, "Failed getting my MAC address");

    // Must add Broadcast MAC as a peer in order to send Broadcast
    easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS);

    // Here you add a unicast peer device, no need to worry about the peer info
    easyEspNow.addPeer(some_peer_device);

    /* How to get a peer */
    peer_t peer;
    esp_now_peer_info_t peer_info;
    peer = easyEspNow.getPeer(some_peer_device, peer_info);
    if (peer.time_peer_added > 0)
    {
        MONITOR(MAIN_TAG, "Peer: [" EASYMACSTR "] with timestamp: %d ms\n", EASYMAC2STR(peer.mac), peer.time_peer_added);
        MONITOR(MAIN_TAG, "Detailed peer info -> MAC: " EASYMACSTR ", CHANNEL: %d, ENCRYPTION: %s\n\n",
                EASYMAC2STR(peer_info.peer_addr), peer_info.channel, peer_info.encrypt == 0 ? "Not Encrypted" : "Encrypted");
    }
    else
    {
        MONITOR(MAIN_TAG, "Can't get peer");
    }

    /* Print peer list */
    easyEspNow.printPeerList();

    /* Check if peer exists */
    if (easyEspNow.peerExists(ESPNOW_BROADCAST_ADDRESS))
        MONITOR(MAIN_TAG, "Peer exists");
    else
        MONITOR(MAIN_TAG, "Peer does not exist");

    if (easyEspNow.peerExists(some_other_peer_device))
        MONITOR(MAIN_TAG, "Peer exists");
    else
        MONITOR(MAIN_TAG, "Peer does not exist");

    delay(5000);

    /* Update last seen peer */
    if (easyEspNow.updateLastSeenPeer(some_peer_device))
        MONITOR(MAIN_TAG, "Last seen peer updated");
    else
        MONITOR(MAIN_TAG, "Peer can't have last seen updated");

    /* Print peer list again to check updates*/
    easyEspNow.printPeerList();

    /* Generate random MAC addresses and add them as peers */
    // Generated MACs will be local/unicast addresses
    // Peer list can hold a maximum of 20 peers
    for (int i = 0; i < 25; i++)
    {
        uint8_t *random_mac = easyEspNow.generateRandomMAC(true, true);
        MONITOR(MAIN_TAG, "Random generated MAC: %s", easyEspNow.easyMac2Char(random_mac));
        if (easyEspNow.addPeer(random_mac))
            MONITOR(MAIN_TAG, "Added peer: %s", easyEspNow.easyMac2Char(random_mac));
        else
            MONITOR(MAIN_TAG, "MAX. capacity! Can't add peer: %s", easyEspNow.easyMac2Char(random_mac));
    }

    /* Print peer list again to check updates*/
    easyEspNow.printPeerList();

    /* Delete peers */
    if (easyEspNow.deletePeer(some_peer_device))
        MONITOR(MAIN_TAG, "Success deleting peer");
    else
        MONITOR(MAIN_TAG, "Failed deleting peer");

    uint8_t *delete_oldest = easyEspNow.deletePeer();
    if (delete_oldest)
        MONITOR(MAIN_TAG, "Success deleting oldest peer");
    else
        MONITOR(MAIN_TAG, "Failed deleting oldest peer");

    /* How to get the reference list for all the peers */
    peer_list_t peer_list = easyEspNow.getPeerList();
    Serial.printf("Total peers: %d\n", peer_list.peer_number);

    for (uint8_t i = 0; i < peer_list.peer_number; i++)
    {
        uint32_t last_seen = peer_list.peer[i].time_peer_added;
        uint8_t mac[MAC_ADDR_LEN] = {0};
        memcpy(mac, peer_list.peer[i].mac, MAC_ADDR_LEN);
        Serial.printf("Peer %d -> MAC Address: %s. Time Peer Added: %u\n", i + 1, easyEspNow.easyMac2Char(mac), last_seen);
    }

    Serial.printf("Total peers: %d. Encrypted Peers: %d. Unencrypted Peers: %d\n\n",
                  easyEspNow.countPeers(TOTAL_NUM), easyEspNow.countPeers(ENCRYPTED_NUM), easyEspNow.countPeers(UNENCRYPTED_NUM));

    /* How to empty peer list */
    peer_list = easyEspNow.getPeerList();
    while (peer_list.peer_number > 0)
    {
        // this will delete Broadcast address as well
        // if you want to keep broadcast address, adjust the while loop `peer_list.peer_number > 1`
        // otherwise you will end into an infinite loop
        uint8_t *oldest_mac = easyEspNow.deletePeer(false);
        if (oldest_mac)
            MONITOR(MAIN_TAG, "Success deleting oldest peer: %s", easyEspNow.easyMac2Char(oldest_mac));
        else
            MONITOR(MAIN_TAG, "Failed deleting oldest peer");

        peer_list = easyEspNow.getPeerList(); // retrieve the peer list
    }

    /* Print peer list again to check updates*/
    easyEspNow.printPeerList();

    Serial.printf("Total peers: %d. Encrypted Peers: %d. Unencrypted Peers: %d\n\n",
                  easyEspNow.countPeers(TOTAL_NUM), easyEspNow.countPeers(ENCRYPTED_NUM), easyEspNow.countPeers(UNENCRYPTED_NUM));

    // add again your initial peers, at this point the list will be empty
    easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS);
    easyEspNow.addPeer(some_peer_device);

    // Register your custom callbacks
    easyEspNow.onDataReceived(onFrameReceived_cb);
    easyEspNow.onDataSent(OnFrameSent_cb);
}

void loop()
{
    String data = message + " " + String(count++);

    // condition is needed to avoid packet drop only when doing asynch
    if (!easyEspNow.readyToSendData())
        easyEspNow.waitForTXQueueToBeEmptied();
    easy_send_error_t code = easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)data.c_str(), data.length());
    // easy_send_error_t code = easyEspNow.sendBroadcast((uint8_t *)data.c_str(), data.length());
    MONITOR(MAIN_TAG, "Last send return code value: %s\n", easyEspNow.easySendErrorToName(code));

    // add some delay to simulate longer code run
    // for this demo i put rate 12us on purpose less than 13us which is the TX exhaust rate
    // in your code TX send should be slower than TX exhaust
    vTaskDelay(pdMS_TO_TICKS(12));
    // delay(2000);

    // simulating enable/disable TX task, careful how it is used.
    // if the TX task is suspended, and messages are being sent, it will cause message drop as soon as TX queue gets full.
    // if the TX task is suspended, `waitForTXQueueToBeEmptied` will unblock by exiting to avoid an infinite loop
    // i suggest to stop sending anything, suspend TX task to free resources as needed
    if (millis() > 20000 && millis() < 40000)
    {
        easyEspNow.enableTXTask(false);
    }
    else if (millis() > 60000 && millis() < 65000)
    {
        easyEspNow.enableTXTask(true);
        delay(1000);

        // for this demo purpose, switch channel after TX task is enabled to assure channel switch happens
        if (easyEspNow.switchChannel(9))
            Serial.println("Switched channel");
        else
            Serial.println("Did not switch channel");
        delay(6000);

        Serial.printf("WiFi channel after attempt to switch on the fly: %d\n", WiFi.channel());
        Serial.printf("WiFi channel: %d\n", easyEspNow.getPrimaryChannel());
    }

    if (millis() > 100000)
    {
        easyEspNow.stop();
        while (true)
        {
            ; // do nothing
        }
    }
}