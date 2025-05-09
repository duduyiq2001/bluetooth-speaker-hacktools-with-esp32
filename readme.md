# Bluetooth Speaker Exploits
![Security](https://img.shields.io/badge/category-security-red)
![Bluetooth](https://img.shields.io/badge/category-bluetooth-blue)
This project provides a collection of exploits targeting Bluetooth speakers using esp32-wroom-32. It is intended for educational and research purposes only. Please use responsibly and ensure compliance with applicable laws and regulations.

## Features
- find_av_devices.c discovers nearby bt devices and filter them down to audio/video class only, logging the mac address of the target
- mac_addr_spoof.c you can use the mac address you obtained from the previous script to impersonate audio/video devices. Callback functions are implemented at the VHCI level, l2cap level as well as GAP level to log connection related events and to save packets if it captures any.

## Requirements
- Python ^3.9 
- Bluetooth-enabled device
- ESP32-WROOM MCU (exploits are launched from this microcontroller)
- Expressif IDF toolchain 

## Installation
1. install expressidf , following this guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html
2. Clone the repository:
    ```bash
    git clone https://github.com/yourusername/speaker_exploit.git
    cd speaker_exploit
    ```
    

## Usage
1. Ensure your Bluetooth audio/vuideo target device is discoverable.
2. compile with idf.py build, make sure to edit CMakeLists to configure which scripts you want to run.
3. connect esp32 board to your laptop 
4. flash the firmware 
5. idf.py -p PORT monitor to check the eps32 logs


## Disclaimer
This project is for educational purposes only. The authors are not responsible for any misuse or damage caused by the use of this software.

## Contributing
Contributions are welcome! Please submit a pull request or open an issue for discussion.

## License
This project is licensed under the [MIT License](LICENSE).