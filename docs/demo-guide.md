# Demonstration guide

This guide describes a repeatable demonstration using disposable profile values. It is a procedure to run on hardware, not a record of a completed demonstration.

## Preparation

Use the verified wiring for the actual board, compile the sketch, and configure a known test card and PIN. The public demonstration UID will not match an arbitrary card. Open Serial at 9600 baud and prepare a disposable two-field local form that contains no real account information.

The output sequence includes Tab and Enter, so a plain text editor may show those characters differently from a login form. Check where focus will move and what Enter will do before selecting a profile.

## Suggested demonstration sequence

1. Show the component layout and the idle LCD screen.
2. Scan a card that is not configured. Show the denial message and buzzer feedback.
3. Scan the accepted card. Enter a wrong PIN and submit with `#`.
4. Repeat accepted-card scans with wrong PINs until the third failure. Show the lockout countdown and wait for expiry.
5. Scan the accepted card again and enter the correct PIN. Use `4` and `6` to browse profiles, then `*` to cancel without typing.
6. Authenticate again. Focus the disposable host form and select a demonstration profile with `#`. Observe the username, Tab, password, and Enter sequence.
7. While idle, hold `A` for two seconds to change output transport. Pair the host if using BLE, then repeat the output check.

Capture actual observed results in the validation record. A successful credential sequence demonstrates local gating and keyboard output; it does not establish that a remote service accepted a login.

## Expected serial event types

The code emits these messages with RTC timestamps. The list below shows message formats, not a captured run.

```text
SYSTEM BOOT
MODE: Bluetooth
MODE: USB
CARD SCANNED - UID: <scanned UID>
ACCESS DENIED - <reason>
LOCKOUT TRIGGERED
LOCKOUT EXPIRED
ACCESS GRANTED - Profile: <profile name>
```

Do not publish a serial capture without reviewing card UIDs and profile names. The public demo configuration contains disposable values, but local configurations may not.

## What to explain during the demo

- How the RFID reader, keypad, LCD, and RTC communicate with the controller.
- Where card comparison, PIN comparison, attempt counting, and output happen in the code.
- Why the firmware's AES usage does not protect against extraction of the compiled key.
- Why rebooting clears lockout, and what a persistent design would need to do differently.
- Why the host focus and transport connection matter when emulating a keyboard.

See [architecture](architecture.md) for the state and sequence diagrams, and [hardware](hardware.md) for the connection reference.
