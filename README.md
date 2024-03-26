# Freertos Esp32 Client for thin-edge.io

This is a thin-edge.io child device client that can run on esp32 microcontrollers. 
It based on the [ESP-MQTT sample application](https://github.com/espressif/esp-idf/tree/v5.2.1/examples/protocols/mqtt/tcp).

## Features

The following features are supported by the client.

* [x] Firmware Over the Air (OTA) updates
* [x] Restart device
* [x] Send measurements periodically


## Requirements 

### Hardware

- ESP32 Board with Wi-Fi connectivity (with at least 4MB Flash)
- Raspberry Pi with [thin-edge.io](https://thin-edge.io) deployed 
- Router

**Notes**

* At least 4MB of Flash is required as the Firmware OTA update requires at least 2 application slots to ensure robust updates

#### Example devices

* diymore ESP32 USB C ESP32 NodeMCU Development board ESP32 WROOM 32 2.4 GHz WLAN WiFi Bluetooth CP2102 Chip
   * Processor: ESP32-WROOM-32 (4MB Flash)
* diymore ESP32 CAM Development board ESP32 USB C, WLAN/Bluetooth, ESP32 DC 5V Dual-Core Development board with 2640 Camera Module
   * Processor: ESP32-S (8MB Flash)

### Setting up the thin-edge.io device

Since the ESP32 board needs to communicate with thin-edge.io over the network, the thin-edge.io instance needs to be setup to enable this communication.

1. On the main device where the MQTT broker, tedge-mapper-c8y and tedge-agent, run the following commands to set the configuration:

    ```sh
    tedge config set c8y.proxy.bind.address 0.0.0.0
    tedge config set mqtt.bind.address 0.0.0.0
    tedge config set http.bind.address 0.0.0.0

    tedge config set c8y.proxy.client.host $HOST.local
    tedge config set http.client.host $HOST.local
    ```

    If your `$HOST` variable is not set, then replace it with the actual name of the host your are running on (e.g. the address must be reachable from outside of the device)

2. Restart the services

    ```
    systemctl restart tedge-agent
    tedge reconnect c8y
    ```

3. Check that the address is reachable from outside, e.g.

    ```sh
    curl http://$HOST.local:8001/c8y/inventory/managedObjects/
    ```

### ESP-IDF

ESP-IDF Visual Studio Code Extension is recommended. Here is [the installation tutorial](https://github.com/espressif/vscode-esp-idf-extension/blob/HEAD/docs/tutorial/install.md) of it.

You can also follow the [Getting Started](https://docs.espressif.com/projects/esp-idf/en/v4.2.3/esp32/get-started/index.html#) in the ESF-IDF Programming Guide to prepare the required software.

## Get started

1. Import project

   Clone this repo to your PC, import this project into VS Code. At the bottom of the window you can find the ESP-IDF tools.

   ![](./images/Toolbar.png)

2. Select port to use.

   Select the port where the esp32 connects to your computer. Port name you can find in `dev` folder

   `ls /dev  --> /dev/ttyUSB1`
   
3. Set device target

   Select matching board target and OpenOCD Configuration File Paths list. For example: 

      - ESP32
      - ESP32 chip (via ESP USB Bridge)
4. Set Wi-Fi
   Open EDF-IDF: SDK Configuration Editor. Under 'Example Connection Configuration' set Wi-Fi SSID and password 
   
   :warning: May need to repeat step 2-4 when you reopen VS Code
5. Build image
6. Select Flash Method

   For USB: UART
7. Flash Device
   Connect ESP32 Board to the PC, then flash device.
8. Optional: monitor device

## Troubleshooting

### Microcontroller does not discover a valid thin-edge.io instance

If you the microcontroller does not detect a thin-edge.io instance anywhere, then you are probably not using a thin-edge.io built using either [Rugpi](https://thin-edge.github.io/thin-edge.io/extend/firmware-management/building-image/rugpi/) or [Yocto](https://thin-edge.github.io/thin-edge.io/extend/firmware-management/building-image/yocto/), as these images will install an avahi service will make thin-edge.io auto discoverable to other devices in the local network.

If you can't use on of the above images, then you can manually control which thin-edge.io the microcontroller will connect to by configuring the `MANUAL_MQTT_HOST` variable in the [main/main.c](main/main.c) file.

```c
const char *MANUAL_MQTT_HOST = "my-rpi-host";
```


Usage

1. Send temperature measurements to Cumulocity IoT Platform via thin-edge device
2. Restart device on Cumulocity IoT Platform

   :construction: A restart command actually triggers two times of restarts. Maintenance in progress
3. Create an alarm before restart
4. Create an event after restart

### Building does not pull in the correct dependencies

If you encounter a build problem about some missing dependencies, e.g. `mdns.h`, then you may need to reconfigure the project to force idf to download the missing dependency. You can do this by running the following commands:

```sh
idf.py reconfigure
idf.py build
```

## Firmware updates

To support testing firmware updates you will need to build two versions of the same application.

1. Create a firmware entry in Cumulocity IoT (you only need to do this once for the tenant)

   ```sh
   c8y firmware get --id esp32-tedge || c8y firmware create --name "esp32-tedge"
   ```

2. Build the first version of the application and upload it (assuming the current version is "1.1.0")

   ```sh
   idf.py build
   c8y firmware versions create --firmware esp32-tedge --version "1.1.0" --file ./build/freertos-esp32-client.bin
   ```

3. You should also flash the version to the esp32 microcontroller

4. Build a new version, but edit the `APPLICATION_VERSION` and set to a new version, e.g. `1.2.0`

   ```sh
   idf.py build
   c8y firmware versions create --firmware esp32-tedge --version "1.2.0" --file ./build/freertos-esp32-client.bin
   ```

5. In Cumulocity IoT, you can now trigger a firmware update using the `esp32-tedge` firmware name

