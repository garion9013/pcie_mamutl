#include <stdint.h>
#include <time.h>

struct timespec write_to_channel(char *channelDevice, uint32_t addr, uint32_t transferSize, void* data);

struct timespec read_from_channel(char *channelDevice, uint32_t addr, uint32_t transferSize, void *output);

struct timespec write_to_channel_verbose(char *channelDevice, uint32_t addr, uint32_t transferSize, void *data);

struct timespec read_from_channel_verbose(char *channelDevice, uint32_t addr, uint32_t transferSize, void *output);
