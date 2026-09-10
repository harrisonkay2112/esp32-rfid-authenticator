# Firmware architecture

The sketch uses one Arduino `loop()` and blocking helper functions. The states below describe its behavior; the implementation does not contain an explicit state enum, task scheduler, or nonblocking state-machine framework.

## States and transitions

```mermaid
stateDiagram-v2
    [*] --> Initializing
    Initializing --> AwaitingCard: setup completes
    AwaitingCard --> AwaitingCard: no readable card
    AwaitingCard --> Denied: UID does not match
    Denied --> AwaitingCard: message and buzzer finish
    AwaitingCard --> EnteringPIN: UID matches
    EnteringPIN --> EnteringPIN: append or delete character
    EnteringPIN --> ChoosingProfile: submit matching PIN
    EnteringPIN --> Denied: wrong PIN and count below 3
    EnteringPIN --> LockedOut: wrong PIN brings count to 3
    LockedOut --> LockedOut: fewer than 30 RTC seconds elapsed
    LockedOut --> AwaitingCard: clear lockout and failure count
    ChoosingProfile --> ChoosingProfile: previous or next profile
    ChoosingProfile --> AwaitingCard: cancel
    ChoosingProfile --> SendingCredentials: select and clear failure count
    SendingCredentials --> AwaitingCard: output sequence finishes
```

On the third wrong PIN, the firmware sets `lockedOut`, records `lockoutTime`, and displays a denial message before the main loop starts displaying the countdown. No further cards are read while `lockedOut` is true. Restarting the device returns to initialization and clears the RAM-held lockout state.

USB/Bluetooth selection is a separate flag, not an authentication state. `checkModeToggle()` runs before the lockout check, so it can also run between lockout updates. It is not serviced while `getPIN()`, `selectProfile()`, or credential output is blocking.

## State to code map

All functions below are in [esp32_authenticator.ino](../firmware/esp32_authenticator/esp32_authenticator.ino).

| Behavior | Function or code location | Stored state |
| --- | --- | --- |
| Initialize peripherals and decrypt configuration | `setup()`, `decryptCredential()` | `validUID`, `validPIN` |
| Poll reader and compare UID | `loop()`, `getCardUID()` | Reader object and formatted UID |
| Collect masked PIN | `getPIN()` | Local `String pin` |
| Count wrong PINs | PIN comparison in `loop()` | `failedAttempts` |
| Enforce timed lockout | `checkLockout()` | `lockedOut`, `lockoutTime` |
| Browse profiles | `selectProfile()` | Local profile index |
| Type selected credentials | `typeCredentials()` and output helpers | Selected profile and `bluetoothMode` |
| Toggle output transport | `checkModeToggle()` | `aKeyHeld`, `aKeyHoldStart`, `bluetoothMode` |
| Show status and log events | LCD helpers, `logEvent()` | RTC time read when each event is logged |

`failedAttempts` counts incorrect PIN submissions after an accepted card. Invalid cards do not change it. A correct PIN followed by cancellation leaves the existing count unchanged; selecting a profile resets it before output begins.

## Successful interaction

```mermaid
sequenceDiagram
    actor User
    participant Reader as RFID reader
    participant Firmware
    participant UI as Keypad and LCD
    participant Host as Focused host application
    User->>Reader: Present card
    Firmware->>Reader: Poll and read UID
    Reader-->>Firmware: UID bytes
    Firmware->>Firmware: Format and compare UID
    Firmware->>UI: Prompt for PIN
    User->>UI: Enter PIN and press the submit key
    UI-->>Firmware: Submitted characters
    Firmware->>Firmware: Compare PIN
    Firmware->>UI: Display profiles
    User->>UI: Select profile with the submit key
    UI-->>Firmware: Profile index
    Firmware->>Firmware: Clear failure count
    Firmware->>UI: Display access granted and beep
    Firmware->>Host: Username via selected HID transport
    Firmware->>Host: Tab
    Firmware->>Host: Password
    Firmware->>Host: Enter
    Note over Firmware,Host: No server response is read or checked
    Firmware->>UI: Return to idle display
```

The firmware waits one second after its success feedback/logging before typing. It inserts 300 ms delays between username, Tab, password, and Enter, then waits two seconds before returning to idle. Bluetooth text output also includes a 50 ms delay per character, plus any timing inside the BLE library. These are configured delays, not measured end-to-end latency.

## Configuration and data flow

```mermaid
flowchart LR
    Config[Local header or public demo header] --> AES[AES startup decryption]
    AES --> Expected[Expected UID and PIN in RAM]
    Reader[Scanned UID] --> Compare[Card and PIN checks]
    Keypad[Submitted PIN] --> Compare
    Expected --> Compare
    Compare --> Menu[Profile selection]
    Config --> Profiles[Profile strings in firmware]
    Profiles --> Menu
    Menu --> Output[USB or BLE keyboard output]
    Output --> Host[Focused host application]
```

The two encrypted blocks cover the expected UID and PIN only. Profile usernames and passwords are plaintext strings. A local configuration header prevents accidental Git publication of that file; it does not protect data in the compiled firmware.

## Implementation choices and tradeoffs

These are consequences of the code structure, not a reconstructed account of the original design process.

| Choice in the code | Benefit | Tradeoff |
| --- | --- | --- |
| Card check followed by PIN entry | Separates the two user interactions | UID matching is not cryptographic card authentication |
| Blocking input helpers | Keeps each screen's logic together | No timeout or background mode handling inside those helpers |
| RTC-based lockout | Uses seconds from the same clock as serial timestamps | RTC changes affect elapsed time; lockout flags still disappear on reset |
| HID credential entry | Sends input through a keyboard interface | Depends on window focus, field order, pairing, and host keyboard layout |
| Transport helper functions | Shares the profile/login sequence between USB and BLE | Both transports are initialized; selecting one does not disable the other |
| Compiled configuration | Simple startup path without filesystem provisioning | Updating profiles requires rebuilding; secrets remain in firmware |

## Follow-up work

Future changes could introduce an explicit nonblocking state machine, input timeouts, persistent attempt tracking, checked peripheral initialization, and a host/transport readiness check before output. Secure boot, flash encryption, and stronger card authentication would require their own implementation and validation. None are claimed as completed features.
