#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device_check.h"

/* check whether xdma device is recognized by searching the list in /proc/devices */
int device_check(){

    printf("Making sure the xdma device is recognized...\n");	

    FILE *fd;
    fd = fopen("/proc/devices", "r");
    if (fd == NULL){
        printf("Error: failed to open /proc/devices\n");
        exit(1);
    }

    char strBuffer[1024];
    int success_flag = 0;
    while(fscanf(fd, "%s", strBuffer) != EOF){
        if (strcmp(strBuffer, "xdma") == 0){
            printf("xdma device is recognized!\n");
            ++success_flag;
        break;
        }
    }

    if (success_flag == 0){
        printf("Error: No xdma device is recognized!\n");
        exit(1);
    }

    return success_flag;
}
