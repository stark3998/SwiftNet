
# SwiftNet: Adaptive IoT Communication System

## Table of Contents
1. [Project Overview](#project-overview)
2. [Features](#features)
3. [System Architecture](#system-architecture)
4. [Hardware Components](#hardware-components)
5. [Setup and Installation](#setup-and-installation)
6. [How It Works](#how-it-works)
7. [Challenges and Future Improvements](#challenges-and-future-improvements)
8. [Demo Video](#demo-video)
9. [Source Code](#source-code)
10. [Contributing](#contributing)
11. [License](#license)

---

## Project Overview
SwiftNet is a low-latency IoT communication system designed to prioritize local device interactions while selectively engaging with cloud services. It employs a dynamic master-slave architecture using UDP for efficient local data exchange and features a custom web-based interface hosted on AWS for real-time data visualization and log analysis.

---

## Features
- **Dynamic Master-Slave Selection**: Adaptive role assignment among devices to optimize communication efficiency.
- **UDP Communication**: Facilitates fast and reliable local data exchange between IoT devices.
- **Selective Cloud Integration**: Enables efficient data offloading to the cloud, reducing unnecessary data transmission.
- **Web-Based Interface**: Hosted on AWS, providing real-time data visualization and analysis.
- **Scalability**: Designed to support the addition of new IoT devices seamlessly.

---

## System Architecture
The system comprises three ESP8266 microcontrollers, a Raspberry Pi, and various sensors. Local communication is managed via UDP, with a dynamically selected master device overseeing operations. A custom web interface, hosted on AWS, offers real-time data visualization and log analysis.

### Repository Structure
The repository is organized as follows:

```
SwiftNet/
├── ESP8266_Code/          # Firmware for ESP8266 microcontrollers
├── rasp_pi_server/        # Server code for Raspberry Pi
├── user_interface/        # Web interface code hosted on AWS
├── .gitignore
└── README.md
```

---

## Hardware Components

| Component/Part            | Quantity |
|----------------------------|----------|
| ESP8266 Microcontrollers   | 3        |
| Raspberry Pi               | 1        |
| Circular LED Board         | 1        |
| LED Matrix                 | 1        |
| 7-Segment Display          | 1        |
| Push Button                | 1        |
| Photoresistors             | 3        |
| Shift Registers            | 2        |

---

## Setup and Installation

### 1. Hardware Setup
1. **Connect ESP8266 Microcontrollers**: Flash the firmware located in the `ESP8266_Code` directory to each microcontroller.
2. **Configure Raspberry Pi**: Set up the Raspberry Pi with the server code found in the `rasp_pi_server` directory.
3. **Assemble Components**: Connect the LED boards, sensors, and shift registers to the Raspberry Pi and ESP8266 microcontrollers as per the provided schematics.

### 2. Software Installation
#### Prerequisites
- Python 3.x
- Node.js
- AWS Account with EC2 instance configured

#### Steps
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/stark3998/SwiftNet.git
   cd SwiftNet
   ```
2. **Install Dependencies**:
   ```bash
   pip install -r rasp_pi_server/requirements.txt
   ```
3. **Start the Local UDP Server**:
   ```bash
   python rasp_pi_server/udp_server.py
   ```
4. **Deploy Web Interface to AWS**:
   - Navigate to the `user_interface` directory.
   - Install Node.js dependencies:
     ```bash
     npm install
     ```
   - Start the web server:
     ```bash
     npm start
     ```
5. **Access the Web Interface**:
   - Open a web browser and navigate to the AWS instance's public IP address to view the interface.

---

## How It Works
1. **Local Communication**: Devices communicate locally using UDP, with a dynamically selected master device coordinating interactions.
2. **Master Selection**: The system evaluates sensor activity to determine the master device, implementing a buffer period to prevent frequent role changes.
3. **Cloud Integration**: The master device selectively offloads data to the AWS-hosted web interface for analysis and visualization.
4. **Web Visualization**: The AWS-hosted interface provides real-time and historical data visualization, enabling users to monitor system performance and sensor data.

---

## Challenges and Future Improvements

### Challenges
- **Seamless Master Transitions**: Addressed by implementing a buffer period to ensure stable role assignments during rapid sensor activity changes.
- **Web Interface Scalability**: Optimized AWS server configurations to handle increased data loads and user interactions.
- **GPIO Limitations**: Utilized shift registers to expand the available GPIO pins on the Raspberry Pi, accommodating additional components.

### Future Improvements
- **Automated Master Failure Recovery**: Develop a mechanism to automatically reassign the master role in the event of a device failure.
- **Enhanced Data Visualization**: Incorporate advanced features such as customizable dashboards and interactive graphs for deeper data analysis.
- **Power Optimization**: Implement strategies to reduce power consumption of the ESP8266 microcontrollers and Raspberry Pi, enhancing suitability for energy-constrained applications.

---

## Demo Video
A demonstration of the system is available here: [SwiftNet Demo Video](https://youtu.be/QuUuNcS8NnI)

---

## Source Code
The complete source code is available in this repository. Refer to the [Repository Structure](#system-architecture) section for details on the organization of the codebase.

---

## Contributing
Contributions are welcome! Please follow the guidelines provided in this repository for submitting issues and pull requests.

---

## License
This project is licensed under the [MIT License](LICENSE).

---
