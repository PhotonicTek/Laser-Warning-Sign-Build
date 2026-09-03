# Laser Warning Sign
---
![Main](/images/render_fx.png)

![Main](/images/laser_sign_rl.png)

---

## Background

I wanted a warning sign that's better than just a paper sign taped to the door. This repo contains everything needed to reproduce the sign and its matching remote: schematics, code, 3D model files, laser cutter files, a parts list, and image assets.

Video walkthrough of the design and build process: *[video to be uploaded]*

---

## Overview

The sign has two states, safe and danger, lit from behind by an LED strip through a diffuse layer, with laser-cut lettering and icon. It's fully battery-operated, wirelessly switched by a handheld remote, and mounts with command strips.

### Sign Electronics
- **ATtiny84** — main microcontroller, chosen for size, low-power sleep states, and flexible power input
- **nRF24** — wireless transceiver, receives state changes from the remote
- **AP7313** — low-dropout regulator, steps battery voltage down to keep the nRF24 under its 3.6V max
- **MT3608** — boost converter, steps battery voltage up to 5V for the LED strip
- **WS2812B (NeoPixel) LED strip** — backlighting for the lettering
- **TP4056** — battery management / charging
- **2x 18650 cells 3000mAh (parallel)** — ~6000mAh combined capacity; recycled cells were tested and verified to ~2780mAh
- **IRLML6402TRPBF** high-side MOSFET — cuts power to the boost converter and LEDs when the sign is off, to eliminate idle draw from that stage

### Remote Electronics
Same microcontroller and transceiver as the sign, with button inputs instead of an LED strip. Powers on when the power button is pressed and transmits the selected sign state.

### Power Notes
- Idle draw when duty-cycled (~1s sleep, brief wake to check for a signal, ~2.5% duty cycle): averages ~640µA, giving roughly a year of standby life
- With the sign on (LEDs lit): ~560mA, ~10 hours of runtime

The ATtiny84 doesn't have onboard USB, so it's flashed using an Arduino Uno as an ISP programmer.

![Main](/images/laser_sign_layout.png)

---

## Repository Contents

- Schematics are provided as source .json files plus image exports for viewing without a schematic program.
- 3D model files for the full sign and remote plus individual 3D-printed parts, all printing was done in PLA on a Bambu Lab P1S.
- LightBurn laser files are set up for 3mm hardboard. Additional DXF and SVG cut paths are also available.
- The BOM lists parts as used; substitutions are noted where relevant (see below).

---

## Substitution Notes

- Generic resistors, capacitors, and other passives can be substituted freely at the correct value/tolerance.
- The ATtiny84 and nRF24 modules are both common and easily sourced new. The nRF24 module comes in many shapes and sizes; the mini version was used in this project as it was what I had on hand.
- 18650 cells used here were recycled/harvested, capacity and health will vary if you're sourcing your own.

---

## Known Issues / Things to Watch For

- The boost converter causes a brief voltage drop on power-up that can brown out the microcontroller and cause the LED strip to briefly show garbage colors. A large capacitor on the boost converter output and a small capacitor across the MOSFET gate (to slow its turn-on) fixes this. Both fixes have been implemented and are shown on the schematics in this repo.
- Make sure to burn the bootloader on the ATtiny84 before programming, skipping this leaves the clock running at the wrong speed.
- Light leakage around the lettering in the dark may need internal dividers depending on your enclosure tolerances.

---

## Licence

Everything in this repository — schematics, code, models, cut files, BOM, and images, is free to use, modify, and build on, for any purpose.

**No support is provided.** This is a documentation dump, not a maintained project. I won't be providing build help, debugging, or answering setup questions, use it at your own discretion.

---

## AI Use Disclaimer

Portions of this documentation were drafted with the use of AI writing tools and reviewed for accuracy against the physical build. All technical content (circuit design, firmware, and component selection) came from the actual build process. If you spot an error, please open an issue.

---

## See Also

- Video walkthrough: *[video to be uploaded]*
- KiCad / EasyEDA (schematic tools): https://www.kicad.org / https://easyeda.com
- Krita (used for the initial sign mockup): https://krita.org
