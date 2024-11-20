## EasyEspNow 2.0.0 (December 2024)

Upgrade to extend functionality to support native CCMP encryption by setting PMK globally and LMK per peer.

- Added member variable flag `pmk_is_set` to keep track if PMK is set or no.
- Added member variable array `pmk` with size `16` to store PMK globally.
- Added member function `setPMK(const uint8_t *pmk_to_set)` to set the PMK provided by the user.

- Added member function `getPMK(uint8_t *pmk_buff)` to get/retrieve the PMK that was previously set. Copies the content into the buffer that user provides.
- Changed name of static variable from `LMK_LENGTH` to `KEY_LENGTH` to make it more general as both **PMK** and **LMK** have the same length of **16** bytes each.

## EasyEspNow 1.0.0 (November 2024)

Inital release of the library
