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

// this could be the MAC of one of your devices, replace with the correct one
uint8_t some_peer_device[] = {0xCD, 0x56, 0x47, 0xFC, 0xAF, 0xB3};

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

    char destination_mac_from_frame_char[18] = {0};
    sprintf(destination_mac_from_frame_char, "%02X:%02X:%02X:%02X:%02X:%02X",
            frame->esp_now_frame->destination_address[0], frame->esp_now_frame->destination_address[1], frame->esp_now_frame->destination_address[2],
            frame->esp_now_frame->destination_address[3], frame->esp_now_frame->destination_address[4], frame->esp_now_frame->destination_address[5]);

    unsigned int channel = frame->radio_header->channel;
    signed int rssi = frame->radio_header->rssi;

    Serial.printf("Comms Received: SENDER_MAC: %s, DEST_MAC: %s\n"
                  "RSSI: %d, CHANNEL: %d, TYPE: %d, SUBTYPE: %d\n",
                  sender_mac_char, destination_mac_from_frame_char,
                  rssi, channel, frame_type, frame_subtype);

    Serial.printf("Data body length: %d. ", len);
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

    // Register your custom callbacks
    easyEspNow.onDataReceived(onFrameReceived_cb);
    easyEspNow.onDataSent(OnFrameSent_cb);

    // Must add Broadcast MAC as a peer in order to send Broadcast
    easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS);

    // Here you add a unicast peer device, no need to worry about the peer info
    easyEspNow.addPeer(some_peer_device);
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