# I2C PPG front-end (bring-up)

Production path: Linux `i2c-dev` (`/dev/i2c-1` on Raspberry Pi) with `ioctl` `I2C_RDWR` or `smbus` for register maps (e.g. MAX30102, AFE4404).

This repository wires the **simulated** `I2cPpgDevice` in `src/cpp/acquisition/` for CI and desktop development. Replace `read_burst` / `write_register` bodies with hardware calls and keep the same upper-layer acquisition API.
