# Validation notes

## Scope

Repository preparation included source review, removal of personal configuration from the published copy, and review of locally installed library APIs. The original local checkpoint remains outside version control.

The published copy uses ASCII Tab and newline writes for both keyboard transports. This avoids passing raw HID usage IDs to the BLE library's ASCII `write()` API and avoids relying on conflicting USB/BLE key macros. The profile count is calculated from the configured array.

## Build status

A local Arduino CLI compile completed successfully on September 10, 2026 with the demonstration configuration:

- Target: `arduino:esp32:nano_nora` (Arduino Nano ESP32).
- Board package: `arduino:esp32` version `2.0.18-arduino.5`.
- Library versions: listed in the README.
- Program storage: 816,593 bytes of 3,145,728 bytes (25%).
- Static/global RAM: 49,152 bytes of 327,680 bytes (15%). This is not a measurement of peak runtime RAM use.

The target is a compilation check, not evidence of the original board model. The installed HijelHID library advertises a newer upstream core requirement even though this local combination compiled. Compatibility with other core versions and runtime behavior remains to be checked.

The compiler reported that LiquidCrystal I2C declares AVR compatibility while this target uses ESP32. The build succeeded, but LCD operation still requires a hardware check.

Reproduction command with Arduino CLI on PATH and the dependencies installed in the Arduino sketchbook:

```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora firmware/esp32_authenticator
```

The public UID and PIN ciphertexts were independently decrypted and verified against their documented demonstration values. The publication files were also checked against the original local account values and AES material; no original values were included.

No firmware has been uploaded during this review. No physical board, wiring, USB login sequence, or BLE pairing behavior has been tested during repository preparation.

## Manual hardware checks

These are planned checks, not completed test results.

| Check | Expected behavior |
| --- | --- |
| Boot | LCD reaches the idle screen and Serial prints a timestamped boot event |
| Unrecognized card | Denial message and failure buzzer pattern; PIN count does not increase |
| Recognized card | PIN entry screen appears |
| PIN editing | Typed characters are masked; `*` removes the last character; `#` submits |
| Wrong PIN once or twice | Failure count increases and access is denied |
| Third wrong PIN | Lockout begins and the display shows remaining time |
| Lockout expiry | After the RTC-based interval, failure count clears and scanning resumes |
| Correct PIN | Profile menu opens |
| Profile navigation | `4` and `6` wrap through configured profiles |
| Cancel profile | Returns to idle without typing credentials |
| USB profile output | Demo username, Tab, demo password, Enter reach a disposable test field |
| BLE profile output | After pairing, the same sequence reaches a disposable test field |
| Mode toggle | Holding `A` while the main loop runs changes the selected transport |
| Reboot during lockout | RAM state resets; this demonstrates a known limitation |

Confirm that no credentials are entered into an unintended window. The firmware cannot identify the focused application or validate a server response.
