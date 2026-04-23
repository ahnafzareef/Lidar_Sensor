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
- Sends scan data over UART in lines

## How It Works

1. The firmware initializes the clock, SysTick, UART, GPIO, stepper motor pins, button interrupt, and VL53L1X sensor.
2. A button press on `PJ0` starts a scan.
3. The sensor is rotated through `128` predefined angle entries covering approximately `0` to `357` degrees.
4. At each position, the firmware waits for valid ToF data and stores the measured distance.
5. Invalid readings are marked as `65535`.
6. When a scan finishes, the motor returns to its starting position and the results are printed over UART.
7. The process repeats until `3` scans have been completed.

If the button is pressed during an active scan, the firmware stops early, returns the sensor to its start position, and requests a rescan of the same scan index.

