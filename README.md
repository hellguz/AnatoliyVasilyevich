# Anatoliy Vasilyevich Bot

Anatoliy Vasilyevich Bot is a project designed to share memes among friends in their physical space using a Heltec Wireless Paper (ESP32 with E-Ink panel) as the medium for displaying memes. The project leverages a Telegram bot to upload new memes to all connected devices.

## Table of Contents

- Introduction
- Project Structure
- Hardware Requirements
- Software Requirements
- Installation
- Usage
- Technical Details
  - Backend
  - Frontend
- Contributing
- License

## Introduction

The Anatoliy Vasilyevich Bot project aims to create a fun and interactive way to share memes among friends using a physical device. The project uses a Heltec Wireless Paper (ESP32 with E-Ink panel) to display memes, and a Telegram bot to manage meme uploads and notifications. Users can upload memes via the Telegram bot, which are then displayed on the E-Ink panels of all connected devices.

## Project Structure

```
.
├── .env
├── .gitignore
├── anatoliy.py
├── AnatoliyFace/
│   ├── AnatoliyFace.ino
│   ├── AnatoliyFace.ino.bak
├── data/
├── docker-compose.yml
├── Dockerfile
├── requirements.txt
└── users/
    ├── networks.json
    └── user_data.json
```

## Hardware Requirements

- Heltec Wireless Paper (ESP32 with E-Ink panel)
- Wi-Fi network

## Software Requirements

- Python 3.10
- Docker
- Docker Compose

## Installation

1. Clone the repository:

```sh
git clone https://github.com/yourusername/anatoliy-vasilyevich-bot.git
cd anatoliy-vasilyevich-bot
```

2. Create a .env file with the following content:

```
TOKEN=<your-telegram-bot-token>
DOMAIN_NAME=<your-domain-name>
WELCOME_TEXT="Welcome! Your user ID has been registered."
NOTIFY_PROMPT="Do you want to notify all users about this upload?"
UPDATE_TEXT="A new image has been uploaded!"
WIFI_CONNECT_TEXT="Thank you for sharing your network!"
```

3. Build and run the Docker container:

```sh
docker-compose up --build
```

## Usage

1. Start the Telegram bot by sending the `/start` command.
2. Upload a meme by sending a photo to the bot.
3. The bot will prompt you to notify all users about the new meme.
4. The meme will be displayed on the E-Ink panels of all connected devices.

## Technical Details

### Backend

The backend is implemented using Python and the Starlette framework. It handles the following functionalities:

- **Telegram Bot**: The bot is implemented using the `python-telegram-bot` library. It handles commands, photo uploads, and user interactions.
- **Webhook**: The bot uses a webhook to receive updates from Telegram.
- **Image Processing**: Uploaded images are processed using the Pillow library to resize and convert them to a format suitable for the E-Ink panel.
- **Wi-Fi Book**: The bot maintains a list of known Wi-Fi networks and their credentials, which are used by the ESP32 devices to connect to the internet.
- **Endpoints**:
  - `/telegram`: Webhook endpoint for Telegram updates.
  - `/get_last_img`: Serves the latest uploaded image.
  - `/get_last_xbm`: Serves the latest uploaded image as an XBM text.
  - `/get_last_md5`: Returns the MD5 hash of the latest uploaded image.
  - `/get_wifi_book`: Serves the 

networks.json

 file as a JSON response.

### Frontend

The frontend is implemented on the Heltec Wireless Paper (ESP32 with E-Ink panel) using the Arduino framework. It handles the following functionalities:

- **Wi-Fi Connection**: The device attempts to connect to known Wi-Fi networks. If it fails, it scans for available networks and tries to connect to them.
- **Image Display**: The device downloads the latest meme from the server and displays it on the E-Ink panel.
- **Preferences**: The device stores the last successful Wi-Fi connection and the MD5 hash of the last displayed image in its preferences.
- **Endpoints**:
  - `/set`: Endpoint to receive and process new image data.
  - `/`: Endpoint to serve the main HTML page.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the MIT License. 