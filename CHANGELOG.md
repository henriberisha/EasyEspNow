## EasyEspNow 2.0.0 (December 2024)

Upgrade to extend functionality to support native CCMP encryption by setting PMK globally and LMK per peer.

- Added member variable flag `pmk_is_set` to keep track if PMK is set or no.
- Added member variable array `pmk` with size `16` to store PMK globally.
- Added member function `setPMK(const uint8_t *pmk_to_set)` to set the PMK provided by the user.

- Added member function `getPMK(uint8_t *pmk_buff)` to get/retrieve the PMK that was previously set. Copies the content into the buffer that user provides.
- Changed name of static variable from `LMK_LENGTH` to `KEY_LENGTH` to make it more general as both **PMK** and **LMK** have the same length of **16** bytes each.
- Added `bool encrypted` flag in `peer_t` structure to keep track if a peer is encrypted or no.
- Set `time_peer_added` in `peer_t` structure to default value `0` to be able to tell that a peer is not in the list and not found.
- Added argument `lmk` to the function `addPeer()` to encrypt the peer. If null, peer will not be encrypted.
- Added structure `espnow_frame_header_common_t` to contain the MAC header which is common for unencrypted and CCMP encrypted frames.
- Modified and more more compact `espnow_frame_format_t` structure to include `espnow_frame_header_common_t` struct.
- Added structure `espnow_frame_format_ccmp_t` to handle CCMP encrypted frame.
- Modified `espnow_frame_recv_info_t` structure to include a pointer to `espnow_frame_format_t` and `espnow_frame_format_ccmp_t`. However one of them will be `nullptr` depending which frame comes in.
- Modified condition to not encrypt a peer. A peer cannot be encrypted if: `LMK is nullptr` OR `peer's MAC is multicast/broadcast` OR `PMK has not been set yet`.
- 🧙🏻‍♂️ 🪄 Modified `rx_cb` function. Here all the magick happens. Handles properly unencrypted or encrypted incoming frames. First it unpacks the incoming bytes into both unencrypted and encrypted structures. Then it checks if the fields containing the specific info for the frame align.

## EasyEspNow 1.0.0 (November 2024)

Inital release of the library
