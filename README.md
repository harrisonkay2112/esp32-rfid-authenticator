# ESP32 RFID and PIN Authenticator

An Arduino C++ prototype that checks an RFID card and keypad PIN before typing a selected username and password through a USB or Bluetooth keyboard interface.

The project combines an MFRC522 RFID reader, a 4x4 keypad, a 20x4 I2C LCD, a DS1307 real-time clock, and a buzzer. It explores peripheral integration, authentication flow, keyboard output, and timed lockout on an ESP32 platform.

The device was built and run on physical hardware in spring 2026 (USB path working April 30, 2026; BLE and profile selection May 1, 2026). The full authentication flow — card check, PIN entry, lockout, profile selection, and keyboard output — was exercised end to end on the wired board. Each peripheral was bench-tested on its own first (RFID reader, keypad, buzzer) before integration.

The published sketch is a sanitized copy of that working build and compiles for the Arduino Nano ESP32 with the toolchain recorded in [validation notes](docs/validation.md). The September 2026 publication pass was documentation and credential sanitization only; it did not re-run the hardware tests, so the notes distinguish what was verified on the bench from what was re-checked at publication time.

![Nano ESP32 pins connected individually to the MFRC522 reader](docs/assets/nano-rfid-wiring.png)

*Pin-to-pin reference based on the sketch and Nano pinout. See the [complete wiring guides](docs/hardware.md) for the keypad, LCD, RTC, and buzzer.*

## Documentation

| Guide | Contents |
| --- | --- |
| [Hardware and connections](docs/hardware.md) | Four illustrated wiring guides, bus diagram, pin mapping, and power references |
| [Firmware architecture](docs/architecture.md) | State diagram, interaction sequence, function map, and implementation tradeoffs |
| [Validation](docs/validation.md) | Hardware validation history, compile record, re-test checklist, and test-record template |
| [Demonstration guide](docs/demo-guide.md) | Steps for showing card rejection, PIN lockout, profile selection, and keyboard output |

## Features

- Match a scanned RFID UID against a configured value.
- Collect a PIN of up to six characters with masked LCD feedback.
- Select from four demonstration credential profiles after the card and PIN checks pass.
- Type the username, Tab, password, and Enter through the selected keyboard interface.
- Switch between USB and Bluetooth output by holding `A` for two seconds when the main loop is active.
- Lock PIN authentication for 30 seconds after three incorrect PIN submissions following an accepted card.
- Print timestamped events to the serial console using the DS1307 clock.
- Decrypt the configured UID and PIN using AES-128 at startup.

## Authentication flow

```mermaid
flowchart TD
    Boot[Initialize peripherals and decrypt UID and PIN] --> Idle[Wait for an RFID card]
    Idle --> Card{UID matches?}
    Card -- No --> Denied[Display denial and sound buzzer]
    Denied --> Idle
    Card -- Yes --> PIN[Collect keypad PIN]
    PIN --> Match{PIN matches?}
    Match -- No --> Count[Increment failed PIN count]
    Count --> Limit{Three failures?}
    Limit -- No --> Denied
    Limit -- Yes --> Lock[Wait for 30-second lockout to expire]
    Lock --> Idle
    Match -- Yes --> Profile[Browse credential profiles]
    Profile -- Cancel --> Idle
    Profile -- Select --> Type[Reset failure count and type credentials]
    Type --> Idle
```

Invalid cards do not increment the PIN failure counter. The counter resets when lockout expires or a profile is selected successfully. Cancelling profile selection does not reset it.

## Repository layout

```text
firmware/
  esp32_authenticator/
    esp32_authenticator.ino
    credentials.example.h
docs/
  assets/
    nano-*.png
    nano-*.svg
  architecture.md
  hardware.md
  demo-guide.md
  validation.md
```

The firmware is a sanitized copy of the original checkpoint. Personal account values have been replaced with demonstration data. Profile configuration is separated into a header, and Tab/Enter output uses character writes for the keyboard APIs.

## Hardware and pin assignments

Select **Arduino Nano ESP32** and **Tools > Pin Numbering > By Arduino pin (default)**. Numeric pin identifiers in the sketch refer to Arduino pin labels in this mode, rather than ESP32 GPIO numbers.

| Component signal | Sketch pin or setting |
| --- | --- |
| MFRC522 SS / SDA | `10` (SPI chip select, not I2C SDA) |
| MFRC522 RST | `9` |
| MFRC522 SCK, MOSI, MISO | `D13`, `D11`, `D12`, the Nano defaults from `SPI.begin()` |
| Keypad rows, in order | `A1`, `A0`, `8`, `7` |
| Keypad columns, in order | `6`, `5`, `4`, `2` |
| Buzzer control | `3` |
| LCD | I2C address `0x27`, 20 columns, 4 rows |
| LCD and DS1307 bus | `A4` SDA, `A5` SCL |
| Serial console | `9600` baud |

The original board was identified as an Arduino Nano ESP32, which provides native USB and BLE. Both keyboard interfaces are initialized at startup. The [hardware guide](docs/hardware.md) maps Arduino labels to GPIO numbers and documents the remaining module and power assumptions.

Check pin conflicts, board voltage levels, and module pull-ups before connecting hardware. The existing pin assignments should not be copied blindly to a different board.

## Dependencies

The sketch uses the Arduino framework and these libraries. Versions below were found in the local development environment during repository preparation; they are not a guarantee of a compatible build combination.

| Library | Local version | Purpose |
| --- | --- | --- |
| [MFRC522](https://github.com/miguelbalboa/rfid) | 1.4.12 | RFID reader over SPI |
| Keypad | 3.1.1 | Matrix keypad scanning |
| [LiquidCrystal I2C](https://github.com/marcoschwartz/LiquidCrystal_I2C) | 1.1.2 | LCD output |
| [RTClib](https://github.com/adafruit/RTClib) | 2.1.4 | DS1307 timekeeping |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) | 1.17.4 | RTClib dependency |
| [HijelHID_BLEKeyboard](https://github.com/HijelHub/HijelHID_BLEKeyboard) | 0.5.0 | Bluetooth keyboard output |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | 2.5.0 | BLE stack dependency |
| [AESLib](https://github.com/suculent/thinx-aes-lib) | 2.3.6 | UID and PIN decryption |

`SPI`, `Wire`, `USB`, and `USBHIDKeyboard` come from the selected board package. Use the linked AESLib implementation, since other libraries share that name. The installed HijelHID library specifies ESP32 Arduino core 3.3.7 or newer and NimBLE-Arduino 2.3.8 or newer. See [validation notes](docs/validation.md) for the build status.

## Configuration and use

1. Open `firmware/esp32_authenticator/esp32_authenticator.ino` in Arduino IDE.
2. Select Arduino Nano ESP32, set Pin Numbering to By Arduino pin (default), and install compatible dependencies.
3. Review the pin mapping against the hardware.
4. The sketch defaults to `credentials.example.h`, containing public demonstration profiles, UID `04 A1 B2 C3`, and PIN `123456`. An arbitrary card will not match that UID.
5. To customize the configuration, copy that header to `credentials.local.h` in the same folder. The local header takes precedence and is excluded from Git. Keep experiments limited to disposable demonstration accounts.
6. The UID and PIN ciphertexts are 16-byte AES-128-CBC blocks. The example uses zero-padded ASCII, a public key, and a public IV. If either value changes, regenerate its ciphertext with the matching key and IV. Format the UID as uppercase hex bytes separated by spaces.
7. Compile before uploading. Then use the serial console at 9600 baud to inspect events. If the RTC is not running, setup initializes it from the firmware's compile date and time.
8. Test keyboard output in a disposable text field before attempting any login form. The firmware sends keystrokes to whichever application has focus and cannot verify login success.

### Controls

| Screen | Key | Action |
| --- | --- | --- |
| Main loop / idle | Hold `A` for two seconds | Toggle USB/Bluetooth output |
| PIN entry | `#` | Submit |
| PIN entry | `*` | Delete last character |
| Profile selection | `4` / `6` | Previous / next profile |
| Profile selection | `#` | Select and type |
| Profile selection | `*` | Cancel |

The PIN routine accepts other keypad characters, including letter keys, up to six characters. It is not restricted to digits.

## Prototype limitations

This is a credential-entry prototype, not a FIDO2/WebAuthn authenticator or a production password vault.

- Card verification compares a UID. It does not perform cryptographic card authentication or establish replay resistance.
- The AES key is stored with the firmware. UID/PIN encryption does not protect against firmware extraction, and profile passwords are stored as plaintext strings in firmware.
- Lockout state is held in RAM and resets on reboot. The code does not provide persistent brute-force protection.
- PIN entry and profile selection block the main loop. There is no input timeout, and mode switching is not serviced inside those screens.
- The code prints card UIDs to Serial. It does not persist an audit log.
- Startup status messages are not comprehensive peripheral health checks. The sketch does not check every initialization result or verify successful keyboard delivery.
- The LCD and DS1307 were powered from `VIN` while the ESP32-S3 drives I2C at 3.3V, with no level shifter and no external pull-ups. If those modules' onboard pull-ups tie to their own supply, the bus idles above 3.3V on a part that is not 5V-tolerant. The build worked, but this is a known electrical risk rather than a validated design, and the actual `VIN` voltage was never measured.
- Secure boot was deliberately not attempted: enabling it burns eFuses irreversibly, which was judged too risky for a prototype board. This repository contains no secure-boot provisioning, firmware-signing configuration, flash-encryption configuration, or documented attack-test results.

These are design limitations of the prototype, not open defects. See [validation notes](docs/validation.md) for what was verified on hardware, the compile record, and a re-test checklist for anyone rebuilding it.
