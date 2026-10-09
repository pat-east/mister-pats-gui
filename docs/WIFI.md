# Wi-Fi configuration — implementation note

This records the MiSTer-specific configuration path for a later Wi-Fi settings feature.
The GUI does not currently edit Wi-Fi settings.

1. Generate a network block with `wpa_passphrase 'MYSSID' 'PASSPHRASE'`.
2. Remove the generated `#psk="PASSPHRASE"` comment, which contains the password in plain
   text. Keep the derived `psk=` value and the SSID.
3. Set the appropriate Wi-Fi country/region in the configuration (`country=XX`, using the
   actual two-letter country code).
4. Save the configuration at **`/media/fat/linux/wpa_supplicant.conf`**. A file placed under
   `/etc/` is not used for this MiSTer setup.

For the future Settings flow: generate the derived key without retaining or logging the
plain-text password, include the region, and write the configuration to the path above.
The command shown in step 1 is illustrative; entering the password as a shell argument can
expose it in shell history or process listings.
