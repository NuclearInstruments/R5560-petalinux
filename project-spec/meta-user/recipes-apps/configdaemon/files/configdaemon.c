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
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h> /* for strncpy */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include "cJSON.h"

#define SEGMENT_SIZE 0x1000
#define PAGE_SIZE ((size_t)getpagesize())
#define PAGE_MASK ((uint64_t)(long)~(PAGE_SIZE - 1))
#define REG_SIZE 4

#define REG_OFS  32
#define REG_ETHFLAG 0x0
#define REG_SAVED_IP 0x1
#define REG_SAVED_NM 0x2
#define REG_SAVED_GW 0x3
#define REG_SAVED_DNS1 0x4
#define REG_MAC1 0x5
#define REG_MAC2 0x6
#define REG_ADC_INFO1 0x7
#define REG_ADC_INFO2 0x8
#define REG_LIVE_IP 0x9
#define REG_BOARDPOSITION 0xE
#define REG_OSVERSION 0x13
#define REG_OSRELEASE 0x14
#define REG_OSSTATUS 0x15

#define UPDT_DO 0x0
#define UPDT_IP 0x1
#define UPDT_NM 0x2
#define UPDT_GW 0x3
#define UPDT_DNS 0x4
#define UPDT_DO_USB 0x5
#define UPDT_DO_IP 0x6

inline uint32_t IICReg(uint32_t REG)
{
    return (REG_OFS+REG);
}

void *R5550RegArea;
void *OpenMap();
int CloseMap();
inline uint32_t ReadReg(void *mm, uint32_t address);
inline int WriteReg(void *mm, uint32_t address, uint32_t v);
uint32_t ReadFIFO(void *mm, uint32_t address, uint32_t status_address, uint32_t len, int timeout_ms, uint32_t *v, uint32_t *word_read, bool blocking);
int WriteMem(void *mm, uint32_t address, uint32_t len, uint32_t *v);
int ReadMem(void *mm, uint32_t address, uint32_t len, uint32_t *v);


bool SetEnumIfExist(cJSON *json, const char *field, int * value, const char *list_of_values [], int cnt );
bool SetStringIfExist(cJSON *json, const char *field, char * value );
bool SetDoubleIfExist(cJSON *json, const char *field, double * value );
bool SetIntegerIfExist(cJSON *json, const char *field, int * value );
bool SetBoolIfExist(cJSON *json, const char *field, bool * value );
void saveSettings();
bool loadEthSettings();
void UpdateNetwork(int c, char *ip, char *nm, char *gw, char *dns, int board_position);

int CheckLink(char *ifname);

char settings_eth_path[29]= "/mnt/sdcard/eth.json";
char interface_path[24] = "/etc/network/interfaces";

int BOARDPOSITION=0;

typedef struct {
    char ip[32];
    char nm[32];
    char gw[32];
    char dns[32];
    bool dhcp;
} t_eth_cfg;

t_eth_cfg currentCfg;
t_eth_cfg toBeApplyCfg;

typedef struct{
    char *iface;
    struct ether_addr hwa;
    struct in_addr ipa;
    struct in_addr bcast;
    struct in_addr nmask;
    u_short mtu;
} ifcfg_t;
/*
 * Grabs local network interface information and stores in a ifcfg_t
 * defined in network.h, returns 0 on success -1 on failure
*/
int get_local_info(int rsock, ifcfg_t *ifcfg)
{
    struct ifreq ifr;



    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifcfg->iface, IF_NAMESIZE);
    if((ioctl(rsock, SIOCGIFHWADDR, &ifr)) == -1){
        //perror("ioctl():");
        return -1;
    }
    memcpy(&(ifcfg->hwa), &ifr.ifr_hwaddr.sa_data, 6);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifcfg->iface, IF_NAMESIZE);
    if((ioctl(rsock, SIOCGIFADDR, &ifr)) == -1){
        //perror("ioctl():");
        return -1;
    }
    memcpy(&ifcfg->ipa, &(*(struct sockaddr_in *)&ifr.ifr_addr).sin_addr, 4);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifcfg->iface, IF_NAMESIZE);
    if((ioctl(rsock, SIOCGIFBRDADDR, &ifr)) == -1){
        //perror("ioctl():");
        return -1;
    }
    memcpy(&ifcfg->bcast, &(*(struct sockaddr_in *)&ifr.ifr_broadaddr).sin_addr, 4);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifcfg->iface, IF_NAMESIZE);
    if((ioctl(rsock, SIOCGIFNETMASK, &ifr)) == -1){
        //perror("ioctl():");
        return -1;
    }
    memcpy(&ifcfg->nmask.s_addr, &(*(struct sockaddr_in *)&ifr.ifr_netmask).sin_addr, 4);

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifcfg->iface, IF_NAMESIZE);
    if((ioctl(rsock, SIOCGIFMTU, &ifr)) == -1){
        //perror("ioctl():");
        return -1;
    }
    ifcfg->mtu = ifr.ifr_mtu;

    return 0;
}

int main(int argc, char **argv)
{
    bool res;
    uint32_t flags;
    struct in_addr tempIP;
    uint32_t temp_data;
    ifcfg_t currentIP;
    currentIP.iface =malloc(32);
    sprintf(currentIP.iface,"eth0");
    int rsock = socket(AF_INET, SOCK_DGRAM, 0);
    get_local_info(rsock, &currentIP);
    printf("IP: %s\n", inet_ntoa(currentIP.ipa));
    printf("NM: %s\n", inet_ntoa(currentIP.nmask));
    printf("HW: %s\n", ether_ntoa(&currentIP.hwa));

    sprintf(currentCfg.ip,"1.2.3.4");
    sprintf(currentCfg.nm,"6.7.8.9");
    sprintf(currentCfg.gw,"A.B.C.D");
    sprintf(currentCfg.dns,"10.11.12.13");


    R5550RegArea = OpenMap();
    if (R5550RegArea==NULL)
    {
        printf("Error connecting daemon do AXI Bus. Daemon die\n");
        return -1;
    }


    BOARDPOSITION = ReadReg(R5550RegArea, REG_BOARDPOSITION);
    printf("DAQ Board position on the R5560: %d\n", BOARDPOSITION);


    printf("Configuration loaded from file\n");
    printf("------------------------------\n");
    res = loadEthSettings();
    if (res!=false) {
        printf("IP:   %s\n", currentCfg.ip);
        printf("NM:   %s\n", currentCfg.nm);
        printf("GW:   %s\n", currentCfg.gw);
        printf("DNS:  %s\n", currentCfg.dns);
    }
    else
    {
        printf("Invalid configuration file\n");
    }
    printf("\n");
    printf("\n");

    ReadReg(R5550RegArea, UPDT_DO);
    while(1)
    {

        flags=0;
        usleep(100000);

        res = loadEthSettings();
        flags += ((res ? 1:0)<<3);

        get_local_info(rsock, &currentIP);

        WriteReg(R5550RegArea, IICReg(REG_LIVE_IP), currentIP.ipa.s_addr);


        inet_pton(AF_INET, currentCfg.ip, &(tempIP));
        WriteReg(R5550RegArea, IICReg(REG_SAVED_IP), tempIP.s_addr);

        inet_pton(AF_INET, currentCfg.nm, &(tempIP));
        WriteReg(R5550RegArea, IICReg(REG_SAVED_NM), tempIP.s_addr);

        inet_pton(AF_INET, currentCfg.gw, &(tempIP));
        WriteReg(R5550RegArea, IICReg(REG_SAVED_GW), tempIP.s_addr);

        inet_pton(AF_INET, currentCfg.dns, &(tempIP));
        WriteReg(R5550RegArea, IICReg(REG_SAVED_DNS1), tempIP.s_addr);

        flags += currentCfg.dhcp ? 1:0;
        flags += (CheckLink("eth0") != 0 ? 1:0)<<2;
        WriteReg(R5550RegArea, IICReg(REG_ETHFLAG), flags);


        temp_data = ReadReg(R5550RegArea, UPDT_DO);
        if (temp_data!=0)
        {
            int c;
            printf("Update eth settings\n");
            c = (temp_data == 1) ? 0:1;
            toBeApplyCfg.dhcp = c;
            printf("DHCP is: %d\n");
            temp_data = ReadReg(R5550RegArea, UPDT_IP);
            tempIP.s_addr = temp_data;  strcpy(toBeApplyCfg.ip, inet_ntoa(tempIP)); printf("IP:   %8x [%s]\n", temp_data, toBeApplyCfg.ip);
            temp_data = ReadReg(R5550RegArea, UPDT_NM);
            tempIP.s_addr = temp_data; strcpy(toBeApplyCfg.nm, inet_ntoa(tempIP));  printf("NM:   %8x [%s]\n", temp_data, toBeApplyCfg.nm);
            temp_data = ReadReg(R5550RegArea, UPDT_GW);
            tempIP.s_addr = temp_data; strcpy(toBeApplyCfg.gw, inet_ntoa(tempIP));  printf("GW:   %8x [%s]\n", temp_data, toBeApplyCfg.gw);
            temp_data = ReadReg(R5550RegArea, UPDT_DNS);
            tempIP.s_addr = temp_data; strcpy(toBeApplyCfg.dns, inet_ntoa(tempIP)); printf("DNS:  %8x [%s]\n", temp_data, toBeApplyCfg.dns);
            saveSettings();
            UpdateNetwork(toBeApplyCfg.dhcp, toBeApplyCfg.ip, toBeApplyCfg.nm, toBeApplyCfg.gw, toBeApplyCfg.dns, BOARDPOSITION);
        }
    }
    return 0;
}




int CheckLink(char *ifname) {
    int state = -1;
    int socId = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socId < 0) return -1;

    struct ifreq if_req;
    (void) strncpy(if_req.ifr_name, ifname, sizeof(if_req.ifr_name));
    int rv = ioctl(socId, SIOCGIFFLAGS, &if_req);
    close(socId);

    if ( rv == -1) return -1;

    return (if_req.ifr_flags & IFF_UP) ;
}


void UpdateNetwork(int c, char *ip, char *nm, char *gw, char *dns, int board_position)
{
    int len= sizeof(gw);
    char *nw_tmp= malloc(sizeof(char)*len);
    char *nw = malloc(sizeof(char)*len);
    char outBuffer[150];

    FILE *file;
    file = fopen("/tmp/interfaces", "wb");

    system("mkdir /mnt/sdcard/etc/\n");
    printf("Update and restart interfaces\n");
    if (c==0)
    {
        strcpy(nw_tmp, gw);
        char *array[4];
        int i=0;
        array[i] = strtok (nw_tmp, ".");

        while(array[i]!=NULL)
        {
            array[++i] = strtok(NULL,".");
        }

        sprintf(nw, "%s.%s.%s.%s", array[0],array[1],array[2], "0");
        sprintf(outBuffer,"iface eth0 inet static\n address %s\n netmask %s\n network %s\n gateway %s\n",ip ,nm,nw,gw );



        fputs("# /etc/network/interfaces -- configuration file for ifup(8), ifdown(8)\n",file);
        fputs("# The loopback interface\n",file);
        fputs("auto lo\n",file);
        fputs("iface lo inet loopback\n",file);
        fputs("# Wireless interfaces\n",file);
        fputs("iface wlan0 inet dhcp\n",file);
        fputs(" wireless_mode managed\n",file);
        fputs(" wireless_essid any\n",file);
        fputs(" wpa-driver wext\n",file);
        fputs(" wpa-conf /etc/wpa_supplicant.conf\n",file);
        fputs("\n",file);
        fputs("iface atml0 inet dhcp\n",file);
        fputs("\n",file);
        fputs("# Wired or wireless interfaces\n",file);
        fputs("auto eth0\n",file);
        fputs(outBuffer,file);
    }
    else
    {
        fputs("iface eth0 inet dhcp\n", file);
    }
    //fputs("iface eth1 inet dhcp\n",file);
    //fputs("\n",file);
    fputs("auto eth0:1\n",file);
    fputs("iface eth0:1 inet static\n",file);
    fprintf(file, "address 192.168.50.%d\n", 21+board_position);
    fprintf(file, "netmask 255.255.255.0\n\n");
    fputs("# Ethernet/RNDIS gadget (g_ether)\n",file);
    fputs("# ... or on host side, usbnet and random hwaddr\n",file);
    fputs("iface usb0 inet static\n",file);
    fprintf(file, "address 192.168.7.%d\n", 21+board_position);
    //fputs(" address 192.168.7.2\n",file);
    fputs(" netmask 255.255.255.0\n",file);
    fputs(" network 192.168.7.0\n",file);
    fputs(" gateway 192.168.7.1\n",file);
    fputs("\n",file);
    fputs("# Bluetooth networking\n",file);
    fputs("iface bnep0 inet dhcp\n",file);

    fclose(file);

    system("cp /tmp/interfaces /etc/network/interfaces\n");
    system("cp /tmp/interfaces /mnt/sdcard/etc/interfaces \n");
    system("ifdown eth0 && ifup eth0 &\n");
    system("ifdown eth0:1 && ifup eth0:1 &\n");
    system("ifdown usb0 && ifup usb0 &\n");
//    system("ifdown eth0");
//    system("ifup eth0");

}


bool loadEthSettings()
{
    FILE *file;
    file = fopen(settings_eth_path, "r");
    int dhcp;
    char buff[255];


    if (file!=NULL)
    {
        fgets(buff, 255, file);
        cJSON *json;
        json=cJSON_Parse(buff);
        if (json!=NULL)
        {
            if (SetIntegerIfExist(json, "dhcp", &dhcp)) {
                bool state;
                if (dhcp == 1)
                    state = true;
                if (dhcp == 0)
                    state = false;
                currentCfg.dhcp=state;
            }

            SetStringIfExist(json, "ip", currentCfg.ip);

            SetStringIfExist(json, "nm", currentCfg.nm);
            SetStringIfExist(json, "gw", currentCfg.gw);
            SetStringIfExist(json, "dns", currentCfg.dns);

        }
        return true;
    }
    else
        return false;


}



void saveSettings()
{
        FILE *file;
        file = fopen(settings_eth_path, "w");
    if (file!=NULL) {
        char data[200];
        int c;
        int len = 0;
        c = toBeApplyCfg.dhcp ? 1 : 0;

        len += sprintf(data + len, "{\"dhcp\":%d,", c);
        len += sprintf(data + len, "\"ip\":\"%s\",", toBeApplyCfg.ip);
        len += sprintf(data + len, "\"nm\":\"%s\",", toBeApplyCfg.nm);
        len += sprintf(data + len, "\"gw\":\"%s\",", toBeApplyCfg.gw);
        len += sprintf(data + len, "\"dns\":\"%s\" }", toBeApplyCfg.dns);
        fputs(data, file);
        fclose(file);


    }

}



bool SetBoolIfExist(cJSON *json, const char *field, bool * value )
{
    cJSON *jsonobj = cJSON_GetObjectItem(json,field);
    if (jsonobj)
    {
        *value = jsonobj->valueint != 0 ? true : false;
        return true;
    }
    else
        return false;
}

bool SetIntegerIfExist(cJSON *json, const char *field, int * value )
{
    cJSON *jsonobj = cJSON_GetObjectItem(json,field);
    if (jsonobj)
    {
        *value = jsonobj->valueint;
        return true;
    }
    else
        return false;
}

bool SetDoubleIfExist(cJSON *json, const char *field, double * value )
{
    cJSON *jsonobj = cJSON_GetObjectItem(json,field);
    if (jsonobj)
    {
        *value = jsonobj->valuedouble;
        return true;
    }
    else
        return false;
}


bool SetStringIfExist(cJSON *json, const char *field, char * value )
{
    cJSON *jsonobj = cJSON_GetObjectItem(json,field);
    if (jsonobj)
    {
        strcpy(value, jsonobj->valuestring);
        return true;
    }
    else
        return false;
}

bool SetEnumIfExist(cJSON *json, const char *field, int * value, const char *list_of_values [], int cnt )
{
    int i;
    cJSON *jsonobj = cJSON_GetObjectItem(json,field);
    if (jsonobj)
    {
        for (i = 0; i<cnt;i++)
        {
            if (strcmp( list_of_values[i], jsonobj->valuestring ) ==0)
            {
                * value = i;
            }
        }
        return true;
    }
    else
        return false;
}




void *OpenMap()
{

    uint64_t offset, base;
    uint32_t value;
    volatile uint8_t *mm;
    offset = (uint64_t) 0x43c20000;
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
