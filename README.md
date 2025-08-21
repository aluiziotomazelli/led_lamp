# ESP32 Smart LED Lamp Controller

## 1. Project Overview

This project is an advanced firmware for an ESP32-based smart LED lamp. It provides a rich set of features including multiple dynamic lighting effects, user-configurable settings, and wireless master/slave synchronization. The architecture is built on FreeRTOS and is designed to be modular and extensible.

## 2. Features

- **Multiple Lighting Effects:** Includes a variety of pre-programmed effects such as Candle, White Temperature, Static Color, Breathing, and several Christmas-themed animations.
- **Advanced Configuration:** Users can enter special modes to configure parameters for each effect (e.g., color, speed, intensity) and adjust system-wide settings (e.g., minimum brightness, LED strip offsets).
- **Persistent Storage:** All user settings and the last active state are saved to Non-Volatile Storage (NVS), so the lamp remembers its configuration after a power cycle.
- **Wireless Master/Slave Sync:** Using ESP-NOW, multiple lamps can be synchronized. A designated "Master" lamp can control the state (power, brightness, effect) of all "Slave" lamps in real-time.
- **Rich User Input:** Supports multiple input types, including a rotary encoder with a push-button, a capacitive touch sensor, and a physical switch.
- **Over-the-Air (OTA) Updates:** The firmware can be updated wirelessly by setting an OTA flag in NVS and rebooting.

## 3. Architecture

The firmware uses an event-driven architecture built on FreeRTOS, with a clear separation of concerns between components. The core of the system relies on tasks and queues for communication, ensuring that different parts of the system are decoupled and can operate independently.

The general data flow is as follows:

`Hardware Inputs -> Input Queues -> Input Integrator -> FSM Queue -> FSM -> LED Command Queue -> LED Controller -> LED Driver -> Physical LEDs`

![Flowchart](fluxograma.txt)

### FreeRTOS Tasks & Queues

The system is multi-threaded, with different key responsibilities handled by separate tasks:
- **Input Tasks:** Each hardware input device (`button`, `encoder`, `touch`, `switch`) runs in its own task, polling the hardware and pushing raw events into a dedicated FreeRTOS queue.
- **`integrator_task`:** Collects all raw events from the various input queues into a single, unified event queue for the state machine.
- **`fsm_task`:** The main state machine task. It waits for integrated events and processes them based on the system's current state, sending logical commands to the LED controller.
- **`led_command_task`:** Processes logical commands (like "set brightness") from the FSM and updates the LED controller's internal state.
- **`led_render_task`:** A high-frequency task that continuously calculates the colors for the LED strip based on the current effect and state, and sends the final pixel buffer to the driver.
- **`led_driver_task`:** A low-level task that sends the pixel buffer to the physical LED strip (e.g., via SPI).

## 4. Component Breakdown

The project is organized into several components located in the `components/` directory.

- **`main`**: The entry point of the application. It initializes all hardware, creates the queues, and starts the FreeRTOS tasks.

- **Input Drivers (`button`, `encoder`, `touch`, `switch`)**: These are low-level components that abstract the hardware inputs. They handle debouncing, timing (for clicks, double-clicks, long-presses), and rotational steps, providing clean events to the rest of the system.

- **`input_integrator`**: A simple but crucial component that multiplexes events from all input queues into a single queue for the FSM. It uses a `QueueSet` to wait on multiple queues at once.

- **`fsm` (Finite State Machine)**: The core logic of the lamp. It defines the different states (`MODE_OFF`, `MODE_DISPLAY`, `MODE_EFFECT_SELECT`, etc.) and determines how to react to user inputs in each state. It is responsible for translating user actions into high-level commands for the `led_controller`.

- **`led_controller`**: The high-level manager for the lighting effects. It holds the current state of the light (on/off, brightness, effect, parameters). It receives commands from the FSM and runs the `led_render_task` to generate the visual output.

- **`led_effects`**: This is not a component, but a collection of source files inside `led_controller/effects/`. Each file defines a specific lighting animation (e.g., `candle.c`, `breathing.c`). They are self-contained and expose a `run` function that the `led_controller` calls. The order and list of effects are defined in `led_effects.c`.

- **`led_driver`**: The low-level driver that communicates with the physical LED strip hardware (e.g., WS2812B, SK6812). It receives a buffer of pixel data and clocks it out.

- **`espnow_controller`**: Manages the ESP-NOW wireless communication protocol for master/slave synchronization. The master's FSM directs it to send commands, and the slave's `input_integrator` receives them.

- **`nvs_manager`**: A helper component that abstracts reading from and writing to the Non-Volatile Storage. It defines data structures for settings that need to be persisted.

- **`ota_updater`**: Handles the Over-the-Air update process when triggered.

## 5. How to Build and Flash

This project is based on the Espressif IoT Development Framework (ESP-IDF).

1.  **Set up ESP-IDF:** Ensure you have a working installation of the ESP-IDF. Please follow the [official Espressif guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

2.  **Configure the Project:**
    ```bash
    idf.py set-target esp32
    idf.py menuconfig
    ```
    Inside `menuconfig`, you can configure project-specific settings, such as Wi-Fi credentials (if needed), pin assignments, and component options.

3.  **Build the Project:**
    ```bash
    idf.py build
    ```

4.  **Flash the Project:** Connect the ESP32 board to your computer and run:
    ```bash
    idf.py -p /dev/ttyUSB0 flash
    ```
    Replace `/dev/ttyUSB0` with your board's serial port.

5.  **Monitor the Output:**
    ```bash
    idf.py -p /dev/ttyUSB0 monitor
    ```
