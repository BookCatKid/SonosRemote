# SonosRemote Project Ideas

This is a list of potential improvements and features for the SonosRemote project.

## UI & Visual Polish
*   **Dynamic Theming:** Extract the dominant or average color from album art to use as a UI accent color.
*   **Smooth Progress Bar:** Implement local interpolation to move the progress bar every second between network refreshes.
*   **Marquee Scrolling:** Add horizontal scrolling for song titles or artists that are too long for the screen.
*   **Screen Dimming/Sleep:** Auto-dim or turn off the display after inactivity to save power.
*   **Animations:** Add simple transitions between the Speaker List and Now Playing screens.

## Advanced Sonos Features
*   **Favorites & Playlists:** A dedicated screen to browse and trigger Sonos "My Favorites".
*   **Grouping Control:** UI to join/leave speaker groups.
*   **Queue Management:** View and potentially skip items in the current playback queue.
*   **Stereo Pair Support:** Better visualization for grouped or paired speakers.

## Portability & Reliability
*   **Battery Management:** Display battery percentage and implement deep sleep (utilizing the XIAO ESP32-S3's built-in charger).
*   **WiFi Manager:** On-device portal to configure WiFi credentials without re-flashing.
*   **OTA Updates:** Over-the-air firmware updates via WiFi.
*   **Error Handling:** Better visual feedback for network timeouts or Sonos errors.

## Hardware Upgrades
*   **Rotary Encoder:** Use a physical dial for volume or list scrolling.
*   **Haptic Feedback:** Tiny vibration motor for button press confirmation.
*   **Light Sensor:** Automatic brightness adjustment based on room light.
*   **Custom PCB & Enclosure:** Moving from breadboard to a finished product.
