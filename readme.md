# Bluetooth Speaker Exploits
![Security](https://img.shields.io/badge/category-security-red)
![Bluetooth](https://img.shields.io/badge/category-bluetooth-blue)
This project provides a collection of exploits targeting Bluetooth speakers using esp32-wroom-32. It is intended for educational and research purposes only. Please use responsibly and ensure compliance with applicable laws and regulations.

## Features
- Various exploit techniques for Bluetooth speakers
- Focus on understanding vulnerabilities in Bluetooth protocols
- Tools and scripts for testing and demonstration

## Requirements
- Python 3.x
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
1. Ensure your Bluetooth device is discoverable.
2. compile with idf.py build
3. connect esp32 board to your laptop 
4. flash the firmware 
5. idf.py -p PORT monitor to check the eps32 logs


## Disclaimer
This project is for educational purposes only. The authors are not responsible for any misuse or damage caused by the use of this software.

## Contributing
Contributions are welcome! Please submit a pull request or open an issue for discussion.

## License
This project is licensed under the [MIT License](LICENSE).