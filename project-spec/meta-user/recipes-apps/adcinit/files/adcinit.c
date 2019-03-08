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

#define ADC_CAL_BA 			0x4FFF0000
#define ADC_CAL_REG_DELAY		0x0
#define ADC_CAL_REG_CFG			0x4
#define ADC_CAL_REG_CHSEL		0xC
#define ADC_CAL_REG_ERRRESET		0x10
#define ADC_CAL_REG_ERR_A		0x18
#define ADC_CAL_REG_ERR_B		0x1C
#define ADC_CAL_REG_SIG_A		0x20
#define ADC_CAL_REG_SIG_B		0x24
#define ADC_CAL_REG_SIG			0x28
#define ADC_CAL_REG_SPISTS		0x2C

#define ADC_CAL_REG_PM_A		0x30
#define ADC_CAL_REG_PM_B		0x34

#define ADC_CAL_RESET			0x38


#define ADC_CAL_REG_SPISEL		0x78
#define ADC_CAL_REG_SPI			0x7C

#define ADC_CAL_SPI_DELAY		10000
#define ADC_CAL_SPI_STARTBIT		0x80000000
#define ADC_CAL_CHANNEL_A		0x000
#define ADC_CAL_CHANNEL_B		0x100
#define ADC_CAL_PROGDELAY		0x1
#define ADC_CAL_BITSLEEP		0x2

#define MAX_ERRORBIT 0

#include <stdio.h>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define PAGE_SIZE ((size_t)getpagesize())
#define PAGE_MASK ((uint64_t)(long)~(PAGE_SIZE - 1))

bool DO_ADC_TEST=false;
int LEN_ADC_TESTms=10;

void ADCSPIWrite(int fd, uint32_t reg);
void SetDelay(int fd, int delay);
int countErrors(int fd, int AB, int inttime);
void DoBitslip(int fd);
int countAlignError(int fd, int AB,  int inttime);
int ReadData(int fd, int AB);
int  PerformTestRamp(int fd, int channel);
int WriteReg(int fd, uint32_t address, uint32_t v);
int ReadReg(int fd, uint32_t address, uint32_t *v);
int ADCCalibration();

int main(int argc, char **argv)
{
	if (argc==2)
	{
		DO_ADC_TEST = true;
		LEN_ADC_TESTms = atoi(argv[1]);  
	}

    ADCCalibration();

    return 0;
}


	int rollin=10;
bool channelLockedBit[64];
int channelBitDelay[64];
bool channelAlligned[64];
int channelBitslipN[64];
//regFGC bit 0 progdelat bit 1 bitslip
//regSPI bit 31 start. deve commutare

void ADCSPIWrite(int fd, uint32_t reg)
{
	uint32_t data;
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_SPI, ADC_CAL_SPI_STARTBIT+  reg);		//ADC
	usleep(10);
	do
	{	
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_SPISTS, &data);	
	}while(data & 0xFF != 0);

	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_SPI, 0);
	usleep(10);
	//
}


void SetDelay(int fd, int delay)
{
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_DELAY, delay);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_CFG, ADC_CAL_PROGDELAY);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_CFG, 0);
}

int countErrors(int fd, int AB, int inttime)
{
	int j;
	int err_cnt=0;
	uint32_t data;
//	int ADDR = (AB==0 ? ADC_CAL_REG_SIG_A : ADC_CAL_REG_SIG_B);
	usleep(1000);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 0);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 1);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 0);
	usleep(inttime*1000);
	if (AB==0)
	{
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERR_A, &data);	
	}
	else
	{
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERR_B, &data);	
	}

	/*for (j=0;j<4096;j++)
	{
		ReadReg(fd, ADC_CAL_BA+ ADDR, &data);	
		if ((data!=0x55) && (data!=0xAA))				
			err_cnt++;
	}*/
	err_cnt = data;
	return err_cnt;
}

void DoBitslip(int fd)
{
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_CFG, ADC_CAL_BITSLEEP);
	usleep(1000);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_CFG, 0);
	usleep(1000);
}


int countAlignError(int fd, int AB,  int inttime)
{
	/*int j;
	int err_cnt=0;
	uint32_t data;
	int ADDR = (AB==0 ? ADC_CAL_REG_SIG_A : ADC_CAL_REG_SIG_B);
	for (j=0;j<512;j++)
	{
		ReadReg(fd, ADC_CAL_BA+ ADDR, &data);	
		if (data!=0xF0)
			err_cnt++;
	}*/

	int j;
	int err_cnt=0;
	uint32_t data;

	usleep(1000);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 0);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 1);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 0);
	usleep(inttime*1000);
	if (AB==0)
	{
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERR_A, &data);	
	}
	else
	{
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERR_B, &data);	
	}

	/*for (j=0;j<4096;j++)
	{
		ReadReg(fd, ADC_CAL_BA+ ADDR, &data);	
		if ((data!=0x55) && (data!=0xAA))				
			err_cnt++;
	}*/
	err_cnt = data;

	return err_cnt;
}

int ReadData(int fd, int AB)
{
	int j;
	int err_cnt=0;
	uint32_t data;
	int ADDR = (AB==0 ? ADC_CAL_REG_SIG_A : ADC_CAL_REG_SIG_B);

		ReadReg(fd, ADC_CAL_BA+ ADDR, &data);	

	return data;
}

int  PerformTestRamp(int fd, int channel)
{
	int TR=0;
	int err_cnt=0;
	int i;
	uint32_t data;
	ADCSPIWrite(fd,0x000D08);
	for (i=0;i<16384;i++)
	{
		uint32_t dato = i << 2;
		ADCSPIWrite(fd,0x001900 + ((dato>>0)&0xFF));
		ADCSPIWrite(fd,0x001A00 + ((dato>>8)&0xFF));
		ADCSPIWrite(fd,0x001B00 + ((dato>>0)&0xFF));
		ADCSPIWrite(fd,0x001C00 + ((dato>>8)&0xFF));

		WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_PM_A, ((dato>>0)&0xFF));
		WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_PM_B, ((dato>>8)&0xFF));

		ChannelSelect(fd, channel, 0);

		WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 0);
		WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 1);
		WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERRRESET, 0);
		usleep(1000 * LEN_ADC_TESTms);
		err_cnt = 0;
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERR_A, &data);	
		err_cnt += data;
		ReadReg(fd, ADC_CAL_BA+ ADC_CAL_REG_ERR_B, &data);	
		err_cnt += data;

		if (i%32)
		{
			printf("Progress: %3.2f%%\r",i/16384.0*100.0);
			fflush(stdout);
		}
		if(err_cnt>0)
		{
			printf("\n\rRAMP: %2d %4X[%4X] %d\n\r",channel, i, dato, err_cnt);	
			TR++;
		}
	}
			printf("\n\r");
	return TR;
}


void ChannelSelect(int fd, int ch, int AB)
{
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_CHSEL, ch + (ADC_CAL_CHANNEL_B*AB));
}

int ADCCalibration()
{
	int fd;
	int i,j;
	int ch_sel;
	int ab=0;
	int chh;
    int cached = 0;
	uint32_t data;
	int err_cnt=0;
	int delay;
	printf("**************************************************\n");
	printf("*                                                *\n");
	printf("*			ADC CALIBRATION PROCEDURE	    	 *\n");
	printf("*                                                *\n");
	printf("**************************************************\n");
	printf("opening /dev/mem\n");


	printf("Start Acquisiton Function (IN)\n");
	rollin=5;

    fd = open("/dev/mem", O_RDWR|(!cached ? O_SYNC : 0));
    if (fd < 0) {
        fprintf(stderr, "open(/dev/mem) failed (%d)\n", errno);
        return 1;
    }


	//RESET ADC
	ADCSPIWrite(fd,0x000018);		
	ADCSPIWrite(fd,0x00001C);		
	usleep(100000);	
	ADCSPIWrite(fd,0x000018);		
	usleep(100000);	

	ADCSPIWrite(fd,0x000800);		
	ADCSPIWrite(fd,0x000803);		
	usleep(100000);	
	ADCSPIWrite(fd,0x000800);		
	usleep(100000);	

	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_RESET, 0x3);
	usleep(1000);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_RESET, 0x1);
	usleep(1000);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_RESET, 0x0);
	usleep(100000);
	for (i=0;i<64;i++)
	{
		channelAlligned[i] =false;
	}

AlgndCNTERR:
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_SPISEL, 0);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_SPI, 0);


	ADCSPIWrite(fd,0x000D08);		//pattern mode: custom
	ADCSPIWrite(fd,0x001400);


	ADCSPIWrite(fd,0x002124);		//mobalità bitwise
	ADCSPIWrite(fd,0x0019CC);
	ADCSPIWrite(fd,0x001ACC);
	ADCSPIWrite(fd,0x001BCC);
	ADCSPIWrite(fd,0x001CCC);

	usleep(100000);

	//Program pattern matching pattern
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_PM_A, 0x55AA);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_PM_B, 0x55AA);

//	ch_sel=0;

	printf("Starting bit delay calibration...\n");


	for (ch_sel=0;ch_sel<32;ch_sel++)
	{
		for (ab=0;ab<2;ab++)
		{
		
		if (channelAlligned[(ch_sel*2)+ab] ==false)
		{
			ChannelSelect(fd, ch_sel, ab);

			
			int try=0;
  			int ScanWin[32];
		retry:

				for (i=0;i<32;i++)
				{
					SetDelay(fd,i);
					err_cnt = countErrors(fd, ab, 5+try*30);
					ScanWin[i] = err_cnt;
					data=ReadData(fd, ab);	
					//printf("    -> %2d    %d\n",i,err_cnt);
					printf("[->] CH: %2d%s Delay: %d Error: %6d Code:%2X                      \r",ch_sel, ab==0?"A":"B", i, err_cnt, data);
					fflush(stdout);
				}
			
				printf("\n0|");				
				int lstartM=0;
				int lendM=0;
				int lw=0;
				int llstart =0;
				int llw =0;
				bool llb = false;
				for (i=0;i<32;i++)
				{
					if (i==15)printf(" ");
					if (ScanWin[i]==0)
					{
						printf(".");
						if (llb==false)
						{
							llstart=i;
							llb = true;
							llw=0;
						}
						else
							llw++;
					}
					else
					{
						printf("#");
						if (llb==true)
						{
							if (llw>lw)
							{
								lstartM=llstart;
								lendM = i-1;
								lw=llw;
							}
						}
						llb=false;
					}
				}

				if (llb)
				{
					if (llw>lw)
					{
						lstartM=llstart;
						lendM = 31;
						lw=llw;
					}
				}

				printf("|31\n");				

			if (lendM - lstartM>10)
			{
				if (lendM<31)
					delay=lendM-4;
				else
					if (lstartM>0)
						delay=lendM+4;
					else
						delay = 17;
			}
			else
				delay = (lstartM + lendM)/2;
//
//			delay = 5;
			SetDelay(fd,delay);
			err_cnt = countErrors(fd, ab, 250);

			channelBitDelay[(ch_sel*2)+ab] = delay;
			channelLockedBit[(ch_sel*2)+ab] = err_cnt>MAX_ERRORBIT?false:true;

				if (channelLockedBit[(ch_sel*2)+ab] == false)
				{
					printf ("   --> [%x] --> delay found: %2d    error@delay: %8d  [MIN:%2d   MAX:%2d    W:%2d]\n",delay,err_cnt, lstartM,lendM,lw);

					if (try < 5)
					{
						try++;
						goto retry;
					}
				}

				

			printf("CH: %2d%s Delay: %d LOCKED: %s  [MIN:%2d   MAX:%2d    W:%2d]                           \r\n",ch_sel, ab==0?"A":"B", delay, channelLockedBit[(ch_sel*2)+ab]?"Y":"F",lstartM,lendM,lw);

		}
		}

	}

	int notAllignedCNT=0;

	printf("Bit delay calibration completed...\n");

	usleep(100000);
	printf("Starting bitslip word alignment...\n");

	ADCSPIWrite(fd,0x002134);		//mobalità bitwise
	ADCSPIWrite(fd,0x0019F0);
	ADCSPIWrite(fd,0x001AF0);
	ADCSPIWrite(fd,0x001BF0);
	ADCSPIWrite(fd,0x001CF0);
	usleep(100000);

	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_PM_A, 0xF0F0);
	WriteReg(fd, ADC_CAL_BA+ ADC_CAL_REG_PM_B, 0xF0F0);

	for (ch_sel=0;ch_sel<32;ch_sel++)
	{
		for (ab=0;ab<2;ab++)
		{
			channelAlligned[(ch_sel*2)+ab] =false;
			channelBitslipN[(ch_sel*2)+ab] =0;
			ChannelSelect(fd, ch_sel, ab);
			for (i=0;i<64;i++)
			{
				int err_cnt=0;
				DoBitslip(fd);
				err_cnt = countErrors(fd, ab, 1);//countAlignError(fd, ab);	
				data=ReadData(fd, ab);	
				printf("[B] CH: %2d%s Bitslip: %d CODE: %2X                      \r",ch_sel, ab==0?"A":"B", i, data);
				fflush(stdout);

				if (err_cnt==0)
				{
					channelAlligned[(ch_sel*2)+ab] =true;
					channelBitslipN[(ch_sel*2)+ab]=i;
					break;
				}
			}
			if(channelAlligned[(ch_sel*2)+ab] ==false)
				notAllignedCNT++;

			printf("\n[B] CH: %2d%s BITSLIP: %d ALLIGNED: %s\r\n",ch_sel, ab==0?"A":"B", channelBitslipN[(ch_sel*2)+ab], channelAlligned[(ch_sel*2)+ab] == true ? "Y":"N");

		}

	}

		if (notAllignedCNT>0)
			goto AlgndCNTERR;
	printf("Calibration process completed...\n");

	printf("------------------------------------------------ \n");
	printf("| CH   | BIT LOCKED | DELAY  | ALIGNED | SHIFT | \n");
	for (i=0;i<64;i++)
	printf("| %2d%s  |     %s      |   %2d   |    %s    |   %2d  | \n", i/2, i%2==0?"A":"B", channelLockedBit[i]== true ? "Y":"N", channelBitDelay[i], channelAlligned[i] == true ? "Y":"N", channelBitslipN[i]);
	printf("------------------------------------------------ \n");

	ADCSPIWrite(fd,0x000D00);		//pattern mode: custom

	printf("ADC Enabled...\n");

	if (DO_ADC_TEST==true)
	{
		int CH_ERROR[32];
		for (i=0;i<32;i++)
		{
			printf("- performing RAMP test on channel: %d\n",i);
			CH_ERROR[i] = PerformTestRamp(fd,i);
		}
		printf("------------------------------------------------ \n");
		printf("| CH   | TEST PASS  |  ERRORS  |       |       | \n");
		for (i=0;i<32;i++)
		printf("| %2d    |     %s      | %8d|        |        | \n", i, CH_ERROR[i] ==0 ? "Y":"N", CH_ERROR[i]);
		printf("------------------------------------------------ \n");

	}
	close(fd);

}





int WriteReg(int fd, uint32_t address, uint32_t v)
{
    uint64_t offset, base;
	uint32_t value;
    volatile uint8_t *mm;
	offset = (uint64_t) address;
    value = v;

    base = offset & PAGE_MASK;
    offset &= ~PAGE_MASK;
//printf("write reg %x value: %d\n", address, value);

    mm = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (mm == MAP_FAILED) {
        fprintf(stderr, "mmap64(0x%x@0x%lx) failed (%d)\n",
                PAGE_SIZE, base, errno);
        return 1;
    }
//printf("map ok\n");
    *(volatile uint32_t *)(mm + offset) = value;
    munmap((void *)mm, PAGE_SIZE);
	return 0;
}

int ReadReg(int fd, uint32_t address, uint32_t *v)
{
    uint64_t offset, base;
    volatile uint8_t *mm;
	offset = (uint64_t) address;


    base = offset & PAGE_MASK;
    offset &= ~PAGE_MASK;

//printf("read reg %x base:%x offset:%d\n", address, base, offset);

    mm = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (mm == MAP_FAILED) {
        fprintf(stderr, "mmap64(0x%x@0x%lx) failed (%d)\n",
                PAGE_SIZE, base, errno);
        return 1;
    }
//printf("map ok\n");
    *v = *(volatile uint32_t *)(mm + offset);
    munmap((void *)mm, PAGE_SIZE);
	return 0;
}

