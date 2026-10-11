# Wi-Fi settings — design and MiSTer notes

Settings → System → Info shows Ethernet and Wi-Fi status in a modal. The Wi-Fi IP and network
name are adjacent under **Networking**. The controller-operated setup flow has been tested
successfully on MiSTer hardware.

The MiSTer image includes the `wpa_supplicant` tools. Use `wpa_cli` for status, scans, and
runtime network control, and `wpa_passphrase` to derive a PSK from a password supplied through
stdin. Never pass a password as a process argument. Keep the existing supplicant in control of
the wireless interface rather than starting a second `wpa_supplicant` instance.

## Connection flow

Under **Settings → System → Info**, the system facts list the Ethernet address, Wi-Fi IP,
and Wi-Fi network name (SSID), with the two Wi-Fi values adjacent. Provide **Connect to Wi-Fi…**
to start setup:

1. Scan for networks, or choose **Connect to hidden Wi-Fi access point**.
2. Select a discovered network, or enter the hidden network's SSID with the shared on-screen
   keyboard.
3. Enter the password with that keyboard. It is masked initially; select **Show** to reveal it
   temporarily or **Hide** to mask it again. Press **Start** to confirm the password regardless
   of which keyboard key is selected.
4. Run a connection test and show whether it connected successfully.
5. After a successful test, show a final review. On confirmation, ask for a country code only if
   neither the existing file nor the active regulatory domain provides one, then save to
   `/media/fat/linux/wpa_supplicant.conf`.

First detect whether a wireless interface exists. Some MiSTer setups have no Wi-Fi adapter;
show **No Wi-Fi adapter** on the setup row and disable it in that case. For a present interface,
use its discovered name (such as `wlan0`) instead of assuming one fixed name.

The Wi-Fi flow reuses the controller-operated keyboard from Search. It has letter and symbol
layouts, a separate numeric keypad, Shift, Space, Delete, Clear, and Done. For a hidden SSID,
**Done** accepts the entered text. Passwords start masked, with an on-screen **Show/Hide** key;
**Start** confirms the password regardless of the selected keyboard key. **B** goes back,
**Y** inserts a space, and **X** deletes the last character.

## ASCII mockups

### Settings → System

```text
┌──────────────────────────── Settings / System ────────────────────────────┐
│  > Info                Time format     24-hour     WHAT THIS DOES         │
│  Time format           Time zone       Europe/Berlin  View system details │
│  Time zone             Download Assets              about this MiSTer.    │
│  …                     Connect to Wi-Fi…                                  │
└───────────────────────────────────────────────────────────────────────────┘

Selecting **Info** opens a modal with the live system facts:

```text
┌────────────────────────── System information ─────────────────────────────┐
│  Networking                                                               │
│  Ethernet       192.168.1.10                                               │
│  Wi-Fi          192.168.1.42                                               │
│  Wi-Fi network  MisterNet                                                  │
│                                                                  B Close  │
└───────────────────────────────────────────────────────────────────────────┘
```
```

Without a Wi-Fi adapter, the network name reports **No Wi-Fi adapter**. With an adapter but no
connection it reports **Not connected**; the IP row reports the current address or **Offline**.

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

The scan view shows progress without duplicating the actions in the modal footer. The network
list uses the rounded rows and left focus marker from **Settings → Categories**. Rows show the
SSID and security/signal details, followed by actions for a hidden network and another scan. An
empty result explains that no networks were found.

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
│  D-pad Move  A Select key  B Back  X Delete  Y Space                      │
└───────────────────────────────────────────────────────────────────────────┘
```

The letter and symbol keys stay on the left; **Symbols** opens extended punctuation. A dedicated
3-by-4 numeric keypad stays on the right. The password uses the same keyboard, with a separate
input field above the keyboard panel and a **Show/Hide** key. Press **Start** to submit it.

### Enter the password

```text
┌──────────────────────────── Wi-Fi password ─────────────────────────────┐
│  Network: MisterNet — enter password                                    │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ *****************_                                                   │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
│                                                                           │
│  ┌──────────────────── Keyboard panel ────────────────────────────────┐  │
│  │ [Letter and symbol keys]                    [Numeric keypad]        │  │
│  │ Symbols     Space      Delete     Clear     Show       Done         │  │
│  └─────────────────────────────────────────────────────────────────────┘  │
│                                                                           │
│  D-pad Move  A Select key  Start Confirm  Show/Hide  B Back              │
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

The configuration file is not written before the user confirms this last screen. If the test is
cancelled or fails, the previous connection is restored. The failure screen offers retry or
cancellation.

## Configuration path and secret handling

The Wi-Fi configuration flow:

1. Generate the network block with `wpa_passphrase`, feeding the password through stdin. The
   installed MiSTer tool accepts stdin; never pass a real password as an argument.
2. Remove the generated `#psk="PASSPHRASE"` comment, which contains the password in plain
   text. Keep the derived `psk=` value and the SSID.
3. Preserve an existing `country=` setting. If none exists, use the active regulatory country
   from `iw reg get`; if that is unavailable, ask for the two-letter country code. A time zone
   is not a reliable substitute for this country code.
4. Save the configuration at **`/media/fat/linux/wpa_supplicant.conf`**. A file placed under
   `/etc/` is not used for this MiSTer setup.

Never put a password in a shell command argument, debug log, notification, or unmasked UI
label. The GUI sends it through stdin to `wpa_passphrase`, removes the generated plain-text
comment, clears the keyboard buffer, and stores only the derived PSK. The configuration is
written atomically with private file permissions, preserving existing contents and appending
the tested network. Restart MiSTer to load the saved profile automatically.

## MiSTer tool findings and hardware test

On the reference MiSTer, `wpa_cli`, `wpa_passphrase`, and `wpa_supplicant` are installed, the
interface is `wlan0`, and `dhcpcd` manages DHCP. `wpa_cli -i wlan0 status` reports the current
SSID and IP; `scan` followed by `scan_results` reports SSID, security flags, and signal level.
`wpa_passphrase <ssid>` reads the passphrase from stdin and emits a derived PSK. The interactive
`wpa_cli -i wlan0` accepts commands over stdin, so secret derived keys need not appear in process
arguments. The current kernel regulatory domain reports `DE`; the saved supplicant config had no
`country=` line. The implementation uses a saved country when present, otherwise the live
regulatory domain, and asks only when neither supplies a valid code.

The GUI detects Wi-Fi interfaces from the `wl` name prefix, matching the existing address and
top-bar detection. Without one, it shows **No Wi-Fi adapter** and disables setup instead of
guessing an interface name. The complete Wi-Fi setup was tested successfully on MiSTer hardware.
