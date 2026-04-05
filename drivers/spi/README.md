# SPI PPG / ADC

Use `/dev/spidevB.C` with `SPI_IOC_MESSAGE` for high-speed ADC streams. The `SpiPpgDevice` class provides a FIFO simulation; map `transfer()` to real full-duplex frames per your datasheet.
