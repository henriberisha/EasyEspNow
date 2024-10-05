// #include "mesh_now.h"
#include "EasyEspNow.h"

int total_send = 0;

uint8_t channel = 9;
uint8_t TEST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFA};
uint8_t TEST_ADDRESS2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB};

// Function to generate a random MAC address
void generateRandomMAC(uint8_t *mac)
{
    for (int i = 0; i < 6; i++)
    {
        mac[i] = random(0, 256); // Generate random byte for each part of the MAC
    }
    mac[0] &= 0xFE; // Set the local bit to indicate it's a locally administered address
    mac[0] |= 0x02; // Ensure it's a unicast address
}

void onFrameReceived_cb(const uint8_t *senderAddr, const uint8_t *data, int len, espnow_frame_recv_info_t *frame)
{
    /*
    TODO, how to calculate the length of the body of data from the frame info, check:
    https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format
    */

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
}

void OnFrameSent_cb(const uint8_t *mac_addr, uint8_t status)
{
    // Delivery success does not neccessarily that the other end received the message. Just means that this sender was able to transmit the message.
    // In order to have a proper delivery assurance, type of ACK system needs to be build.
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    char mac_char[18] = {0};
    sprintf(mac_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    Serial.println(mac_char);
}

int CURRENT_LOG_LEVEL = LOG_VERBOSE; // need to set the log level, otherwise will have issues
constexpr auto MAIN_TAG = "MAIN";    // need to set a tag

esp_err_t ret;

void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("Hello World!");
    MONITOR(MAIN_TAG, "Hello World & Mesh");

    easyEspNow.printtest();
    // Serial.println(mesh.getEspNowVersion());

    WiFi.mode(WIFI_MODE_STA);

    Serial.printf("Starting WiFi channel: %d\n", WiFi.channel());

    Serial.println(easyEspNow.begin(channel, WIFI_IF_STA));

    Serial.println(easyEspNow.getEspNowVersion());

    easyEspNow.onDataReceived(onFrameReceived_cb);
    easyEspNow.onDataSent(OnFrameSent_cb);

    // // peer can be in a different interface from the home (this station) and still receive the message.
    // esp_now_peer_info_t peer_info;
    // memcpy(peer_info.peer_addr, ESPNOW_BROADCAST_ADDRESS, 6);
    // peer_info.ifidx = WIFI_IF_STA; // this does not really matter to set it the same as the peer. This is relevant to the home station WiFi mode and interface. ESP_ERR_ESPNOW_IF
    // peer_info.channel = channel;
    // peer_info.encrypt = false;

    // ret = esp_now_add_peer(&peer_info);
    // if (ret == ESP_OK)
    // {
    //     Serial.println("Success adding broadcast peer");
    // }
    // else
    // {
    //     Serial.println("Failed to add broadcast peer");
    //     return;
    // }

    Serial.println(easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS));

    uint8_t *res;
    res = easyEspNow.deletePeer(false);
    if (res)
    {
        Serial.printf("Deleted: \n" EASYMACSTR, EASYMAC2STR(res));
    }
    else
    {
        Serial.printf("No deletion");
    }
    return;

    int count = 0;
    Serial.println(easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS));
    count++;
    for (int i = 0; i < 25; i++)
    {
        uint8_t addr[6];
        generateRandomMAC(addr);
        if (easyEspNow.addPeer(addr))
            count++;

        Serial.printf("Iteration: %d. Success addition: %d. Mac: \n" EASYMACSTR, i, count, EASYMAC2STR(addr));

        memset(addr, 0, 6);
    }

    easyEspNow.printPeerList();

    res = easyEspNow.deletePeer();

    Serial.println(easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS));

    res = easyEspNow.deletePeer();

    Serial.println(easyEspNow.addPeer(TEST_ADDRESS));

    easyEspNow.printPeerList();

    res = easyEspNow.deletePeer();

    easyEspNow.printPeerList();

    Serial.println(easyEspNow.deletePeer(ESPNOW_BROADCAST_ADDRESS));
    easyEspNow.printPeerList();
    Serial.println(easyEspNow.deletePeer(TEST_ADDRESS));
    easyEspNow.printPeerList();
    Serial.println(easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS));
    easyEspNow.printPeerList();

    Serial.println(easyEspNow.addPeer(TEST_ADDRESS));

    Serial.printf("Total peers: %d. Encrypted Peers: %d. Unencrypted Peers: %d\n\n",
                  easyEspNow.countPeers(TOTAL_NUM), easyEspNow.countPeers(ENCRYPTED_NUM), easyEspNow.countPeers(UNENCRYPTED_NUM));

    peer_t *peer;
    esp_now_peer_info_t peer_info;
    peer = easyEspNow.getPeer(TEST_ADDRESS2, peer_info);
    if (peer)
    {
        Serial.printf("Peer: [" EASYMACSTR "] with timestamp: %d ms\n", EASYMAC2STR(peer->mac), peer->time_peer_added);
        Serial.printf("Detailed peer info -> MAC: " EASYMACSTR ", CHANNEL: %d, ENCRYPTION: %s\n\n",
                      EASYMAC2STR(peer_info.peer_addr), peer_info.channel, peer_info.encrypt == 0 ? "Not Encrypted" : "Encrypted");
    }
    else
    {
        Serial.printf("Can't get peer");
    }
}

void loop()
{
    // THIS IS FOR TEST, DELETE LATER
    // int num = random();
    // Serial.printf("Sent: %d. Total sent: %d\n", num, ++total_send);
    // easyEspNow.sendTest(num);
    // vTaskDelay(pdMS_TO_TICKS(80));
    // return;

    String message = "Hello, world! " + String(random());
    // Even to send a broadcast, the broadcast address needs to be added as a peer, o/w it will fail
    // ret = esp_now_send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)message.c_str(), message.length());
    // Serial.println(esp_err_to_name(ret));

    easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)message.c_str(), message.length());
    vTaskDelay(pdMS_TO_TICKS(80));
    // delay(1000);
    //  Your code here
}