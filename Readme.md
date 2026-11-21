
# AXI GPIO IP Core Example — PYNQ-Z1

An example of using the **Xilinx AXI GPIO IP core** on a PYNQ-Z1 (Zynq-7020) to
drive the on-board LEDs / RGB LEDs and read the switches / buttons from Linux
via **libgpiod**.

The project covers the full stack:

1. **Hardware (Vivado)** — block design with a dual-channel `axi_gpio` connected
   to the PS.
2. **Linux (Yocto)** — a Scarthgap image where the core appears as
   `/dev/gpiochip0`.
3. **Application (C++20)** — `axi-gpio`, a libgpiod-based demo that blinks LEDs
   and reacts to the buttons with interrupts.

---

## Overview

| Item        | Value                                                     |
|-------------|-----------------------------------------------------------|
| Board       | PYNQ-Z1 (Zynq-7020, `xc7z020clg400-1`)                    |
| IP core     | `xilinx.com:ip:axi_gpio` @ **0x4120_0000** (dual channel) |
| Channel 1   | 9-bit output → LED2–LED4 + RGBLED1–6                       |
| Channel 2   | 6-bit input → SWITCH1–2 + BUTTON1–4                        |
| Linux device| `/dev/gpiochip0` (libgpiod v2)                            |
| App         | `axi-gpio` (C++20)                                         |
| Toolchain   | Vivado **2024.2**, Yocto **Scarthgap** (kas)              |

## Setup environment

- Vivado 2024.2
- Yocto Scarthgap + [kas](https://kas.readthedocs.io/)
- PYNQ-Z1 board
- Ubuntu 22.04 (or a similar Linux host)

## Hardware design

### Block design (`Vivado/scripts/pynq_z1_bd.tcl`)

The block design contains:

- `processing_system7` — the Zynq ARM Cortex-A9 PS.
- `axi_gpio_0` — a **dual-channel** AXI GPIO at base address `0x4120_0000`:
  - `GPIO` (channel 1) → `GPIO_OUT` interface (9-bit output).
  - `GPIO2` (channel 2) → `GPIO_IN` interface (6-bit input).
- `proc_sys_reset`, `xlconcat`, `smartconnect` — reset / interrupt / AXI glue.

### Top level & pins

`Vivado/src/pynq_z1_top.vhd` connects the AXI GPIO channels to the PYNQ-Z1
on-board peripherals; `Vivado/src/pynq_z1.xdc` assigns the package pins:

| Peripheral | Pins (package)                          |
|------------|-----------------------------------------|
| SWITCH1/2  | `M20`, `M19`                            |
| BUTTON1–4  | `D19`, `D20`, `L20`, `L19`              |
| LED1–4     | `R14`, `P14`, `N16`, `M14`              |
| RGBLED1–6  | `L15`, `G17`, `N15`, `G14`, `L14`, `M15`|

## Linux / device tree

The `axi_gpio` node is **generated** from the exported `.xsa` by
`meta-xilinx-tools` and bound to the Xilinx GPIO driver, appearing as
`/dev/gpiochip0`. `system-user.dtsi` only adds board-level pieces (console
bootargs, Ethernet PHY, USB, SPI flash).

Because the core is dual-channel, the Linux GPIO lines are offset:

| Channel          | Linux lines |
|------------------|-------------|
| GPIO (output)    | 0–8         |
| GPIO2 (input)    | 32–37       |

## Application (`apps/gpio-test`)

A C++20 demo (`axi-gpio` binary) using **libgpiod v2**:

- `Gpio/` — thin libgpiod wrapper (`SetOutput`, `GetPinValue`, interrupt
  callbacks).
- `main.cpp`:
  - Blinks an LED; **BUTTON1** speeds the blink up, **BUTTON2** slows it down.
  - **BUTTON3 / BUTTON4** cycle the colour of RGB LED 1 / 2 (interrupt-driven).

Pin map used by the app:

```cpp
constexpr uint32_t LED2_PIN = 0;      // … outputs on channel 1
constexpr uint32_t RGB_LED1_PIN0 = 3; // …
constexpr uint32_t SWITCH1_PIN = 32;  // … inputs on channel 2
constexpr uint32_t BUTTON1_PIN = 34;  // …
```

The binary is installed to `/home/root/axi-gpio`.

## Build

### 1. Hardware — Vivado

```bash
source <Vivado_install>/settings64.sh
cd Vivado
make
```

`make` runs the Tcl scripts to create the project, build the bitstream, and
export `pynq_z1_top.xsa` (plus `pynq_z1_top.bit`) at the `Vivado/` root.

### 2. Linux image — Yocto (kas)

Prepare the host for Yocto:

- [Preparing the build host](https://docs.yoctoproject.org/singleindex.html#preparing-the-build-host)
- [System requirements (Scarthgap)](https://docs.yoctoproject.org/scarthgap/ref-manual/system-requirements.html#system-requirements)

Install `kas`:

```bash
pip install kas
```

Build:

```bash
cd yocto
make
```

`make` copies `../Vivado/pynq_z1_top.xsa` into the HDF recipe
(`HDF_URI = "file://pynq_z1_top.xsa"`) and runs `kas build build.yaml` (machine
`pynq-z1`, target `image-pynq-z1`). The SD-card image is the `.wic` file under
`yocto/build/tmp-glibc/deploy/images/pynq-z1/`
(e.g. `image-pynq-z1-pynq-z1.rootfs.wic`).

### 3. Flash & run

1. Flash the `.wic` image to a microSD card (e.g. with Balena Etcher).
2. Insert the card, connect the USB UART (115200 8N1), and power the board.
3. Log in as `root` and run:

```bash
/home/root/axi-gpio
```

Use the buttons to change the blink speed and RGB colours.

## Repository layout

```
.
├── Vivado/                  # Hardware: block design, VHDL top, constraints
│   ├── scripts/             #   create_project.tcl, pynq_z1_bd.tcl, build_bitstream.tcl
│   └── src/                 #   pynq_z1_top.vhd, pynq_z1.xdc
├── yocto/                   # Linux image: build.yaml + meta-user layer
│   └── meta-user/
│       ├── recipes-bsp/     #   device-tree, HDF, u-boot
│       ├── recipes-kernel/  #   kernel config
│       └── recipes-apps/    #   gpio-test recipe
└── apps/
    └── gpio-test/           # C++20 libgpiod demo
```

## References

- [AXI GPIO v2.0 — Product Guide (PG144)](https://docs.amd.com/r/en-US/pg144-axi-gpio)
- [PYNQ-Z1 Reference Manual](https://digilent.com/reference/programmable-logic/pynq-z1/reference-manual)
- [libgpiod](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/about/)
