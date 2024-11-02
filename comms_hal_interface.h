#ifndef COMMS_HAL_INTERFACE_H
#define COMMS_HAL_INTERFACE_H
#ifdef ESP32

#include <esp_wifi.h>

/*
typedef enum {
    WIFI_MODE_NULL = 0,  // null mode
    WIFI_MODE_STA,       // WiFi station mode
    WIFI_MODE_AP,        // WiFi soft-AP mode
    WIFI_MODE_APSTA,     // WiFi station + soft-AP mode
    WIFI_MODE_MAX
} wifi_mode_t;

typedef enum {
    WIFI_IF_STA = ESP_IF_WIFI_STA,
    WIFI_IF_AP  = ESP_IF_WIFI_AP,
} wifi_interface_t;
*/

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format
typedef struct
{
    unsigned protocol : 2;
    unsigned type : 2;
    unsigned subtype : 4;
    uint8_t flags;
    uint16_t duration;
    uint8_t destination_address[6];
    uint8_t source_address[6];
    uint8_t broadcast_address[6];
    uint16_t sequence_control;

    uint8_t category_code;
    uint8_t organization_identifier[3]; // 0x18fe34
    uint8_t random_values[4];
    struct
    {
        uint8_t element_id;                 // 0xdd ; set to the value (221)
        uint8_t length;                     //
        uint8_t organization_identifier[3]; // 0x18fe34
        uint8_t type;                       // 4
        uint8_t version;
        uint8_t body[0];
    } vendor_specific_content;
} __attribute__((packed)) espnow_frame_format_t;

typedef struct
{
    wifi_pkt_rx_ctrl_t *radio_header;
    espnow_frame_format_t *esp_now_frame;
} espnow_frame_recv_info_t;

typedef std::function<void(const uint8_t *src_mac, const uint8_t *data, int data_len, espnow_frame_recv_info_t *esp_now_frame)> frame_rcvd_data;
typedef std::function<void(const uint8_t *dst_addr, uint8_t status)> frame_sent_data;

typedef enum
{
    EASY_SEND_OK = 0,                    /**< Data was enqued for sending successfully */
    EASY_SEND_PARAM_ERROR = -1,          /**< Data was not sent due to parameter call error */
    EASY_SEND_PAYLOAD_LENGTH_ERROR = -2, /**< Data was not sent due to payload too long */
    EASY_SEND_QUEUE_FULL_ERROR = -3,     /**< Data was not sent due to queue full */
    EASY_SEND_MSG_ENQUEUE_ERROR = -4,    /**< Data was not sent due to message queue push error */
    EASY_SEND_CONFIRM_ERROR = -5,        /**< Data was not sent due to confirmation error (only for synchronous send) */
} easy_send_error_t;

class CommsHalInterface
{
protected:
    esp_err_t err;

    wifi_mode_t wifi_mode;
    wifi_interface_t wifi_phy_interface;
    uint8_t wifi_primary_channel; ///< @brief Comms channel to be used
    wifi_second_chan_t wifi_secondary_channel;

    frame_sent_data dataSent = nullptr;
    frame_rcvd_data dataReceived = nullptr;

    virtual bool initComms() = 0;

public:
    CommsHalInterface() {}

    virtual bool begin(uint8_t channel, wifi_interface_t phy_interface, int tx_q_size, bool synch_send) = 0;

    virtual void stop() = 0;

    virtual uint8_t getPrimaryChannel() = 0;

    virtual wifi_second_chan_t getSecondaryChannel() = 0;

    virtual easy_send_error_t send(const uint8_t *da, const uint8_t *data, size_t len) = 0;

    virtual void onDataReceived(frame_rcvd_data frame_rcvd_cb) = 0;

    virtual void onDataSent(frame_sent_data frame_sent_cb) = 0;

    virtual uint8_t getAddressLength() = 0;

    virtual uint8_t getMaxMessageLength() = 0;

    virtual void enableTXTask(bool enable) = 0;
};

#endif // ESP32
#endif