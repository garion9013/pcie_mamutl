# Host-to-FPGA Offloading

## host_code
This directory contains master version of host code for Host-to-FPGA offloading.

## File Description
* `channel_readwrite.c`: functions for reading and writing from/to HW logic
* `ctrl_register_read.c`: functions for checking number of enabled H2C and C2H channels by reading xdma control register values
* `device_check.c`: function for checking whether the device is recognized by host PC
* `fpga_offload.c`: functions for offloading matrix multiplications to FPGA. main function performs various functionality tests
* `utils.c`: utility functions which include reference cpu code and time keeping functions

## Overall WorkFlow of Host Code
1. Load device driver for PCIe DMA IP
2. Check DMA control registers to find out the number of enabled H2C(Host to Card) and C2H(Card to Host) channels
3. Write input to BRAM via H2C channel(s) 
4. Trigger HW logic by sending op code
5. Read output from BRAM via C2H channel(s)
