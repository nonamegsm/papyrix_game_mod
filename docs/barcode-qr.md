# Barcode and QR Codes

Both apps generate codes for display on the reader. They work offline once the
data has been saved. Code areas use black modules on a white background, even
when the rest of the interface uses a dark theme.

## Barcode

1. Open **Apps > Barcode**.
2. Choose **Code 128** or **EAN-13**.
3. Enter the data with the on-screen keyboard. Tap keys on the X4 Pro, or use
   the direction buttons and Center to select a key.
4. Select **Confirm** in the keyboard's top control row to save and display it.

**Code 128** accepts printable ASCII letters, numbers, and symbols, up to 63
characters. Even-length numeric values use the more compact Code C encoding.
The code must fit the current screen at an integer module width; a long value
that does not fit produces an error. Use landscape orientation or shorten it.

**EAN-13** accepts 12 or 13 digits. A 12-digit value gets its check digit added.
For a 13-digit value, the check digit must already be correct. Invalid values
remain in the editor with an error rather than producing an incorrect code.

The last successfully saved barcode and its format are kept on the SD card.
**Edit** opens the keyboard with its data; **Format** opens the format chooser.
Back cancels an edit and restores the saved code. Back from the display exits.

## QR Codes from the web interface

1. Open **Apps > WiFi Transfer** and join a network or create a hotspot.
2. Connect your phone/computer and open the reader's displayed web address.
3. Open the **QR Codes** tab. Enter a name and the exact text or URL to encode.
4. Select **Save Code**. Existing codes can be edited or deleted in the same tab.
5. Leave WiFi Transfer. It may restart the reader as part of the normal exit.
6. Open **Apps > QR Codes** and select a saved code to display it.

Up to 16 named codes can be saved. Names are limited to 48 UTF-8 bytes and
payloads to 512 UTF-8 bytes, so a non-ASCII character can use more than one byte.
Newlines and valid UTF-8 text are preserved. The web form shows byte counters
and rejects oversized or invalid data. Codes are stored on the SD card in
`/.papyrix/apps/qrcodes/`; they remain available after Wi-Fi is turned off or the
reader restarts.

Use Left/Right or Up/Down to cycle codes while displaying one. **List** returns
to the chooser. The chooser pages longer lists to fit each screen orientation.
Back exits the app. QR codes use medium error correction and a four-module
quiet zone; the encoder chooses a version large enough for the payload.

Codes are generated from the data you enter. The apps do not read a barcode
from a camera or turn an uploaded code image into text.
