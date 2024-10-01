#ifndef COMMS_HAL_INTERFACE_H
#define COMMS_HAL_INTERFACE_H

#include <esp_wifi.h>

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
        uint8_t element_id;                 // 0xdd
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

// typedef std::function<void(uint8_t *address, uint8_t *data, uint8_t len, signed int rssi, bool broadcast)> comms_hal_rcvd_data;
// typedef std::function<void(uint8_t *address, uint8_t status)> comms_hal_sent_data;

typedef enum
{
    COMMS_SEND_OK = 0,                    /**< Data was enqued for sending successfully */
    COMMS_SEND_PARAM_ERROR = -1,          /**< Data was not sent due to parameter call error */
    COMMS_SEND_PAYLOAD_LENGTH_ERROR = -2, /**< Data was not sent due to payload too long */
    COMMS_SEND_QUEUE_FULL_ERROR = -3,     /**< Data was not sent due to queue full */
    COMMS_SEND_MSG_ENQUEUE_ERROR = -4,    /**< Data was not sent due to message queue push error */
    COMMS_SEND_CONFIRM_ERROR = -5,        /**< Data was not sent due to confirmation error (only for synchronous send) */
} comms_send_error_t;

class CommsHalInterface
{
protected:
    esp_err_t err;

    wifi_mode_t wifi_mode;
    wifi_interface_t wifi_phy_interface;
    uint8_t wifi_primary_channel; ///< @brief Comms channel to be used
    wifi_second_chan_t wifi_secondary_channel;

    int test_var;

    // comms_hal_rcvd_data dataRcvd = 0;   ///< @brief Pointer to a function to be called on every received message
    // comms_hal_sent_data sentResult = 0; ///< @brief Pointer to a function to be called to notify last sending status
    frame_sent_data dataSent = nullptr;
    frame_rcvd_data dataReceived = nullptr;

    virtual bool initComms() = 0;

public:
    CommsHalInterface() {}

    /**
     * @brief Setup communication environment and establish the connection from node to gateway
     * @param gateway Address of gateway. It may be `NULL` in case this is used in the own gateway
     * @param channel Establishes a channel for the communication. Its use depends on actual communications subsystem
     * @param peerType Role that peer plays into the system, node or gateway.
     * @return Returns `true` if the communication subsystem was successfully initialized, `false` otherwise
     */
    virtual bool begin(uint8_t channel, wifi_interface_t phy_interface) = 0;

    /**
     * @brief Terminates communication and closes all connectrions
     */
    virtual void stop() = 0;

    virtual uint8_t getPrimaryChannel() = 0;

    virtual wifi_second_chan_t getSecondaryChannel() = 0;

    virtual bool setChannel(uint8_t primary, wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE) = 0;

    /**
     * @brief Sends data to the other peer
     * @param da Destination address to send the message to
     * @param data Data buffer that contain the message to be sent
     * @param len Data length in number of bytes
     * @return Returns sending status. 0 for success, any other value to indicate an error.
     */
    virtual comms_send_error_t send(const uint8_t *da, const uint8_t *data, size_t len) = 0;

    /**
     * @brief Attach a callback function to be run on every received message
     * @param dataRcvd Pointer to the callback function
     */
    // virtual void onDataRcvd(comms_hal_rcvd_data dataRcvd) = 0;

    /**
     * @brief Attach a callback function to be run after sending a message to receive its status
     * @param dataRcvd Pointer to the callback function
     */
    // virtual void onDataSent(comms_hal_sent_data dataSent) = 0;

    virtual void onDataReceived(frame_rcvd_data frame_rcvd_cb) = 0;

    virtual void onDataSent(frame_sent_data frame_sent_cb) = 0;

    /**
     * @brief Get address length that a specific communication subsystem uses
     * @return Returns number of bytes that is used to represent an address
     */
    virtual uint8_t getAddressLength() = 0;

    /**
     * @brief Get max message length for a specific communication subsystems
     * @return Returns number of bytes of longer supported message
     */
    virtual uint8_t getMaxMessageLength() = 0;

    /**
     * @brief Enables or disables transmission of queued messages. Used to disable communication during wifi scan
     * @param enable `true` to enable transmission, `false` to disable it
     */
    virtual void enableTransmit(bool enable) = 0;
};

#endif