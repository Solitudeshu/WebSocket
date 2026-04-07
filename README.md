# CSC10008 WebSocket-based LAN Remote Administration System - AuraLink 

**Course:** Computer Networks  
**Institution:** University of Science, VNU-HCM  
**Faculty:** Faculty of Information Technology (FIT)  

## Student Information

* 24120498 - Phan Minh Anh
* 24120256 - Ho Ngoc Lan Anh
* 24120501 - Nguyen Le Thanh Huy

**Instructor:** PhD. Do Hoang Cuong

## Introduction

AuraLink is a Remote Administration Tool (RAT) designed to operate within a Local Area Network (LAN), allowing administrators to seamlessly monitor and control client workstations through a centralized web interface. The system leverages the WebSocket protocol to enable real-time bidirectional data transmission, effectively overcoming the latency limitations inherent to traditional HTTP Polling methods.

### System Architecture

The project is composed of three primary components:
1. **Agent (Server):** A background C++ application executed on the target client workstation.
2. **Dashboard (Client):** An HTML/JS web-based interface used by the administrator for control and monitoring.
3. **Discovery Service (Registry):** An intermediary server that facilitates automatic device discovery and IP mapping within the network.

---

## Technical Stack

The system is built utilizing advanced technologies and libraries to ensure high performance and seamless compatibility on Windows environments.

### Backend (C++ Agent & Registry)
* **Language:** C++ (C++14 and above).
* **Network Library:** Boost.Beast & Boost.Asio (for WebSocket handling and Asynchronous I/O).
* **System Core:** Windows API (Win32 API).
* **Multimedia:** Microsoft Media Foundation (Webcam streaming), Microsoft GDI (Image processing).
* **Audio/Speech:** Microsoft SAPI (Text-to-Speech synthesis).
* **Cryptography:** Windows Cryptography API (Base64 data encoding/decoding).

### Frontend (Dashboard)
* **Core:** HTML5, CSS3, Vanilla JavaScript.
* **Communication:** Standard WebSocket API.
* **UI Design:** Glassmorphism Design System.

---

## Features

The system provides a robust set of administrative tools, thoroughly tested in a LAN environment:

* **Auto Discovery:** Automatically scans the network and displays a list of currently online client machines.
* **Webcam Streaming:** Captures and streams live video from the client's webcam using Media Foundation.
* **Keylogger:** Records keystrokes in real-time, featuring full support for Vietnamese input (Telex/VNI) and Unicode processing.
* **Screenshot:** Captures the client's screen and transmits the image to the Dashboard in real-time.
* **System Monitor:** Tracks real-time system metrics and retrieves active processes and applications.
* **File Explorer:** Navigates the client's directory tree, drives, and supports remote file downloading.
* **Clipboard Manager:** Monitors and retrieves text content from the client's clipboard.
* **Text-to-Speech:** Allows the administrator to send text messages that are synthesized and played as audio on the client machine.
* **Power Control:** Remotely executes system shutdown or restart commands.

---

## System Requirements

* **IDE:** Visual Studio 2019 or 2022.
* **Dependencies:** Boost C++ Libraries (Requires proper configuration of Include and Library paths in the Visual Studio Project Settings).

---

## Installation and Build Instructions

### 1. Build the Server (Agent)
This is the background application that runs on the target machine you wish to control.

1. Navigate to the `Server/` directory.
2. Open the solution file **`Server.sln`** using Visual Studio.
3. Ensure the Build Configuration is set to **Release** and the platform is **x64**.
4. Press `Ctrl + Shift + B` to build the solution.
5. The compiled executable `Server.exe` will be generated in the `x64/Release/` directory.

### 2. Build the Registry (Discovery Server)
This is the intermediary server responsible for network IP discovery.

1. Navigate to the `Register/` directory.
2. Open the solution file **`Register.sln`** using Visual Studio.
3. Build the project using the same configuration as the Server (**Release / x64**).
4. Run the compiled `Register.exe` on the Administrator's machine.

### 3. Run the Client (Dashboard)
The web-based control interface does not require compilation.

1. Navigate to the `Client/` directory.
2. Open the **`index.html`** file using any modern web browser (e.g., Google Chrome, Microsoft Edge, Mozilla Firefox).
3. Click the **Scan Network** button to discover devices, then select a target Server to establish the WebSocket connection.
