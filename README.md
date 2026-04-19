 2DX3 Hallway Scanner

Embedded scanning project for the MSP432E401Y using a VL53L1X time-of-flight sensor and a stepper motor to collect distance measurements across a full sweep. The firmware captures multiple scans, streams the results over UART, and can be paired with a MATLAB post-processing script to convert the scan data into a point cloud.

## Overview

This project rotates a VL53L1X sensor through a set of predefined angles, records distance measurements, and outputs the scan results in a simple serial format for later visualization or reconstruction.

Core behavior:

- Uses the VL53L1X ToF sensor for distance measurements
- Rotates the sensor with a stepper motor connected to Port H
- Starts and stops scans using the PJ0 push button interrupt
- Collects `3` scans by default (`NUM_SCANS`)
- Samples `128` angular positions per scan (`NUM_ANGLES`)
- Sends scan data over UART in CSV-style lines

## Main Files

- `scanningLogic.c`: main scan controller, motor stepping logic, button interrupt handling, and UART output formatting
- `VL53L1X_api.c`: ST ToF sensor API
- `vl53l1_platform_2dx4.c`: low-level platform support for the sensor
- `uart.c`: UART initialization and serial printing helpers
- `SysTick.c`: delay utilities
- `PLL.c`: clock setup
- `onboardLEDs.c`: board LED support
- `KeilProject.uvprojx`: Keil uVision project file
- `zareefXYZHallwayProper.xyz`: example point-cloud style output generated from scan data

## Hardware / Interfaces

- **Board:** Texas Instruments MSP432E401Y
- **Sensor:** VL53L1X time-of-flight distance sensor
- **Motor:** stepper motor driven from `PH0-PH3`
- **Button input:** `PJ0`
- **Sensor reset / XSHUT control:** `PG0`
- **I2C:** `PB2-PB3`
- **UART:** `PA0-PA1`

## How It Works

1. The firmware initializes the clock, SysTick, UART, GPIO, stepper motor pins, button interrupt, and VL53L1X sensor.
2. A button press on `PJ0` starts a scan.
3. The sensor is rotated through `128` predefined angle entries covering approximately `0` to `357` degrees.
4. At each position, the firmware waits for valid ToF data and stores the measured distance.
5. Invalid readings are marked as `65535`.
6. When a scan finishes, the motor returns to its starting position and the results are printed over UART.
7. The process repeats until `3` scans have been completed.

If the button is pressed during an active scan, the firmware stops early, returns the sensor to its start position, and requests a rescan of the same scan index.

## UART Output Format

Each successful measurement is transmitted as:

```text
scan_index,angle_degrees,distance_mm
```

Example:

```text
0,90,734
1,123,65535
```

Control/status messages include:

- `END_SCAN` when one scan finishes and more remain
- `END` when all scans are complete
- `STOPPED` when a scan is interrupted
- `RESCAN n` to indicate which scan should be repeated

## My Contribution

My main implementation work for this project was the scan-control logic in `scanningLogic.c`, including:

- stepper motor sweep control
- button-based start/stop behavior
- scan state management across multiple passes
- distance filtering for invalid measurements
- UART formatting for downstream processing

If `scanner.m` is included with the full submission, it can be described as the post-processing script used to take the UART scan output and convert it into coordinates for visualization or `.xyz` export.

## Build / Run

This project is set up for **Keil uVision** with the MSP432E401Y device pack.

To run:

1. Open `KeilProject.uvprojx` in Keil uVision.
2. Build and flash the project to the MSP432E401Y board.
3. Connect the VL53L1X sensor, stepper motor, and button to the pins listed above.
4. Open a serial terminal connected to UART0.
5. Press the PJ0 button to begin scanning.
6. Capture the UART output for plotting or conversion into point-cloud data.
