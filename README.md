This is a thin-edge.io child device client that can run on esp32 microcontrollers. 
It based on the [ESP-MQTT sample application](https://github.com/espressif/esp-idf/tree/v5.2.1/examples/protocols/mqtt/tcp).

## Requirements 

-----

### Hardware

- ESP32 Board supported Wi-Fi connectivity
- Raspberry Pi with [thin-edge.io](https://thin-edge.io) deployed 
- Router

### Setting up the thin-edge.io device

Since the ESP32 boadr needs to communicate with thin-edge.io over the network, the thin-edge.io instance needs to be setup to enable this communication.

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

------------
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
5. Set MQTT Broker URI
   Open main.c, set the MQTT Broker URI on line 180. Use the IP address of Raspberry Pi
6. Optional: change device ID
   You can change the device id. If you do so,do not forget change the device id in topics as well.
   
   :construction: Todo: Remove hardcoded topics
7. Build image
8. Select Flash Method

   For USB: UART
9. Flash Device
   Connect ESP32 Board to the PC, then flash device.
10. Optional: monitor device


Usage
---------

1. Send temperature measurements to Cumulocity IoT Platform via thin-edge device
2. Restart device on Cumulocity IoT Platform

   :construction: A restart command actually triggers two times of restarts. Maintenance in progress
3. Create an alarm before restart
4. Create an event after restart

