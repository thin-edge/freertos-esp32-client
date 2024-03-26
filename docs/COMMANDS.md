## Command

The client supports the following commands, and their sequence diagram are listed below.

### Restart device

Trigger a device restart.

```mermaid
sequenceDiagram
    thin-edge.io->>controller: Send "restart" command
    controller-->>thin-edge.io: Create alarm
    controller-->>controller: Restart device
    controller-->>thin-edge.io: Create "Device rebooted" event
    controller-->>thin-edge.io: Clear alarm
    controller-->>thin-edge.io: Set operation to SUCCESSFUL
```

### Firmware update

Update the firmware/application running on the device by downloading a new image. The process is similar to an A/B update where there are two application slots, and the non-active partition receives the new image, then the device reboots into the new image. The new application will be verified if it is ok, and commit to the current application, otherwise rollback to the previous partition (which will require an additional restart).

```mermaid
sequenceDiagram
    thin-edge.io->>controller: Send "firmware_update" operation
    controller-->>thin-edge.io: Set operation to EXECUTING
    
    controller-->>controller: Trigger ESP-IDF esp_https_ota update
    controller-->>controller: Restart device

    alt New image ok
      controller-->>thin-edge.io: Set operation to SUCCESSFUL
    else No update in progress
      controller-->>thin-edge.io: Set operation to FAILED
    else Misc
      controller-->>thin-edge.io: Set operation to FAILED
    end
```
