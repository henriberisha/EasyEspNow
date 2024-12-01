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
int CURRENT_LOG_LEVEL = LOG_VERBOSE;     // need to set the log level, otherwise will have issues
constexpr auto MAIN_TAG = "MAIN_SKETCH"; // need to set a tag

uint8_t pmk[KEY_LENGTH] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B, 0x9C, 0xAD, 0xBC, 0xCF, 0xDA, 0xEB, 0xFC, 0x0D};
uint8_t lmk[KEY_LENGTH] = {0x1B, 0x2B, 0x3C, 0x5C, 0x5E, 0x6F, 0x7A, 0x8B, 0x9C, 0xAD, 0xBC, 0xCF, 0xDA, 0xEB, 0xFC, 0x1E};

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

    uint8_t frame_type;
    uint8_t frame_subtype;
    uint8_t source_address[MAC_ADDR_LEN];
    uint8_t destination_address[MAC_ADDR_LEN];
    uint8_t broadcast_address[MAC_ADDR_LEN];
    uint body_length;
    uint8_t ccmp_params[8];
    // in a similar fashion you can get the other data from the frame, just declare them here, and set them according to the incoming frame

    if (frame->unencrypted_frame != nullptr)
    {
        // retrieve info from the unencrypted frame
        frame_type = frame->unencrypted_frame->esp_now_frame_header.type;
        frame_subtype = frame->unencrypted_frame->esp_now_frame_header.subtype;
        memcpy(source_address, frame->unencrypted_frame->esp_now_frame_header.source_address, MAC_ADDR_LEN);
        memcpy(destination_address, frame->unencrypted_frame->esp_now_frame_header.destination_address, MAC_ADDR_LEN);
        memcpy(broadcast_address, frame->unencrypted_frame->esp_now_frame_header.broadcast_address, MAC_ADDR_LEN);

        // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format
        // Length: The Length is the total length of Organization Identifier, Type, Version and Body.
        // hence length of Body = total Length - length of Organization Identifier - length of Type - length of Version
        body_length = frame->unencrypted_frame->vendor_specific_content.length -
                      sizeof(frame->unencrypted_frame->vendor_specific_content.organization_identifier) -
                      sizeof(frame->unencrypted_frame->vendor_specific_content.type) -
                      sizeof(frame->unencrypted_frame->vendor_specific_content.version);

        // similar fashion get more info, also you can get info that is vendor specific and can be found in the unencrypted frame
    }

    else if (frame->ccmp_encrypted_frame != nullptr)
    {
        // retrieve info from the unencrypted frame
        frame_type = frame->ccmp_encrypted_frame->esp_now_frame_header.type;
        frame_subtype = frame->ccmp_encrypted_frame->esp_now_frame_header.subtype;
        memcpy(source_address, frame->ccmp_encrypted_frame->esp_now_frame_header.source_address, MAC_ADDR_LEN);
        memcpy(destination_address, frame->ccmp_encrypted_frame->esp_now_frame_header.destination_address, MAC_ADDR_LEN);
        memcpy(broadcast_address, frame->ccmp_encrypted_frame->esp_now_frame_header.broadcast_address, MAC_ADDR_LEN);

        // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format
        // Length: The Length is the total length of Organization Identifier, Type, Version and Body.
        // hence length of Body = total Length - length of Organization Identifier - length of Type - length of Version
        body_length = frame->ccmp_encrypted_frame->vendor_specific_content.length -
                      sizeof(frame->ccmp_encrypted_frame->vendor_specific_content.organization_identifier) -
                      sizeof(frame->ccmp_encrypted_frame->vendor_specific_content.type) -
                      sizeof(frame->ccmp_encrypted_frame->vendor_specific_content.version);

        memcpy(ccmp_params, frame->ccmp_encrypted_frame->ccmp_parameters, 8);
        Serial.printf("CCMP Parameters: ");
        for (int i = 0; i < sizeof(ccmp_params); i++)
        {
            Serial.printf("%02X ", ccmp_params[i]);
        }

        // similar fashion get more info, also you can get ccmp parameters found in encrypted frame
    }

    char sender_mac_from_frame_char[18] = {0};
    sprintf(sender_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            source_address[0], source_address[1], source_address[2],
            source_address[3], source_address[4], source_address[5]);

    char destination_mac_from_frame_char[18] = {0};
    sprintf(destination_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            destination_address[0], destination_address[1], destination_address[2],
            destination_address[3], destination_address[4], destination_address[5]);

    char broadcast_mac_from_frame_char[18] = {0};
    sprintf(broadcast_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            broadcast_address[0], broadcast_address[1], broadcast_address[2],
            broadcast_address[3], broadcast_address[4], broadcast_address[5]);

    unsigned int channel = frame->radio_header->channel;
    signed int rssi = frame->radio_header->rssi;

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

    WiFi.mode(wifi_mode);
    WiFi.disconnect(false, true); // use this if you do not need to be on any WiFi network

    wifi_interface_t wifi_interface = easyEspNow.autoselect_if_from_mode(wifi_mode);

    /* begin in synch send */
    bool begin_esp_now = easyEspNow.begin(channel, wifi_interface, 1, true);
    /* begin in asynch send */
    // bool begin_esp_now = easyEspNow.begin(channel, wifi_interface, 7, false);
    if (begin_esp_now)
        MONITOR(MAIN_TAG, "Success to begin EasyEspNow!!");
    else
        MONITOR(MAIN_TAG, "Fail to begin EasyEspNow!!");

    // Optional if you want CCMP encryption
    bool set_PMK = easyEspNow.setPMK(pmk);
    uint8_t my_set_pmk[KEY_LENGTH];
    bool get_PMK = easyEspNow.getPMK(my_set_pmk);
    if (get_PMK)
    {
        Serial.printf("PMK was set to: ");
        for (int i = 0; i < sizeof(my_set_pmk); i++)
        {
            Serial.printf("%02X ", my_set_pmk[i]);
        }
    }

    // Register your custom callbacks
    easyEspNow.onDataReceived(onFrameReceived_cb);
    easyEspNow.onDataSent(OnFrameSent_cb);

    // Must add Broadcast MAC as a peer in order to send Broadcast
    easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS);

    // Here you add a unicast peer device, no need to worry about the peer info
    easyEspNow.addPeer(some_peer_device);            // unencrypted
    easyEspNow.addPeer(some_other_peer_device, lmk); // encrypted
}

void loop()
{
    String data = message + " " + String(count++);
    // Here is doing only Broadcasting
    easy_send_error_t error = easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)data.c_str(), data.length());
    MONITOR(MAIN_TAG, "Last send return error value: %s\n", easyEspNow.easySendErrorToName(error));

    // add some delay to simulate longer ce run
    vTaskDelay(pdMS_TO_TICKS(2000));
    // delay(2000);
}