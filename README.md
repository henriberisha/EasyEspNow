# An easy-to-use ESP-NOW wrapper library for ESP32 platform that simplifies peer communication and data handling.

---

## Contents

[Credits & Disclaimer 🏆](#credits--disclaimer)
[ESP-NOW References 📚](#esp-now-references)
[Boards Compatibility ⬇️](#boards-compatibility)
[TODO ↗️](#todos)
[Examples 👀💡](#examples)
[Technical Explanations ⚠️](#technical-explanations)
[Debugger 🐛](#debugger)
[EasyEspNow API Functionality 📝🔍](#api-functionality)
[Guide How to use send() depending on mode 📜](#guide-on-using-send-to-avoid-packet-drop)
[About Encryption 🔐 🔓](#some-words-about-encryption)

### Credits & Disclaimer

I want to give great credits to the main source from which i got the inspiration to work on this library and on which i based a lot of the logic and concepts used in the code. This source is the repository: [gmag11/QuickESPNow](https://github.com/gmag11/QuickESPNow) with Germán Martín as a maintainer. I included the license with the original copyright. In addition i appended another copyright that would cover any functionalities that I added.  
When i started working on `EasyEspNow` source code, I initially wanted to learn more about this protocol called `ESP-NOW` and was coding different sketches using the low level API that ESP-NOW has. In addition, i wanted to keep my coding skills sharp and practice more on C/C++. As the work continued, it turned into the shape of a library which aimed to facilitate the use and implementation of `ESP-NOW`. Finally i decided to publish it as an open source library with the goal to help other fellow programmers.  
If you are looking for a more mature library i would strongly advise you to look into [gmag11/QuickESPNow](https://github.com/gmag11/QuickESPNow). It also has around 95 ⭐ as per this time.

### ESP-NOW References

[What is ESP-NOW?](https://www.espressif.com/en/solutions/low-power-solutions/esp-now)
[ESP-NOW low level API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html)
[ESP-NOW Example 1](https://github.com/espressif/esp-idf/tree/v5.2.3/examples/wifi/espnow)
[More examples](https://github.com/espressif/esp-now)

### Boards Compatibility

I have build `EasyEspNow` library for ESP32 board version `2.0.17` This library will not work with board versions `3.x` due to some low level API differences. In addition, I have not setup the development environment for boards `3.x` and that has been a limitation for me.
At this time i am not sure if it will work with board versions `< 2.0.17`

### TODOs

- Extend functionality to support native CCMP encryption by setting `PMK` globally and `LMK` per peer.
- Extend this library to work for `ESP8266` boards. Currently i do not have the time bandwidth to work on it.
- Modify this library to work for board versions `>= 3.x`

### Examples

- `QuickStart.ino` -> basic functionality, START HERE
- `AllFunctions.ino` -> extended functionality showcasing full API
- `ProcessRX.ino` -> how to process RX messages in the main sketch by the user in a similar fashion how TX is processed by the library in the background. This also shows how TX and RX happen together in the same runtime. Note: You will need another device that is sending data either to Broadcast MAC or Receiver device MAC.
- `EncryptedSender.ino` and `EncryptedReceiver.ino` -> these sketches show how to encrypt data in user level and send it encrypted. On the other hand, data is received, decrypted. This example was needed because user must have the ability to send encrypted data. For now this library does not support the native `ESP-NOW` encryption which requires setting `PMK` and `LMK`.

### Technical Explanations

- It is possible to use `Wireshark` to sniff promiscuous `ESP-NOW` packets in `802.11`. You will need to put in monitor mode, have a network card that supports monitor mode such as an `Alfa`. In addition, you will need to set the channel in which you are monitoring to match the channel that `ESP-NOW` communication is happening. Filter for `Action Frames` => `wlan.fc.type_subtype == 0x000d`.
- In `unicast`, a device sends `ESP-NOW` message with an `Action Frame` and the receiver should reply with `ACK` frame => `wlan.fc.type_subtype == 0x001d`.
- A device can send a message successfully, but that does not mean that the `Delivery status` is `true`. Delivery is true only when the receiver responds to the sender with the `ACK` frame. Basically sending does not mean delivery.
- For `broadcast`, `Delivery` is always `true` because it does not expect an acknowledgment. Sending will be the same as delivering.

- This back and forth is handled by the low level `ESP-NOW API` and you do not need to worry about it.
- The source code is fully documented with block/inline comments for every function.
- Encryption is not supported by this library. The reason is that makes it hard to properly parse the promiscuous packet to extract the frame, and limits the number of peers. I recommend using encryption in higher level in your main sketch. [Crypto](https://github.com/OperatorFoundation/Crypto/blob/master/examples/AES128_Basics/AES128_Basics.ino) library is a great one to add encryption to your code. Pass the encrypted message and its length to `send(...)` function.
- When WiFi mode is selected, the appropriate corresponding WiFi interface must be selected, otherwise will have issues with error `ESP_ERR_ESPNOW_IF`. See `autoselect_if_from_mode(...)` for more.

```
WIFI_MODE_STA  --->  WIFI_IF_STA
WIFI_MODE_AP  --->  WIFI_IF_AP
WIFI_MODE_APSTA  --->  WIFI_IF_AP or WIFI_IF_STA (will work either or, does not matter much)
WIFI_MODE_NULL or WIFI_MODE_MAX  --->  useless for ESP-NOW
```

- The MAC address of this device will be correspondent to the WiFi interface selected. `WIFI_IF_STA` has a different MAC from `WIFI_IF_AP`

* Maximum 20 peers allowed (this is dictated by ESP-NOW API.)
* Radiotap information (including RSSI) and complete ESP-NOW frame returned in the receive callback for more user control.
* Peer management and peer reference list with last seen information.
* Synchronous (defaul mode) and asynchronus send mode. If synchronous, TX queue will default to size=1 and have only space for one message at a time. Next send will happen after the current sent and no packet drop will occur. If asynch. TX queue can keep more than one message and send them one after the other. If TX queue is full in asynch mode, the messages will be dropped.
* Only TX data processing under the hood. RX data must be processed by the user. Can get the received messages in the function `onDataReceived(...)` which will be in your main sketch.
* If destination is `NULL` in the `send()` function, message will be sent to all unicast peers as per ESP-NOW API.
* When a peer is added, only the following info structure is used for the peer by `EasyEspNow` library:

```c
esp_now_peer_info_t peer_info;
memcpy(peer_info.peer_addr, peer_addr_to_add, MAC_ADDR_LEN); // MAC address
peer_info.ifidx = wifi_phy_interface; // setting WiFi interface
peer_info.channel = wifi_primary_channel; // setting WiFi channel, must be the same that the device is on
peer_info.encrypt = false; // encryption not supported
```

- When adding peers and some details about peer info structure:
  - Once a peer is added, you cannot modify the MAC address of an existing peer using the `esp_now_mod_peer()` function. The MAC address is a fundamental identifier for the peer, and once a peer is added, its MAC address is fixed in the peer list. Better delete that peer and add again with proper MAC.
  - Peer can be in a different WiFi interface from the home (this station) and still receive the message.
  - This is more relevant to ESP-NOW API, encrypted peers are not accepted for multicast/broadcast addresses.

### Debugger

This library comes with an-out-of-the-box basic debugger with different log levels defined at this header file: `easy_debug.h`.

- Any print in your sketch that uses `Serial.print` will not be affected by the log level.
- In your main sketch you need to define the log level. Set to `LOG_NONE` if not interested in logs.
- Set a `TAG` for the logging macros to use.
- Logging macros can take arguments just like `printf` does to format your output and include variable values.

```c
// Define the log levels
#define LOG_NONE 0 // to not output any logs
#define LOG_MONITOR 1
#define LOG_ERROR 2
#define LOG_WARNING 3
#define LOG_INFO 4
#define LOG_DEBUG 5
#define LOG_VERBOSE 6
```

Macros to help format and print a MAC address in upper case

```c
#define EASYMAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define EASYMACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

// How to use in code
uint8_t some_MAC[] = {0xCD, 0x56, 0x47, 0xFC, 0xAF, 0xB3};
MONITOR(TAG, "Mac: " EASYMACSTR " This was some MAC address" , EASYMAC2STR(some_MAC));
```

### API Functionality

To use the `EasyEspNow` library in a main sketch, include `EasyEspNow.h` header file.

```c
 #include <EasyEspNow.h>
```

> To date, the following low level ESP-NOW functions are not supported by this library:
> `esp_wifi_config_espnow_rate(wifi_interface_t ifx, wifi_phy_rate_t rate)`
> esp_now_set_pmk(const uint8_t \*pmk)
> esp_now_set_wake_window(uint16_t window).
> I believe that you can still use them in conjugtion with this library in your main sketch. Just need to call the ESP-NOW API directly.
> For example you may use `esp_wifi_config_espnow_rate()` after `begin()` to specify a rate

#### ===> Core Functions

```c
begin(channel, phy_interface, tx_q_size, synch_send) // begin everything, set channel, wifi interface, tx queue size, synchronous send. If synch. send true => tx size will default to 1
stop() // stop everything
easy_send_error_t send(dstAddress, payload, payload_len) // to enqueu message for send with specific length to destination address
easy_send_error_t sendBroadcast(payload, payload_len) // just a call to send() with Broadcast address as destination
enableTXTask(enable) // enable or disable the TX task responsible for exhausting TX queu and sending the messages
readyToSendData() // readinnes to send if TX has space, if full not ready
waitForTXQueueToBeEmptied() // blocking function to wait until TX queue is empty
onDataReceived(frame_rcvd_cb) // to register user defined callback function upon receiving data. Higher level
onDataSent(frame_sent_cb) // to register user defined callback function upon sending data. Higher level
```

#### ===> Peer Management Functions

Peer Management involves having a defined structure that keeps track of the peers added. ESP-NOW low level API does not have a proper way to deliver that. hence it is needed to have a reference of the peers in Higher level to allow more flexibility. The change in these structures happens in parallel with what ESP-NOW does in lower level such as when adding or deleting peers

```c
typedef struct
{
	uint8_t mac[MAC_ADDR_LEN]; // MAC address of the peer
	uint32_t time_peer_added; // last time a peer was seen; millis()
} peer_t;

typedef struct
{
	uint8_t peer_number; // totoal number of peers
	peer_t peer[MAX_TOTAL_PEER_NUM]; // list of peers
} peer_list_t;
```

```c
addPeer(peer_addr_to_add) // add peer with provided MAC address
deletePeer(peer_addr_to_delete); // delete peer with provided MAC address
uint8_t *deletePeer(keep_broadcast_addr = true) // this deletes the oldest peer and returns its MAC. It can delete the broadcast peer too if it is the oldest and `keep_broadcast_addr = false`
peer_t *getPeer(peer_addr_to_get, esp_now_peer_info_t &peer_info) // returns peer_t structure for the peer and puts the info in the peer_info structure
peerExists(peer_addr) // check if peer exists or no
updateLastSeenPeer(peer_addr) // update last seen value for peer with MAC address
countPeers(CountPeers count_type) // count peers, total | unencrypted | encrypted
printPeerList() // prints the peer list, used more for debugging
peer_list_t getPeerList() // this will return the complete peer_list structure for the user's convinience. returns it by value
```

#### ===> Miscellaneous Functions

These are functions that can be useful depending on the use case

```c
const char *easySendErrorToName(easy_send_error_t send_error) // returns the send error as a char array
wifi_interface_t autoselect_if_from_mode(wifi_mode_t mode, bool apstaMOD_to_apIF = true) // helper function to determine wifi interface depending on wifi mode
char *easyMac2Char(const uint8_t *some_mac, size_t len = MAC_ADDR_LEN, bool upper_case = true) // return a MAC as a char array for easy print, if any issue will default to {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
easyPrintMac2Char(const uint8_t *some_mac, size_t len = MAC_ADDR_LEN, bool upper_case = true) // just print the MAC, if any issue will default to {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
getEspNowVersion() // esp-now version
getPrimaryChannel() // returns channel in which wifi radio is on, same channel in which esp-now frames will be sent
wifi_second_chan_t getSecondaryChannel() // secondary channel
getAddressLength() // length of a MAC address (6 because there are 6 octets in a MAC)
getMaxMessageLength() // max length of a message that can be delivered via ESP-NOW
getDeviceMACAddress() // return MAC address of this device
uint8_t *generateRandomMAC(bool local = true, bool unicast = true) // generate random mac, can be local/global unicast/multicast
switchChannel(uint8_t primary, wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE) // switches operating wifi channel on the fly, updates channel for all peers in their info. The switch of channel for this base station and for the peers it holds may result in messages not being sent to destination or received from source peers due to channel change. Handle carefully.
```

#### ===> Important Structures

```c
/**
 * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html#frame-format
 * ESP-NOW Unencrypted frame format, without (CCMP)
 * This would be the promiscuous packet
 * It is an Action frame. On 802.11 managment type (0), subtype 0x0d (13)
 */
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

/**
 * This is the structure that is returned to the user in the receive callback function
 * It contains all the radio header information, and the complete frame
 * This gives user more control on the code
*/
typedef struct
{
    wifi_pkt_rx_ctrl_t *radio_header;  // radio metadata includin rssi and much more
    espnow_frame_format_t *esp_now_frame;  // complete above frame
} espnow_frame_recv_info_t;
```

### Guide on using `send()` to avoid packet drop

Best approach is to have the send rate lower than TX queue exhaust rate
TX exhaust rate has a delay of 13us. If send rate has a delay larger than 13us for example 20us, things should be good and you should not see any packet drop

```c
/* SYNCHRONOUS MODE */
// assures no packet drop when sending
void loop()
{
    String data = "Hello World";
    // Here is doing only Broadcasting
    easy_send_error_t code = easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)data.c_str(), data.length());
    MONITOR(MAIN_TAG, "Last send return code value: %s\n", easyEspNow.easySendErrorToName(code));

    // add some delay to simulate longer code run
    // if this is faster than TX exhaust rate (i have set it in the code to be `vTaskDelay(pdMS_TO_TICKS(13)`
    // you will get this: [115535] [WARNING] [EASY_ESP_NOW]: Synchronous send mode. Waiting for free space in TX Queue
    // otherwise no WARNING.
    vTaskDelay(pdMS_TO_TICKS(13));
}
```

```c
/* ASYNCHRONOUS MODE */
// TX queue can hold more than 1 outgoing message
// packet dropping can happen when TX queue is full and a message can't be enqued
void loop()
{
    String data = "Hello World";
    // Here is doing only Broadcasting
    easy_send_error_t code = easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)data.c_str(), data.length());
    MONITOR(MAIN_TAG, "Last send return code value: %s\n", easyEspNow.easySendErrorToName(code));

    // add some delay to simulate longer code run
    // if this is significantly faster than TX exhaust rate (i have set it in the code to be `vTaskDelay(pdMS_TO_TICKS(13)`
    // you will get this: [31220] [WARNING] [EASY_ESP_NOW]: TX Queue full. Can not add message to queue. Dropping message...
    // packet drop
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

```c
/* ASYNCHRONOUS MODE */
// TX queue can hold more than 1 outgoing message
// how to mitigate packet dropping when TX queue is full
void loop()
{
    String data = "Hello World";
    // condition is needed to avoid packet drop only when doing asynch
    if (!easyEspNow.readyToSendData()) // if TX queue is full, wait for it to completely exhaust
        easyEspNow.waitForTXQueueToBeEmptied();
    easy_send_error_t code = easyEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t *)data.c_str(), data.length());
    MONITOR(MAIN_TAG, "Last send return code value: %s\n", easyEspNow.easySendErrorToName(code));

    // add some delay to simulate longer code run
    // if this is significantly faster than TX exhaust rate (i have set it in the code to be `vTaskDelay(pdMS_TO_TICKS(13)`
    // packet drop mitigation condition will take place
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### Some Words About Encryption

This library does not support ESP-NOW API's encryption mechanism. However, it is important for me to share some of my findings related to the encryption. I think it may be useful to anyone that desires to use directly the `ESP-NOW API`. Can't set encryption for multicast peers such as `broadcast` MAC. Setting `PMK` only will not encrypt anything. You need to set the `LMK` for the specific peer to achieve `CCMP` level encryption for the frame. If encryption is successful, you will see that data will be encrypted in `Wireshark`. In my understanding, for every pair of peers you will need an `LMK`. Or you can use the same `LMK` across the board. For example:

```txt
There are 3 devices. Device A, B, C and none of them has a multicast MAC.
There is the same `PMK` for all devices that needs to be set with `esp_now_set_pmk(const uint8_t *pmk);`
In order for any of these devices to talk to each other in an encrypted way they need to have each other in the peer list and have the same `LMK`

A has B as a peer AND
B has A as a peer AND
Both have the same `LMK` in the peer info
=>> A and B can exchange encypted messages

A has C as a peer AND
C has A as a peer AND
Both have the same `LMK` in the peer info
=>> A and C can exchange encypted messages

B has C as a peer AND
C has B as a peer AND
Both have the same `LMK` in the peer info
=>> B and C can exchange encypted messages

Now the above `LMKs` can be the same or different. It is important that between every 2 devices the `LMK is the same when they set each other as each other's peer.
```

```c
uint8_t PMK[] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x7A, 0x8B, 0x9C, 0xAD, 0xBE, 0xCF, 0xDA, 0xEB, 0xFC, 0x0D};
uint8_t LMK1[] = {0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
uint8_t LMK2[] = {0xFB, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

// On sketches for Device A, B, C
esp_now_init();
esp_now_set_pmk(PMK);

// On device A adding B as a peer
esp_now_peer_info_t peerInfo;
memcpy(peerInfo.peer_addr, MAC_B, 6);
peerInfo.channel = some_channel;
peerInfo.encrypt = true;
memcpy(peerInfo.lmk, LMK1, 16);
esp_now_add_peer(&peerInfo);

// On device B adding A as a peer
esp_now_peer_info_t peerInfo;
memcpy(peerInfo.peer_addr, MAC_A, 6);
peerInfo.channel = some_channel;
peerInfo.encrypt = true;
memcpy(peerInfo.lmk, LMK1, 16);
esp_now_add_peer(&peerInfo);
/* Now A and B can talk in encrypted way using ESP-NOW API */

///////////////////////////////////////////////////////////////////////
// On device A adding C as a peer
esp_now_peer_info_t peerInfo;
memcpy(peerInfo.peer_addr, MAC_C, 6);
peerInfo.channel = some_channel;
peerInfo.encrypt = true;
memcpy(peerInfo.lmk, LMK2, 16);
esp_now_add_peer(&peerInfo);

// On device C adding A as a peer
esp_now_peer_info_t peerInfo;
memcpy(peerInfo.peer_addr, MAC_A, 6);
peerInfo.channel = some_channel;
peerInfo.encrypt = true;
memcpy(peerInfo.lmk, LMK2, 16);
esp_now_add_peer(&peerInfo);

/* Now A and C can talk in encrypted way using ESP-NOW API */

///////////////////////////////////////////////////////////////////////
// On device B adding C as a peer
esp_now_peer_info_t peerInfo;
memcpy(peerInfo.peer_addr, MAC_C, 6);
peerInfo.channel = some_channel;
peerInfo.encrypt = true;
memcpy(peerInfo.lmk, LMK1, 16);
esp_now_add_peer(&peerInfo);

// On device C adding B as a peer
esp_now_peer_info_t peerInfo;
memcpy(peerInfo.peer_addr, MAC_B, 6);
peerInfo.channel = some_channel;
peerInfo.encrypt = true;
memcpy(peerInfo.lmk, LMK1, 16);
esp_now_add_peer(&peerInfo);

/* Now B and C can talk in encrypted way using ESP-NOW API */

/** Bottom line:
 *  A uses LMK1 to talk to B and LMK2 to talk to C
 *  B uses LMK1 to talk to A and LMK1 to talk to C
 *  C uses LMK2 to talk to A and LMK1 to talk to B
*/

```
