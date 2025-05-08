# Bluetooth Speaker Exploits -- A2DP DDoS
![Security](https://img.shields.io/badge/category-security-red)
![Bluetooth](https://img.shields.io/badge/category-bluetooth-blue)

This project provides a collection of exploits targeting Bluetooth speakers using esp32-wroom-32. It is intended for educational and research purposes only. Please use responsibly and ensure compliance with applicable laws and regulations.

## Features
dos_attack.c implements an A2DP flooding attack:
- Continuously attempts to connect to a hardcoded Bluetooth speaker MAC address.
- Immediately disconnects after connection attempt or waits for timeout.
- Repeats the process in a loop to block legitimate devices from pairing.
- Operates passively; does not advertise itself, making detection harder.

## Requirements
- Python ^3.9 
- Bluetooth-enabled device
- ESP32-WROOM MCU (exploits are launched from this microcontroller)
- Expressif IDF toolchain 

## Installation
1. install expressidf
2. Clone the repository:
    ```bash
    git clone https://github.com/yourusername/speaker_exploit.git
    cd speaker_exploit
    ```   

## Usage
1. Open vscode and enable the esp32-idf extension
2. Open this project through esp32-idf extension
3. connect esp32 board to your laptop 
4. Select the correct port which should be soemthing like /dev/tty.usbserial-0001
5. Build the project and flash it to the ESP32 board through the esp32-idf extension
6. You can see the logs in the terminal

## Disclaimer
This project is for educational purposes only. The authors are not responsible for any misuse or damage caused by the use of this software.

## Contributing
Contributions are welcome! Please submit a pull request or open an issue for discussion.

## License
This project is licensed under the [MIT License](LICENSE).