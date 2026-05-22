# SmartLight
My bachelor graduation project

readme_content = """# Automated Lighting Control: An Adaptive IoT System with PID Optimization

An advanced, closed-loop smart micro-scale lighting system that dynamically balances energy conservation with ergonomic visual comfort. This edge-computing IoT system utilizes an ESP32 node to harvest natural daylight, achieving a stable, flicker-free dimming experience while significantly reducing power consumption.

## 🌟 Key Features

- **Daylight Harvesting:** Dynamically modulates LED intensity via PWM to maintain a constant target illuminance of **400 Lux** (the ergonomic sweet spot for working/reading).
- **Advanced Presence Detection:** Employs a **24GHz mmWave radar (LD2410)** capable of detecting stationary human presence through respiratory micro-movements, overcoming the limitations of traditional PIR sensors.
- **Discrete Positional PID Control:** Eliminates optical feedback oscillations and flickering caused by the proximity of the sensor and light source.
- **Signal Conditioning:** Implements an exponential smoothing low-pass filter, anti-windup logic, and a **±30 Lux Deadband** to ensure long-term system stability.
- **Full-Stack IoT Ecosystem:** Features sub-second latency data visualization and remote control via a custom **WeChat Mini Program** communicating through the **MQTT protocol** over Bemfa Cloud.
- **Dynamic Provisioning:** Integrates a Captive Portal framework for wireless network provisioning without hardcoded credentials.

---

## 🛠️ Hardware Architecture

| Component | Part / Module | Function |
| :--- | :--- | :--- |
| **Main Controller** | ESP32 SoC (Dual-core) | Edge-computing node, multi-tasking, WiFi client |
| **Ambient Light Sensor** | GY-302 (BH1750) | 16-bit linear digital Lux measurement via I2C |
| **Human Presence Sensor**| Hi-Link LD2410 | 24GHz millimeter-wave radar for micro-motion detection |
| **Actuator & Source** | COB Flexible LED Strip | Light source driven by hardware PWM dimming |

---

## 📐 Control Theory & Mathematical Modeling

The core of the system relies on a **Discrete Positional PID Algorithm** to compute the absolute PWM duty cycle output ($u[k]$) at each sampling interval:

$$u[k] = K_p e[k] + K_i \sum_{i=0}^{k} e[i] \Delta t + K_d \frac{e[k] - e[k-1]}{\Delta t}$$

Where the system error $e[k]$ is defined as:
$$e[k] = Setpoint(400\\text{ Lux}) - CurrentLux$$

### Engineering Optimizations:
1. **The 30 Lux Deadband:** $$if \\ |e[k]| < 30 \\implies e[k] = 0$$
   This prevents the microcontroller from making micro-oscillations and jitter once the target lux is reached.
2. **Integral Anti-Windup:** Caps the accumulated error sum to prevent brightness runaway when the sensor is heavily obstructed.

---

## 📊 Experimental Results

Continuous 12-hour simulation tests and continuous comparative evaluations under dynamic natural daylight ingress yielded the following results:
- **60% - 70% Energy Reduction** compared to traditional manual binary (On/Off) lighting systems.
- Complete eradication of forward optical feedback loops (stable, flicker-free transition).
- Zero power load achieved during peak natural daylight hours.

---

## 🚀 Getting Started

### Prerequisites
- **Development Environment:** Arduino IDE (v2.0+) or PlatformIO.
- **Required Libraries:**
  - `PubSubClient` (for MQTT communication)
  - `WiFiManager` (for Captive Portal implementation)
  - `Wire` (for I2C communication with BH1750)
  - `BH1750` library by Christopher Laws
