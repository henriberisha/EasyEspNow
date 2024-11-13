/*

AES encryption requires data to be in multiples of the AES block size, which is 16 bytes for AES-128.
If your data isn't already a multiple of 16 bytes, you have two main options:

    * Add Padding: This is the most common approach. You pad the data to make its length a multiple of the block size before encryption, and then remove the padding after decryption.
    * Use a Stream Cipher Mode: In certain cases, you could use a stream mode (like AES-CTR or AES-OFB), which doesn’t require padding. However, in many cases, CBC mode is preferred for security.

Here we will use padding

Padding with PKCS#7

PKCS#7 padding is a widely used padding scheme. Here’s how it works:

    Padding: If your data is not a multiple of 16 bytes, you add extra bytes, each with a value equal to the number of bytes of padding.
    Unpadding: After decryption, you check the last byte to know how many padding bytes to remove.

    To find the padding value (how many bytes to pad in the end of the data buffer) use the formula:
        *** byte paddingValue = blockSize - (dataSize % blockSize) ***

    if your data is 14 bytes long, you add 2 bytes of padding: 0x02, 0x02
    if your data is 16 bytes long, you add 16 bytes of padding: 0x10, 0x10, 0x10, 0x10, ... , 0x10, 0x10, 0x10, 0x10
    if your data is 28 bytes long, you add 4 bytes of padding: 0x04, 0x04, 0x04, 0x04
    and so forth

*/

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
#include <Crypto.h>
#include <AES.h>

uint8_t channel = 7;                            // sender and receiver must be on the same channel
int CURRENT_LOG_LEVEL = LOG_VERBOSE;            // need to set the log level, otherwise will have issues
constexpr auto MAIN_TAG = "RECEIVER_ENCRYPTED"; // need to set a tag

// this could be the MAC of one of your devices, replace with the correct one
uint8_t some_mac[] = {0xCD, 0x56, 0x47, 0xFC, 0xAF, 0xB3};

/* Encryption object and key */
// the key must be the same for sender and receiver
AES128 aes128;                                                                                                   // AES-128 (16-byte key)
byte key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}; // 16-byte key for AES-128

// Make sure this is the same for sender and receiver
#define MAX_STRING_LEN 33 // chars // make this larger than max data to see the fail in encryption

// The data structure must be the same for both sender and receiver
// Must keep the size of the structure to a maximum of 250 bytes as per ESP-NOW max data send length
typedef struct
{
    boolean flag;
    char character;
    char str[MAX_STRING_LEN];
    int num;
    float value;
    double other_value;
} some_message_t;

/* Takes care of adding padding as needed before encryption */
bool encryptData(const byte *plain_text_buff, size_t plain_text_size, byte *encrypted_buff, size_t &encrypted_size, size_t block_size, size_t max_data_size)
{
    // Round down max_data_size to the nearest multiple of AES block size
    size_t max_padded_size = (max_data_size / block_size) * block_size;
    // Subtract 1 byte to leave room for at least one padding byte
    size_t max_allowed_plain_text_size = max_padded_size - 1;

    if (plain_text_size < 1 || plain_text_size > max_allowed_plain_text_size)
    {
        Serial.println("\nError! Encryption impossible. Plain text size is less than 1 or exceeds max encryptable size that can fit within \"max_data_size\" constrain.");
        return false;
    }

    // if plain text size is within the allowed range that makes it a multiple of block size after padding

    size_t padding = block_size - (plain_text_size % block_size);

    size_t total_size = plain_text_size + padding;
    byte plain_text_padded_buff[total_size]; // Make sure this is large enough for your data + padding
    memcpy(plain_text_padded_buff, plain_text_buff, plain_text_size);
    for (size_t i = plain_text_size; i < total_size; i++)
    {
        plain_text_padded_buff[i] = padding;
    }

    Serial.printf("\nPadding value: %02X\n", plain_text_padded_buff[total_size - 1]);

    // For debug only, print the plain text padded
    Serial.printf("\nPlain Text Padded as HEX Before Encryption (Length = %d):\n", total_size);
    for (int i = 0; i < total_size; i++)
    {
        Serial.printf("%02X", plain_text_padded_buff[i]);
    }
    Serial.println();

    // Encrypt block by block
    for (size_t i = 0; i < total_size; i += block_size)
    {
        aes128.encryptBlock(encrypted_buff + i, plain_text_padded_buff + i);
    }

    encrypted_size = total_size;
    return true;
}

/* Takes care of adding padding as needed before encryption */
bool decryptData(const byte *encrypted_buff, size_t encrypted_size, byte *decrypted_buff, size_t &decrypted_size, size_t block_size)
{
    byte decrypted_padded_buff[encrypted_size];
    // Decrypt block by block
    for (size_t i = 0; i < encrypted_size; i += block_size)
    {
        aes128.decryptBlock(decrypted_padded_buff + i, encrypted_buff + i);
    }

    size_t final_decrypted_size = encrypted_size;                    // set initial value
    byte padding_length = decrypted_padded_buff[encrypted_size - 1]; // Last byte shows padding length

    Serial.printf("\nPaded size: %d\n", padding_length);

    // Check if padding is valid
    bool isValidPadding = padding_length > 0 && padding_length <= 16;
    for (size_t i = 0; i < padding_length && isValidPadding; i++)
    {
        if (decrypted_padded_buff[encrypted_size - 1 - i] != padding_length)
        {
            isValidPadding = false;
        }
    }

    // Remove padding if valid
    if (isValidPadding)
    {
        final_decrypted_size -= padding_length;
    }
    else
    {
        Serial.println("Invalid padding detected!");
        return false;
    }

    memcpy(decrypted_buff, decrypted_padded_buff, final_decrypted_size);
    decrypted_size = final_decrypted_size;
    return true;
}

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

    Serial.printf("=========> START incoming message <=========\n");
    Serial.printf("Comms Received: SENDER_MAC: %s, DEST_MAC: %s\n"
                  "RSSI: %d, CHANNEL: %d, TYPE: %d, SUBTYPE: %d\n",
                  sender_mac_char, destination_mac_from_frame_char,
                  rssi, channel, frame_type, frame_subtype);

    /* This is how you normally would print the data */
    // Serial.printf("Data body length: %d. ", len);
    // Serial.printf("Data Message: %.*s\n\n", len, data);

    ///////////// Load the Cipher /////////////

    size_t cipher_size = len;
    byte cipher[cipher_size];
    Serial.printf("\nContents of the encrypted message (Length = %d):\n", len);
    for (int i = 0; i < len; i++)
    {
        Serial.printf("%02X", data[i]);
        cipher[i] = data[i];
    }

    ///////////// Decrypt the Cipher.  Decryption will remove padding and done block by block as determined by the block size /////////////

    byte decrypted[len];
    size_t decrypted_size;
    bool decryption_status = decryptData(cipher, cipher_size, decrypted, decrypted_size, aes128.blockSize());
    if (decryption_status == false)
        return; // if decryption fails, no point to continue

    Serial.println("\n");
    Serial.printf("Plain Text as HEX After Decryption (Length = %d):\n", decrypted_size);
    for (int j = 0; j < decrypted_size; j++)
    {
        Serial.printf("%02X", decrypted[j]);
    }

    ///////////// Recreate the original message struct from decrypted cipher /////////////

    some_message_t decrypted_message;
    memcpy(&decrypted_message, decrypted, decrypted_size);

    Serial.println("\n");
    Serial.printf("Contents of the structure - After Decryption (Size = %d):\n"
                  "-Flag = %s\n"
                  "-Character = %c\n"
                  "-String = %s\n"
                  "-Number = %d\n"
                  "-Float = %.4f\n"
                  "-Double = %.8f\n\n",
                  sizeof(decrypted_message),
                  decrypted_message.flag == 0 ? "false" : "true", decrypted_message.character,
                  decrypted_message.str, decrypted_message.num, decrypted_message.value, decrypted_message.other_value);
    Serial.printf("=========> FINISH incoming message <=========\n\n");

    /* Here you should further process your RX messages as needed */
}

void setup()
{
    Serial.begin(115200);
    delay(3000);

    WiFi.mode(wifi_mode);
    WiFi.disconnect(false, true); // use this if you do not need to be on any WiFi network

    // Set AES key
    aes128.setKey(key, sizeof(key));

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

    /* For the receiver you dont need to add peers, unless this device will also send data */
    // Must add Broadcast MAC as a peer in order to send Broadcast
    // easyEspNow.addPeer(ESPNOW_BROADCAST_ADDRESS);

    // Here you add a unicast peer device, no need to worry about the peer info
    // easyEspNow.addPeer(some_peer_device);

    // Set AES key
    aes128.setKey(key, sizeof(key));
}

void loop()
{
    // your code here
    vTaskDelay(pdMS_TO_TICKS(2000));
}