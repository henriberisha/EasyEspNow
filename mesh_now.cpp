#include "mesh_now.h"

constexpr auto TEST_TAG = "TEST";
constexpr auto TAG = "MESH_NOW_ESP";

// MeshNowEsp mesh;

bool Test::begin()
{
    Serial.println("Begin Test");

    INFO(TEST_TAG, "Info message");
    MONITOR(TEST_TAG, "TEST");
    ERROR(TEST_TAG, "An error occurred: %s", "Connection failed");
    WARNING(TEST_TAG, "Warning value: %d", 42);
    INFO(TEST_TAG, "Info message");
    DEBUG(TEST_TAG, "Debug: %s", "Connection failed");
    VERBOSE(TEST_TAG, "vhgc");
    return true;
}

bool MeshNowEsp::begin(uint8_t channel, wifi_interface_t phy_interface)
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

    this->wifi_mode = mode;
    this->wifi_phy_interface = phy_interface;

    return true;
}

bool MeshNowEsp::setChannel(uint8_t primary_channel, wifi_second_chan_t second)
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

void MeshNowEsp::stop()
{
}

int32_t MeshNowEsp::getEspNowVersion()
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

comms_send_error_t MeshNowEsp::send(const uint8_t *dstAddress, const uint8_t *payload, size_t payload_len)
{
}

void MeshNowEsp::onDataRcvd(comms_hal_rcvd_data dataRcvd)
{
}

void MeshNowEsp::onDataSent(comms_hal_sent_data sentResult)
{
}

void MeshNowEsp::enableTransmit(bool enable)
{
}

void MeshNowEsp::initComms()
{
}