# Wi-Fi settings — design and MiSTer notes

The GUI does not currently edit Wi-Fi settings. This document records the proposed
controller-driven flow, first-pass ASCII mockups, and the MiSTer-specific configuration
details that implementation must follow.

## Proposed flow

Under **Settings → System**, show the current Wi-Fi connection when one is active, including
the network name (SSID) and IP address. Provide **Connect to Wi-Fi…** to start setup:

1. Scan for networks, or choose **Connect to hidden Wi-Fi access point**.
2. Select a discovered network, or enter the hidden network's SSID with the shared on-screen
   keyboard.
3. Enter the password with that keyboard. Mask it while typing.
4. Run a connection test and show whether it connected successfully.
5. After a successful test, show a final review. Only after the user confirms should the GUI
   write `/media/fat/linux/wpa_supplicant.conf`.

The reusable, controller-operated keyboard is already implemented for Search and can be shared
with the Wi-Fi flow for both SSIDs and passwords. It has letter and symbol layouts, a separate
numeric keypad, a Shift key, Space, Delete, Clear, and Done. While open, **B** closes it, **Y**
inserts a space, and **X** deletes the last character; the on-screen **Symbols** key switches
layouts and **Shift** toggles letter case. Wi-Fi should use **Done** to accept the entered text,
and mask password characters while typing.

## ASCII mockups

### Settings → System

```text
┌──────────────────────────── Settings / System ────────────────────────────┐
│                                                                           │
│  Wi-Fi                                                                    │
│  Connected to  MisterNet                                                  │
│  IP address    192.168.1.42                                               │
│                                                                           │
│  > Connect to Wi-Fi…                                                      │
│                                                                           │
│  A  Open                         B  Back                                 │
└───────────────────────────────────────────────────────────────────────────┘
```

When disconnected, show **Not connected** in place of the SSID and IP details.

### Choose a network

```text
┌──────────────────────────── Connect to Wi-Fi ────────────────────────────┐
│  Select a network                                                        │
│                                                                           │
│  > MisterNet                 WPA2                 ▂▄▆█                    │
│    Workshop                  WPA2                 ▂▄▆                     │
│    Guest                     Open                 ▂▄                      │
│    Connect to hidden Wi-Fi access point                                │
│                                                                           │
│  A  Select                       B  Cancel                               │
└───────────────────────────────────────────────────────────────────────────┘
```

The scan view should indicate that a scan is in progress, allow a rescan, and provide a
useful empty state when no access points are found. Signal and security labels are shown when
the MiSTer scan tools provide them.

### Enter a hidden network name

```text
┌────────────────────────── Hidden Wi-Fi network ──────────────────────────┐
│  Network name (SSID)                                                     │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ My hidden network_                                                  │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                                                           │
│  ┌─────────────────────────────────────────────────┬─────────────────┐   │
│  │ !   @   #   $   %   ^   &   *   (   )   -   =    │  7    8    9    │   │
│  │ q   w   e   r   t   y   u   i   o   p   [   ]    │  4    5    6    │   │
│  │  a   s   d   f   g   h   j   k   l   ;   '   \  │  1    2    3    │   │
│  │ Shift   `   z   x   c   v   b   n   m   ,   .   /  │  .    0    -    │   │
│  │ Symbols      Space          Clear      Done       │                 │   │
│  └─────────────────────────────────────────────────┴─────────────────┘   │
│                                                                           │
│  D-pad Move  A Activate  B Close keyboard  X Delete  Y Space             │
└───────────────────────────────────────────────────────────────────────────┘
```

The letter and symbol keys stay on the left; the top row shows the common shifted number-row
symbols, and **Symbols** opens the extended punctuation set. A dedicated 3-by-4 numeric keypad
stays on the right. Both areas are part of one reusable keyboard and share the same
focus/navigation model.
The same layout is used for the password. The password field masks entered characters and
offers a deliberate reveal toggle if that proves useful during hardware testing. The keyboard
must support the characters accepted by the MiSTer configuration path, including punctuation
and symbols commonly used in passphrases.

### Enter the password

```text
┌──────────────────────────── Wi-Fi password ─────────────────────────────┐
│  Network: MisterNet                                                      │
│  Password                                                                │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ •••••••••••••••••_                                                   │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                                                           │
│  [Letter and symbol keys]                          [Numeric keypad]        │
│                                                                           │
│  D-pad Move  A Activate  B Close keyboard  X Delete  Y Space             │
└───────────────────────────────────────────────────────────────────────────┘
```

### Test and confirm

```text
┌──────────────────────────── Testing connection ──────────────────────────┐
│  Network: MisterNet                                                      │
│                                                                           │
│  Connecting…                                                             │
│  Checking for an IP address…                                             │
│                                                                           │
│  Please wait                                                             │
└───────────────────────────────────────────────────────────────────────────┘
```

```text
┌────────────────────── Connection test successful ───────────────────────┐
│  Connected to MisterNet                                                  │
│  IP address: 192.168.1.42                                                │
│                                                                           │
│  Save this network to MiSTer?                                            │
│  This writes /media/fat/linux/wpa_supplicant.conf.                       │
│                                                                           │
│  A  Save Wi-Fi settings          B  Go back                              │
└───────────────────────────────────────────────────────────────────────────┘
```

The configuration file is not written as the final file before the user confirms this last
screen. A failed test should show a clear error and offer retry or return to password entry.

## Configuration path and secret handling

MiSTer's timezone setup script uses `/usr/share/zoneinfo/posix` for its system data. Wi-Fi
configuration is separate:

1. Generate the network block with `wpa_passphrase`, or derive the PSK in memory.
2. Remove the generated `#psk="PASSPHRASE"` comment, which contains the password in plain
   text. Keep the derived `psk=` value and the SSID.
3. Set the appropriate Wi-Fi country/region in the configuration (`country=XX`, using the
   actual two-letter regulatory country code). A time zone is not a reliable substitute for
   this country code.
4. Save the configuration at **`/media/fat/linux/wpa_supplicant.conf`**. A file placed under
   `/etc/` is not used for this MiSTer setup.

Never put a password in a shell command argument, debug log, notification, or unmasked UI
label. The `wpa_passphrase` command is useful as documentation, but passing a real password as
its argument can expose it in process listings. Build the derived key without retaining or
logging the plain-text password longer than necessary. Write the final file atomically so an
interrupted write cannot leave a truncated configuration.

## Implementation questions to resolve

The existing notes establish the persistent file path and network-block format, but they do
not establish how MiSTer applies a changed configuration while already running. Before
implementing the test step, identify the installed scan/connect tools and service behavior on
the target image. In particular:

- Which command/API reports scan results, SSID, security mode, and signal strength?
- How can a candidate network be tested without first replacing the persistent configuration?
- Does that test interrupt an existing Wi-Fi connection, and can the old connection be
  restored on failure or cancellation?
- How is the regulatory country selected or discovered when no country is already configured?
- After final confirmation, does MiSTer apply the new file immediately, or does it take effect
  on a later service restart or reboot?
- If a `wpa_supplicant.conf` already exists, should saving replace it or preserve unrelated
  network blocks and global settings?

Do not assume that writing the file alone connects the running system. Confirm the behavior on
MiSTer hardware before presenting the final save as an active connection.
