/*
* Copyright (C) 2013 - 2016  Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or (b) that interact
* with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h> /* for strncpy */
#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>



#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <termios.h>

#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>


#ifdef DAEMON_MODE
#define __PRINT_LOG(...) syslog (LOG_NOTICE,__VA_ARGS__)
#else
#define __PRINT_LOG(...) printf (__VA_ARGS__)
#endif

#define ADDRESS_SI5324 0x68
#define ADDRESS_SI570 0x55

#define SEGMENT_SIZE 0x10000000
#define PAGE_SIZE ((size_t)getpagesize())
#define PAGE_MASK ((uint64_t)(long)~(PAGE_SIZE - 1))
#define REG_SIZE 4

void *R5550RegArea;
int iic_sys;

void *OpenMap()
{

    uint64_t offset, base;
    uint32_t value;
    volatile uint8_t *mm;
    offset = (uint64_t) 0x50000000;
    int fd;
    int cached = 0;
    fd = open("/dev/mem", O_RDWR|(!cached ? O_SYNC : 0));
    if (fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed (%d)\n", errno);
        return 1;
    }


    base = offset & PAGE_MASK;
    offset &= ~PAGE_MASK;

    mm = mmap(NULL, SEGMENT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (mm == MAP_FAILED) {
        fprintf(stderr, "mmap64(0x%x@0x%lx) failed (%d)\n",
                PAGE_SIZE, base, errno);
        return NULL;
    }

    return mm;
}



int CloseMap(void *mm)
{
    munmap((void *)mm, SEGMENT_SIZE);
}


inline int WriteReg(void *mm, uint32_t address, uint32_t v)
{
    if (address < (SEGMENT_SIZE>>2))
    {
        *(volatile uint32_t *)(mm + (address*4)) = v;
        return 0;
    }
    else
        return -1;
    
}

inline uint32_t ReadReg(void *mm, uint32_t address)
{
    if (address > (SEGMENT_SIZE>>2))
    {
        return -1;
    }
    return *(volatile uint32_t *)(mm + (address*4));

}


int WriteMem(void *mm, uint32_t address, uint32_t len, uint32_t *v)
{
    if (((address+len)*4) < (SEGMENT_SIZE))
    {
        memcpy(mm + (address*4), v, len*4);
        return 0;
    }
    else
    {

        return -1;
    }
    
}

int ReadMem(void *mm, uint32_t address, uint32_t len, uint32_t *v)
{
    if (((address+len)*4) < (SEGMENT_SIZE))
    {
        memcpy(v, mm + (address*4), len*4);
        return 0;
    }
    else
        return -1;

}



void iicSiWrite(uint8_t address, uint8_t int_address, uint8_t data)
{
	uint8_t value[2];
	if (ioctl(iic_sys, I2C_SLAVE, address) < 0) {
		__PRINT_LOG("iic_disp_erorr_on_write byte\n");
	}
		else
		{
			value[0] = int_address;
			value[1] = data;
			write(iic_sys, value, 2);
		}
}

uint8_t iicSiRead(uint8_t address, uint8_t int_address)
{
	uint8_t value[2];
	if (ioctl(iic_sys, I2C_SLAVE, address) < 0) {
		__PRINT_LOG("iic_disp_erorr_on_write byte\n");
	}
		else
		{
			value[0] = int_address;
            write(iic_sys, value, 1);
            read(iic_sys, value, 1);
            return value[0];
		}
		
	return 0;
}


uint8_t cfg_SI5324 [] = { 
  0, 0x14,
  1, 0xE4,
  2, 0xA2,
  3, 0x15,
  4, 0x92,
  5, 0xED,
  6, 0x2D,
  7, 0x2A,
  8, 0x00,
  9, 0xC0,
 10, 0x00,
 11, 0x40,
 19, 0x29,
 20, 0x3E,
 21, 0xFF,
 22, 0xDF,
 23, 0x1F,
 24, 0x3F,
 25, 0x00,
 31, 0x00,
 32, 0x00,
 33, 0x07,
 34, 0x00,
 35, 0x00,
 36, 0x07,
 40, 0x00,
 41, 0x02,
 42, 0x7F,
 43, 0x00,
 44, 0x00,
 45, 0x4F,
 46, 0x00,
 47, 0x00,
 48, 0x4F,
 55, 0x00,
 131, 0x1F,
 132, 0x02,
 137, 0x01,
 138, 0x0F,
 139, 0xFF,
 142, 0x00,
 143, 0x00,
 136, 0x40 

};

uint8_t cfg_SI570 [] = { 
    137, 0x10,
    13,  0x01, 
    14,  0xc2, 
    15,  0xc6, 
    16,  0x0f, 
    17,  0x4e, 
    18,  0xe9, 
    137,  0x00, 
    135,  0x40 
};

uint8_t sts_SI5324 [] = { 
    128,
    129,
    130,
    131,
    132
};
int main () {

    int old_value;
    int cached = 0;
    printf("STARTING DCRM DAEMON\n");
#ifdef DAEMON_MODE   
   pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    /* Open the log file */
    openlog ("DCRM", LOG_PID, LOG_DAEMON);
#endif    
    
    
    R5550RegArea = OpenMap();
    if (R5550RegArea==NULL)
    {
        printf("Error connecting daemon do AXI Bus. DCRM Daemon die\n");
        return -1;
    }

    iic_sys = open("/dev/i2c-0", O_RDWR);
    
    if (iic_sys<0)
    {
        printf("Unable to open i2c-0 device. DCRM Daemon die\n");
        return -2;
    }
    
    
    __PRINT_LOG("Configuring the SI570 device\nESS: programming SI570 via I2C...");
    for (int i =0; i<9; i++) {
        __PRINT_LOG("Writing: device [%2x]   address: %3d, data:%2x\n", ADDRESS_SI570, cfg_SI570[i*2], cfg_SI570[(i*2)+1]);
        iicSiWrite(ADDRESS_SI570, cfg_SI570[i*2], cfg_SI570[(i*2)+1]);
        usleep(1000);    
    }
    
    usleep(100000);
    __PRINT_LOG("\n\nConfiguring the SI5324 device\nESS: configuring SI5324 jitter cleaner to 158.4945 MHz.\n");
    for (int i =0; i<43; i++) {
        __PRINT_LOG("Writing: device [%2x]   address: %3d, data:%2x\n", ADDRESS_SI5324, cfg_SI5324[i*2], cfg_SI5324[(i*2)+1]);
        iicSiWrite(ADDRESS_SI5324, cfg_SI5324[i*2], cfg_SI5324[(i*2)+1]);   
    }    
    usleep(100000);
    
    while(1) {
        uint32_t data;
        __PRINT_LOG("ESS: getting SI5324 jitter cleaner status.\n");
        for (int i =0; i<5; i++) {
            data = iicSiRead(ADDRESS_SI5324, sts_SI5324[i]);
            __PRINT_LOG("   SI5324 -> REG: %3d : %2x\n", sts_SI5324[i], data);
            WriteReg(R5550RegArea, 0x03FFFFE0+i, data);
            usleep(1000);    
        }
        
        usleep(250000);
    }
    
    
    return 0;
}