# QNX AMP Gateway Pi 4 Edge Node


## Project Overview
This repository contains the architecture documentation, system builder configurations, and POSIX C source code for an Asymmetric Multi-Processing (AMP) Automotive Safety Gateway. 

The platform simulates a modern automotive zonal architecture, utilizing a true microkernel Real-Time Operating System (QNX 8.0) running on a Raspberry Pi 4 as the Zonal Controller, which securely interfaces with a bare-metal STM32 Edge Node. 

A primary technical focus of this project is deterministic hardware initialization, specifically achieving a stable bare-metal QNX boot from an external USB 3.0 SSD by bypassing consumer-grade USB Attached SCSI (UAS) hardware quirks and managing QNX CAM subsystem race conditions.

## Hardware Architecture
* **Zonal Controller:** Raspberry Pi 4 Model B
* **Operating System:** BlackBerry QNX RTOS 8.0.0 (AArch64)
* **Boot Media:** External USB 3.0 SSD (via Ankeje/JMicron bridge)
* **Edge Node:** STM32F411CEU6 (Bare-metal C / FreeRTOS)
* **Debug Interface:** 3.3V FTDI USB-to-TTL Serial Bridge (mapped to Pi miniUART)
* **Inter-Processor Comms:** Deterministic UART (Migrating to SPI/CAN)

## ⚠️ Intellectual Property & Licensing Notice
**In strict compliance with BlackBerry QNX software licensing and intellectual property guidelines, this repository DOES NOT contain any compiled QNX binaries, proprietary `.so` shared libraries, or Board Support Package (BSP) source code.**

This repository serves as an architectural blueprint. It contains the custom `.build` configurations, Raspberry Pi bootloader setups, and custom POSIX C application code. To deploy this gateway, you must compile the provided `rpi4.build` file using your own licensed QNX Software Development Platform (SDP 8.0).


## Current Status & Milestones

[x] Milestone 1: Bare-Metal RTOS Bring-Up. Successfully boot QNX 8.0 from an external SSD. Resolved Broadcom hardware pinmux conflicts (Bluetooth vs. PL011) and managed JMicron UAS CAM enumeration timing. (See docs/QNX_RPI4_BringUp.pdf for full engineering log).

[ ] Milestone 2: Edge Node Simulation. Flash STM32 with deterministic 500ms UART transmission loop.

[ ] Milestone 3: QNX IPC Handshake. Cross-compile qnx_uart_rx.c, inject it into the QNX Initial File System (IFS), and successfully parse STM32 sensor payloads in the QNX shell.

## Build Instructions (For QNX License Holders)

To generate the bootable payload:

Source your QNX SDP 8.0 environment: source ~/qnx800/qnxsdp-env.sh

Cross-compile the IPC application: qcc -Vgcc_ntoaarch64le qnx_workspace/apps/qnx_uart_rx.c -o qnx_uart_rx

Generate the Initial File System: mkifs qnx_workspace/build_files/rpi4.build ifs-rpi4.bin

Stage ifs-rpi4.bin to the Pi's FAT32 boot partition alongside the provided config.txt.
