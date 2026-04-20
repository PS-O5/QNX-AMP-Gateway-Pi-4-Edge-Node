# QNX AMP Gateway: Pi 4 Zonal Controller & STM32 Edge Node

## Project Overview
This repository contains the architecture documentation, system builder configurations, and POSIX C source code for an Asymmetric Multi-Processing (AMP) Automotive Safety Gateway. 

The platform simulates a modern automotive zonal architecture, utilizing a true microkernel Real-Time Operating System (QNX 8.0) running on a Raspberry Pi 4 as the Zonal Controller, which securely interfaces with a bare-metal STM32 Edge Node. 

A primary technical focus of this project is deterministic hardware initialization, specifically achieving a stable bare-metal QNX boot from an external USB 3.0 SSD, mapping correct ARM GIC interrupt vectors, and implementing ASIL-inspired fault-tolerant binary data protocols.

## Hardware Architecture
* **Zonal Controller:** Raspberry Pi 4 Model B
* **Operating System:** BlackBerry QNX RTOS 8.0.0 (AArch64)
* **Boot Media:** External USB 3.0 SSD (via Ankeje/JMicron bridge)
* **Edge Node:** STM32F411CEU6 (Bare-metal C)
* **Debug Interface:** 3.3V FTDI USB-to-TTL Serial Bridge (mapped to Pi miniUART)
* **Inter-Processor Comms:** Deterministic UART via GPIO 4/5 (UART3 / GIC 153)

## Protocol Architecture
To simulate production automotive safety standards, this gateway explicitly rejects raw ASCII serial strings in favor of a deterministic, 9-byte packed binary protocol protected by CRC-16:
1.  **Framing (2 Bytes):** `0xAA 0x55` for state-machine synchronization.
2.  **Sequence (1 Byte):** 0-255 rolling counter to detect dropped packets.
3.  **Payload (4 Bytes):** IEEE 754 Float (Sensor Data).
4.  **Integrity (2 Bytes):** CRC-16-CCITT checksum to validate payload and sequence.

## Software Architecture: The Microkernel Paradigm
This gateway eschews monolithic driver design in favor of a distributed, fault-tolerant QNX topology:
* **The Edge Node:** Runs bare-metal C (zero HAL dependencies). Utilizes the HSI clock for zero baud drift, transmits CRC-validated telemetry, handles downstream actuation via `RXNE` hardware interrupts, and is protected by a 32kHz Independent Watchdog (IWDG) for ASIL compliance.
* **The Resource Manager:** A multi-threaded QNX daemon utilizing `thread_pool_create`. It manages raw `termios` I/O, evaluates the CRC state machine, and mounts the hardware directly into the QNX Virtual File System (VFS) as `/dev/asg_sensor` via `resmgr_attach()`.
* **The Network Bridge:** A decoupled POSIX daemon utilizing standard BSD sockets to read from the VFS node and broadcast telemetry across standard Ethernet via UDP.

## ⚠️ Intellectual Property & Licensing Notice
**In strict compliance with BlackBerry QNX software licensing and intellectual property guidelines, this repository DOES NOT contain any compiled QNX binaries, proprietary `.so` shared libraries, or Board Support Package (BSP) source code.**

This repository serves as an architectural blueprint. It contains the custom `.build` configurations, Raspberry Pi bootloader setups, and custom POSIX C application code. To deploy this gateway, you must compile the provided `rpi4.build` file using your own licensed QNX Software Development Platform (SDP 8.0).

## Milestones Achieved

- [x] **Milestone 1: Bare-Metal RTOS Bring-Up.** Successfully booted QNX 8.0 from an external SSD. Resolved Broadcom hardware pinmux conflicts and managed JMicron UAS CAM enumeration timing.
- [x] **Milestone 2: Edge Node Simulation.** Flashed STM32 with deterministic bare-metal C (bypassing HAL). Manipulated RCC, GPIOA, and USART2 registers for 115200 baud @ 16MHz.
- [x] **Milestone 3: QNX IPC Handshake.** Overcame ARM GIC vector mapping errors (Offset 153). Successfully ingested hardware loopback payloads in strict `termios` raw mode.
- [x] **Milestone 4: The Internal IPC Server.** Upgraded the link to a CRC-16 validated binary packet structure. Decoupled the monolithic reader into a threaded Client/Server architecture using native QNX Message Passing and Mutex locking. Established a bidirectional control loop utilizing STM32 NVIC hardware interrupts for downstream actuation.
- [x] **Milestone 5: Distributed Systems & VFS Integration.** Replaced internal IPC with a formal POSIX Resource Manager (`/dev/asg_sensor`). Implemented a UDP Network Bridge for cross-network telemetry broadcasting, and secured the bare-metal edge node with an Independent Hardware Watchdog (IWDG) to guarantee deterministic fault recovery.

## Build Instructions (For QNX License Holders)
To generate the bootable payload:
1. Source your QNX SDP 8.0 environment: `source ~/qnx800/qnxsdp-env.sh`
2. Cross-compile the application stack: `make all`
3. Generate the Initial File System: `mkifs qnx_workspace/build_files/rpi4.build ifs-rpi4.bin`
4. Stage `ifs-rpi4.bin` to the Pi's FAT32 boot partition alongside the provided `config.txt`.
