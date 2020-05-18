#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "channel_readwrite.h"
#include "utils.h"

/* transferSize of data pointed by the data ptr will be written to the device at addr
 * returns total execution time of function 
 */
struct timespec write_to_channel(char *channelDevice, uint32_t addr, uint32_t transferSize, void* data){

    /* local variables */    
    int rc;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_start, ts_end;

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* allocate memory to buffer */
    posix_memalign((void **)&allocated, 4096/*alignment*/, transferSize + 4096);
    assert(allocated);
    buffer = allocated;
    //printf("host memory buffer = %p\n", buffer);

    /* first need to copy data to buffer */
    memcpy(buffer, data, transferSize);

    int fpga_fd = open(channelDevice, O_RDWR); 
    assert(fpga_fd >= 0);

    /* select AXI MM address */
    off_t off = lseek(fpga_fd, addr, SEEK_SET);

    /* Write data to the AXI MM address using SGDMA */
    rc = write(fpga_fd, buffer, transferSize);
    assert(rc == transferSize); // make sure that the entire data is written

    close(fpga_fd);
    free(allocated);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    timespec_sub(&ts_end, &ts_start);

    return ts_end;
}

/* transferSize of data at addr will be read from device to output ptr
 * returns total execution time of function
 */
struct timespec read_from_channel(char *channelDevice, uint32_t addr, uint32_t transferSize, void *output){

    /* local variables */
    int rc;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_start, ts_end;

    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    posix_memalign((void **)&allocated, 4096/*alignment*/, transferSize + 4096);
    assert(allocated);
    buffer = allocated;
    //printf("host memory buffer = %p\n", buffer);

    /* Open device */
    int fpga_fd = open(channelDevice, O_RDWR | O_NONBLOCK);
    assert(fpga_fd >= 0);

    /* zero-initialize buffer and read data from device */
    memset(buffer, 0x00, transferSize);
    off_t off = lseek(fpga_fd, addr, SEEK_SET);

    rc = read(fpga_fd, buffer, transferSize);
    if ((rc > 0) && (rc < transferSize)){
        printf("Short read of %d bytes into a %d bytes buffer, could be a packet read?\n", rc, transferSize);
    }

    /* copy data from buffer to output */
    memcpy(output, buffer, transferSize);

    close(fpga_fd);
    free(allocated);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    timespec_sub(&ts_end, &ts_start);

    return ts_end;
}

/* verbose version of write_to_channel with detailed time profiling 
 * returns actual write time without the overhead
 * NOTE: reported total execution time will be greater than actual total exeuction time
 *       because clock_gettime functions are inevitably included in time measurement
 */
struct timespec write_to_channel_verbose(char *channelDevice, uint32_t addr, uint32_t transferSize, void* data){

    /* local variables */    
    int rc;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_global_start, ts_global_end;
    struct timespec ts_mem_start, ts_mem_end, ts_open_start, ts_open_end, ts_close_start, ts_close_end, ts_cleanup_start, ts_cleanup_end;
    struct timespec ts_write_start, ts_write_end;

    clock_gettime(CLOCK_MONOTONIC, &ts_global_start);

    /* allocate memory to buffer */
    clock_gettime(CLOCK_MONOTONIC, &ts_mem_start);

    posix_memalign((void **)&allocated, 4096/*alignment*/, transferSize + 4096);
    assert(allocated);
    buffer = allocated;
    //printf("host memory buffer = %p\n", buffer);

    /* first need to copy data to buffer */
    memcpy(buffer, data, transferSize);

    clock_gettime(CLOCK_MONOTONIC, &ts_mem_end);

    clock_gettime(CLOCK_MONOTONIC, &ts_open_start);
    int fpga_fd = open(channelDevice, O_RDWR); 
    assert(fpga_fd >= 0);
    clock_gettime(CLOCK_MONOTONIC, &ts_open_end);

    /* select AXI MM address */
    off_t off = lseek(fpga_fd, addr, SEEK_SET);

    /* Write data to the AXI MM address using SGDMA */
    clock_gettime(CLOCK_MONOTONIC, &ts_write_start);

    rc = write(fpga_fd, buffer, transferSize);
    assert(rc == transferSize); // make sure that the entire data is written

    clock_gettime(CLOCK_MONOTONIC, &ts_write_end);

    clock_gettime(CLOCK_MONOTONIC, &ts_close_start);
    close(fpga_fd);
    clock_gettime(CLOCK_MONOTONIC, &ts_close_end);

    clock_gettime(CLOCK_MONOTONIC, &ts_cleanup_start);
    free(allocated);
    clock_gettime(CLOCK_MONOTONIC, &ts_cleanup_end);

    clock_gettime(CLOCK_MONOTONIC, &ts_global_end);

    timespec_sub(&ts_global_end, &ts_global_start);
    timespec_sub(&ts_mem_end, &ts_mem_start);
    timespec_sub(&ts_open_end, &ts_open_start);
    timespec_sub(&ts_write_end, &ts_write_start);
    timespec_sub(&ts_close_end, &ts_close_start);
    timespec_sub(&ts_cleanup_end, &ts_cleanup_start);


    printf("Profiling WRITE of %d bytes to Device...\n", transferSize);
    printf("total transfer time    : %ld.%09ld seconds\n", ts_global_end.tv_sec, ts_global_end.tv_nsec);
    printf("memory alloc. & align  : %ld.%09ld seconds\n", ts_mem_end.tv_sec, ts_mem_end.tv_nsec);
    printf("open file              : %ld.%09ld seconds\n", ts_open_end.tv_sec, ts_open_end.tv_nsec);
    printf("actual transfer(write) : %ld.%09ld seconds\n", ts_write_end.tv_sec, ts_write_end.tv_nsec);
    printf("close file             : %ld.%09ld seconds\n", ts_close_end.tv_sec, ts_close_end.tv_nsec);
    printf("cleanup                : %ld.%09ld seconds\n", ts_cleanup_end.tv_sec, ts_cleanup_end.tv_nsec);

    return ts_write_end;
}

/* verbose version of read_from_channel with detailed time profiling 
 * retuns actual read time without the overhead


struct timespec read_from_channel_verbose(char *channelDevice, uint32_t addr, uint32_t transferSize, void *output); * NOTE: reported total execution time will be greater than actual total exeuction time
 *       because clock_gettime functions are inevitably included in time measurement
 */
struct timespec read_from_channel_verbose(char *channelDevice, uint32_t addr, uint32_t transferSize, void *output){

    /* local variables */
    int rc;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_global_start, ts_global_end;
    struct timespec ts_mem_start, ts_mem_end, ts_open_start, ts_open_end, ts_close_start, ts_close_end, ts_cleanup_start, ts_cleanup_end;
    struct timespec ts_read_start, ts_read_end;

    clock_gettime(CLOCK_MONOTONIC, &ts_global_start);

    clock_gettime(CLOCK_MONOTONIC, &ts_mem_start);
    posix_memalign((void **)&allocated, 4096/*alignment*/, transferSize + 4096);
    assert(allocated);
    buffer = allocated;
    //printf("host memory buffer = %p\n", buffer);
    clock_gettime(CLOCK_MONOTONIC, &ts_mem_end);

    /* Open device */
    clock_gettime(CLOCK_MONOTONIC, &ts_open_start);
    int fpga_fd = open(channelDevice, O_RDWR | O_NONBLOCK);
    assert(fpga_fd >= 0);
    clock_gettime(CLOCK_MONOTONIC, &ts_open_end);

    /* zero-initialize buffer and read data from device */
    memset(buffer, 0x00, transferSize);
    off_t off = lseek(fpga_fd, addr, SEEK_SET);

    clock_gettime(CLOCK_MONOTONIC, &ts_read_start);
    rc = read(fpga_fd, buffer, transferSize);
    if ((rc > 0) && (rc < transferSize)){
        printf("Short read of %d bytes into a %d bytes buffer, could be a packet read?\n", rc, transferSize);
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_read_end);

    /* copy data from buffer to output */
    memcpy(output, buffer, transferSize);

    /* subtract the start time from the end time */
    timespec_sub(&ts_global_end, &ts_global_start);
    /* display passed time, a bit less accurate but side-effects are accounted for */

    clock_gettime(CLOCK_MONOTONIC, &ts_close_start);
    close(fpga_fd);
    clock_gettime(CLOCK_MONOTONIC, &ts_close_end);

    clock_gettime(CLOCK_MONOTONIC, &ts_cleanup_start);
    free(allocated);
    clock_gettime(CLOCK_MONOTONIC, &ts_cleanup_end);

    clock_gettime(CLOCK_MONOTONIC, &ts_global_end);

    timespec_sub(&ts_global_end, &ts_global_start);
    timespec_sub(&ts_mem_end, &ts_mem_start);
    timespec_sub(&ts_open_end, &ts_open_start);
    timespec_sub(&ts_read_end, &ts_read_start);
    timespec_sub(&ts_close_end, &ts_close_start);
    timespec_sub(&ts_cleanup_end, &ts_cleanup_start);


    printf("Profiling READ of %d bytes to Device...\n", transferSize);
    printf("total transfer time    : %ld.%09ld seconds\n", ts_global_end.tv_sec, ts_global_end.tv_nsec);
    printf("memory alloc. & align  : %ld.%09ld seconds\n", ts_mem_end.tv_sec, ts_mem_end.tv_nsec);
    printf("open file              : %ld.%09ld seconds\n", ts_open_end.tv_sec, ts_open_end.tv_nsec);
    printf("actual transfer(read)  : %ld.%09ld seconds\n", ts_read_end.tv_sec, ts_read_end.tv_nsec);
    printf("close file             : %ld.%09ld seconds\n", ts_close_end.tv_sec, ts_close_end.tv_nsec);
    printf("cleanup                : %ld.%09ld seconds\n", ts_cleanup_end.tv_sec, ts_cleanup_end.tv_nsec);

    return ts_read_end;
}

