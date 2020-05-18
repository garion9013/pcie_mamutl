CC := gcc

all: fpga_offload

fpga_offload: fpga_offload.c ctrl_register_read.c channel_readwrite.c device_check.c utils.c
	$(CC) -o $@ $^

clean:
	rm -rf fpga_offload

