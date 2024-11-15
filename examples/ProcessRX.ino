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

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>

uint8_t channel = 7;
int CURRENT_LOG_LEVEL = LOG_VERBOSE;     // need to set the log level, otherwise will have issues
constexpr auto MAIN_TAG = "MAIN_SKETCH"; // need to set a tag

// this could be the MAC of one of your devices, replace with the correct one
uint8_t some_peer_device[] = {0xCD, 0x56, 0x47, 0xFC, 0xAF, 0xB3};

// This is a very basic message to send. In your case you may need to send more complex data such as structs
String message = "Hello, world! From EasyEspNow";
int count = 1;

TaskHandle_t rxTaskHandle;
QueueHandle_t rxQueue;
int rx_queue_size = 5;

// WARNING!! Leave "payload" as the last member of this struct, otherwise will have problems with values not being set for members that are after "payload"
// No idea why that is happening
typedef struct
{
    uint8_t frame_type;
    uint8_t frame_subtype;
    unsigned int channel;
    signed int rssi;                  /**< RSSI */
    int payload_len;                  /**< Payload length */
    uint8_t srcAddress[MAC_ADDR_LEN]; /**< Source Address */
    uint8_t dstAddress[MAC_ADDR_LEN]; /**< Destination Address */
    uint8_t payload[MAX_DATA_LENGTH]; /**< Message payload */
} rx_queue_item_t;

void processRXTask(void *pvParameters)
{
    rx_queue_item_t rx_item;
    while (true)
    {
        // Wait for data from the queue
        if (xQueueReceive(rxQueue, &rx_item, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            Serial.printf("Comms Received: SENDER_MAC: " EASYMACSTR ", DEST_MAC: " EASYMACSTR "\n"
                          "RSSI: %d, CHANNEL: %d, TYPE: %d, SUBTYPE: %d\n",
                          EASYMAC2STR(rx_item.srcAddress), EASYMAC2STR(rx_item.dstAddress),
                          rx_item.rssi, rx_item.channel,
                          rx_item.frame_type, rx_item.frame_subtype);

            Serial.printf("Data body length: %d bytes.\n", rx_item.payload_len);
            // This will work fine if message is unencrypted and has ASCII values
            // For a more generalized output, print each byte as HEX using a for loop and iterating over payload
            Serial.printf("Data Message: %.*s\n\n", rx_item.payload_len, rx_item.payload);

            taskYIELD();
        }
        taskYIELD();
    }
}

void onFrameReceived_cb(const uint8_t *senderAddr, const uint8_t *data, int len, espnow_frame_recv_info_t *frame)
{
    uint8_t frame_type = frame->esp_now_frame->type;
    uint8_t frame_subtype = frame->esp_now_frame->subtype;
    unsigned int channel = frame->radio_header->channel;
    signed int rssi = frame->radio_header->rssi;

    /* Here you should further process your RX messages as needed */

    int enqueued_rx_messages = uxQueueMessagesWaiting(rxQueue);
    MONITOR("RX TASK", "RX Queue Status (Enqueued | Capacity) -> %d | %d\n", enqueued_rx_messages, rx_queue_size);

    if (enqueued_rx_messages < rx_queue_size)
    {
        rx_queue_item_t rx_message;
        rx_message.frame_type = frame_type;
        rx_message.frame_subtype = frame_subtype;
        rx_message.payload_len = len;
        rx_message.rssi = rssi;
        rx_message.channel = channel;
        memcpy(rx_message.srcAddress, senderAddr, ESP_NOW_ETH_ALEN);
        memcpy(rx_message.dstAddress, frame->esp_now_frame->destination_address, ESP_NOW_ETH_ALEN);
        memcpy(rx_message.payload, data, len);

        // portMAX_DELAY -> will wait indefinitely
        // pdMS_TO_TICKS -> will have a timeout
        if (xQueueSend(rxQueue, &rx_message, pdMS_TO_TICKS(10)) == pdTRUE)
            MONITOR(MAIN_TAG, "Success to enqueue RX message");
        else
            WARNING(MAIN_TAG, "Failed to enqueue RX message, message dropped");
    }
    else
        WARNING(MAIN_TAG, "Queue FULL. Failed to enqueue RX message, message dropped");
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

    uint8_t *my_mac = easyEspNow.getDeviceMACAddress();
    easyEspNow.easyPrintMac2Char(my_mac, MAC_ADDR_LEN);
    Serial.println();
    easyEspNow.easyPrintMac2Char(my_mac, MAC_ADDR_LEN, false);

    // Start RX queue and tasks before registering the callbacks
    rxQueue = xQueueCreate(rx_queue_size, sizeof(tx_queue_item_t));
    // Check if the queue was created successfully
    if (rxQueue == NULL)
    {
        ERROR(MAIN_TAG, "Failed to create RX Queue");
        // Handle the error, possibly halt or retry queue creation
    }
    else
    {
        MONITOR(MAIN_TAG, "Successfully created RX Queue");
    }

    BaseType_t task_creation_result = xTaskCreateUniversal(processRXTask, "user_RX_processing", 8 * 1024, NULL, 2, &rxTaskHandle, CONFIG_ARDUINO_RUNNING_CORE);
    if (task_creation_result != pdPASS)
    {
        // Task creation failed, handle somehow
        ERROR(MAIN_TAG, "RX Task creation failed! Error: %ld", task_creation_result);
    }
    else
    {
        // Task creation succeeded
        MONITOR(MAIN_TAG, "RX Task creation successful");
    }

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
    // your code here
    String data = message + " " + String(count++);
    // Here is doing only Broadcasting
    easy_send_error_t error = easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)data.c_str(), data.length());
    MONITOR(MAIN_TAG, "Last send return error value: %s\n", easyEspNow.easySendErrorToName(error));

    vTaskDelay(pdMS_TO_TICKS(2000));
}