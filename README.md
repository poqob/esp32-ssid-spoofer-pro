# ESP32 SSID Spoofer Pro

A web-controlled SSID beacon flooding tool for ESP32, built with ESP-IDF framework.

## Features

- **Web-based Control Panel** - No app needed, works from any browser
- **WiFi Network Scanner** - Scan and clone nearby SSIDs
- **Custom SSID Support** - Add your own SSIDs to broadcast
- **Real-time Status** - Monitor active spoofing attacks
- **WPA2 Protected AP** - Secure control interface with password

## Hardware Requirements

- ESP32 Development Board (ESP32-WROOM-32, ESP32-WROVER, etc.)
- USB cable for programming
- 4MB flash recommended

## Software Requirements

- PlatformIO IDE (VS Code extension) or PlatformIO CLI
- Python 3.8+

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/esp32-ssid-spoofer-pro.git
cd esp32-ssid-spoofer-pro
```

### 2. Install PlatformIO

```bash
# Install PlatformIO Core
pip install platformio

# Or use VS Code with PlatformIO IDE extension
```

### 3. Build and Flash

```bash
# Connect ESP32 via USB
# Build and upload
pio run -t upload

# Optional: Monitor serial output
pio device monitor
```

## Usage

### 1. Connect to ESP32

- **SSID**: `saturn`
- **Password**: `kova3210`

### 2. Open Control Panel

Open your browser and navigate to: **http://192.168.4.1**

### 3. Control Panel Features

- **SCAN** - Scan for nearby WiFi networks
- **+ Button** - Add scanned network to spoof list
- **Custom SSID** - Enter custom SSID name and click ADD
- **- Button** - Remove SSID from list
- **START** - Begin beacon flooding
- **STOP** - Stop beacon flooding

## Technical Details

### WiFi Configuration

- **AP Mode**: SoftAP + Station (APSTA) for scanning capability
- **Channel**: Auto-selected based on target networks
- **Security**: WPA2-PSK

### Beacon Frame Structure

Each beacon frame includes:
- Frame Control (Management, Beacon subtype)
- Destination MAC (Broadcast: FF:FF:FF:FF:FF:FF)
- Source MAC (Unique per SSID: DE:AD:BE:EF:XX:XX)
- BSSID (Same as Source MAC)
- Timestamp, Beacon Interval, Capabilities
- SSID Tag (Variable length)
- Supported Rates
- DS Parameter Set (Channel)
- ERP Information
- Extended Supported Rates

### Frame Injection

Uses `esp_wifi_80211_tx()` for raw 802.11 frame injection with:
- DMA-safe heap-allocated buffers (malloc)
- Burst mode (3 frames per SSID)
- 10ms interval between SSIDs

## Project Structure

```
esp32-ssid-spoofer-pro/
├── platformio.ini          # PlatformIO configuration
├── CMakeLists.txt          # ESP-IDF project config
├── src/
│   ├── CMakeLists.txt      # Component registration
│   └── main.c              # Main application code
└── README.md               # This file
```

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Control panel HTML |
| `/api/scan` | GET | Scan nearby networks |
| `/api/select` | GET | Get selected SSIDs |
| `/api/select` | POST | Add SSID to list |
| `/api/select/delete` | DELETE | Remove SSID from list |
| `/api/spoof/start` | POST | Start beacon flooding |
| `/api/spoof/stop` | POST | Stop beacon flooding |
| `/api/status` | GET | Get current status |

## Configuration

Edit `src/main.c` to customize:

```c
#define AP_SSID     "saturn"          // Control AP SSID
#define AP_PASS     "kova3210"        // Control AP password
#define AP_CHANNEL  6                 // Default channel
#define MAX_SSID    20                // Max SSIDs in list
#define BEACON_BURST 3                // Frames per SSID per cycle
#define BEACON_INTERVAL_MS 10         // Interval between SSIDs
```

## Safety & Legal Disclaimer

This tool is intended for:
- Educational purposes
- Security research
- Authorized penetration testing
- Cybersecurity coursework

**DO NOT** use this tool to:
- Disrupt networks you don't own
- Interfere with critical infrastructure
- Violate local laws and regulations

Always obtain proper authorization before testing any network.

## Troubleshooting

### ESP32 Not Detected

```bash
# Check USB connection
ls /dev/ttyUSB*  # Linux
# or
ls /dev/cu.usbserial*  # macOS

# Install USB drivers if needed (CP210x, CH340, etc.)
```

### Build Errors

```bash
# Clean build environment
pio run --target clean

# Rebuild
pio run
```

### Scan Returns Empty

- Ensure ESP32 has antenna connected
- Move to location with WiFi networks
- Check if WiFi is enabled in your area

### Web Page Not Loading

- Ensure you're connected to "saturn" WiFi
- Try http://192.168.4.1 (not https)
- Clear browser cache and reload

## License

MIT License - See LICENSE file for details

## Credits

Built with ESP-IDF 4.4.5 and PlatformIO.

## Support

For issues and feature requests, please open an issue on GitHub.
