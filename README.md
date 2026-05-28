# EcoLight Maestro: Time & Intensity Adaptive Street Light Automation

This project implements an intelligent, energy-efficient municipal streetlight automation grid built on a bare-metal ARM7 LPC2148 microcontroller. The system balances chronological tracking with live ambient lux monitoring to eliminate municipal grid power-bleed.

## 📋 System Design & Features
- **Chronographic Scheduling:** Interfaces with the LPC2148's on-chip hardware **Real-Time Clock (RTC)** registers to track accurate time windows (Hours, Minutes, Seconds).
- **Ambient Light Sampling:** Maps light gradients using an LDR sensor circuit routed into the 10-bit Successive Approximation **ADC0.1 peripheral channel**.
- **Dual-Condition Automation Matrix:**
  - *Daytime Isolation Window (06:00 to 17:59):* Overrides all sensor fluctuations to force the entire streetlight output grid completely `OFF`, preventing accidental power activation caused by moving shadows or dense cloud cover.
  - *Nighttime Active Window (18:00 to 05:59):* Automatically shifts to an adaptive tracking state. The MCU samples immediate ADC counts; if the lux scale falls below a safety threshold of 400 counts, the GPIO streetlight banks are driven `ON`.
- **Asynchronous Admin Calibration Menu:** Utilizes External Interrupt 0 (`EINT0`) configured for falling-edge sensitivity. Tripping the physical switch stalls standard automated execution to open a runtime configuration menu, allowing live time adjustments using a 4x4 matrix keypad.
- **Acoustic Verification:** Integrates a physical GPIO buzzer output line to give real-time audible error alerts if an administrator tries to input an out-of-bounds clock metric.

## 🛠️ Hardware Stack
- Microcontroller: LPC2148 (ARM7TDMI-S Core)
- Inputs: LDR Sensor Module, 4x4 Matrix Keypad, Push Button Switch (EINT0)
- Outputs: 16x2 Character LCD Display, LED Streetlight Array, Warning Buzzer
- Peripherals Used: On-chip RTC, ADC0, GPIO, Vectored Interrupt Controller (VIC)

## 💻 Software Stack
- **Language:** Embedded C
- **IDE Framework:** Keil uVision
- **Flashing Target Tool:** Flash Magic
