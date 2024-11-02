# An easy-to-use ESP-NOW wrapper library for ESP32 platform that simplifies peer communication and data handling.

---

## Contents

[Credits & Disclaimer](#credits--disclaimer)
[ESP-NOW References](#esp-now-references)
[Boards Compatibility](#boards-compatibility)
[TODO](#todos)
[Technical Explanations](#technical-explanations)
[EasyEspNow API Functionality](#api-functionality)
[About Encryption](#some-words-about-encryption)

### Credits & Disclaimer

I want to give great credits to the main source from which i got the inspiration to work on this library and on which i based a lot of the logic and concepts used in the code. This source is the repository: [gmag11/QuickESPNow](https://github.com/gmag11/QuickESPNow) with Germán Martín as a maintainer. I included the MIT license with the original copyright. In addition i appended another copyright that would cover any functionalities that I added.  
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

- Extend this library to work for `ESP8266` boards. Currently i do not have the time bandwidth to work on it.
- Modify this library to work for board versions `>= 3.x`

### Technical Explanations

- The source code is fully documented with block/inline comments for every function.
- Encryption is not supported by this library. The reason is that makes it hard to properly parse the promiscuous packet to extract the frame. I recommend using encryption in higher level in your main sketch. [Crypto](https://github.com/OperatorFoundation/Crypto/blob/master/examples/AES128_Basics/AES128_Basics.ino) library is a great one to add encryption to your code. Pass the encrypted message and its length to `send(...)` function.
- When WiFi mode is selected, the appropriate corresponding WiFi interface must be selected, otherwise will have issues. See `autoselect_if_from_mode(...)` for more.
- Maximum 20 peers allowed (this is dictated by ESP-NOW API.)
- Radiotap information (including RSSI) and complete ESP-NOW frame returned in the receive callback for more user control.
- Peer management and peer reference list with last seen information.
- Only TX data processing under the hood. RX data must be processed by the user. Can get the received messages in the function `onDataReceived(...)` which will be in your main sketch.
- If destination is `NULL` in the `send()` function, message will be sent to all unicast peers as per ESP-NOW API.
-

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

### Some Words About Encryption
