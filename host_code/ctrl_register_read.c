#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/mman.h>

#include "ctrl_register_read.h"

/* ltoh: little to host */
/* htol: little to host */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#  define ltohl(x)       (x)
#  define ltohs(x)       (x)
#  define htoll(x)       (x)
#  define htols(x)       (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#  define ltohl(x)     __bswap_32(x)
#  define ltohs(x)     __bswap_16(x)
#  define htoll(x)     __bswap_32(x)
#  define htols(x)     __bswap_16(x)
#endif
  
#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)
 
#define MAP_SIZE (32*1024UL)
#define MAP_MASK (MAP_SIZE - 1)

#define H2C_REG 0x0000
#define C2H_REG 0x1000

/* check xdma control registers and find available H2C/C2H channels
 * 
 * base H2C address: 0x0000 (H2C: Host to Card)
 * base C2H address: 0x0100 (C2H: Card to Host)
 * offsets: 0x0000, 0x0100, 0x0200, 0x0300 - H2C/C2H can each have upto 4 channels
 * 
 * if a channel is enabled, then the first three hexa digits of control register are set as "1fc"
 */
int check_channels(off_t target_addr){

    /* local variables */
    int fd; // file descriptor
    void *map_base, *virt_addr;
    off_t target = target_addr; // base address to start
    off_t offset = 0x0000; // offset will be incremented by 0x0100
    uint32_t read_result;

    int num_ch = 0; // number of channels

    /* Open xdma control device */
    if ((fd = open("/dev/xdma0_control", O_RDWR | O_SYNC)) == -1){
        FATAL;
    }
    printf("xdma control device opened.\n"); 
    fflush(stdout);

    /* map one page */
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_base == (void *) -1){
        FATAL;
    }
    printf("memory mapped at address %p.\n", map_base); 
    fflush(stdout);

    /* loop over offset from 0x0000 to 0x0300, i.e. search upto 4 H2C/C2H channels */
    for (offset; offset < 0x0400; offset +=0x0100){
        virt_addr = map_base + target + offset;
    
        read_result = *((uint32_t *) virt_addr);
        /* swap 32-bit endianess if host is not little-endian */
        read_result = ltohl(read_result);
        printf("Read 32-bit value at address 0x%08x (%p): 0x%08x\n", (unsigned int)target, virt_addr, (unsigned int)read_result);

        /* check if the channel is enabled or not */
        if ((read_result & 0xfff00000) == 0x1fc00000){
            ++num_ch;
        }
    }

    /* cleanup */
    if (munmap(map_base, MAP_SIZE) == -1){
        FATAL;
    }
    close(fd);

    return num_ch;
}

int check_h2c_channels(){
    return check_channels(H2C_REG);
}

int check_c2h_channels(){
    return check_channels(C2H_REG);
}
