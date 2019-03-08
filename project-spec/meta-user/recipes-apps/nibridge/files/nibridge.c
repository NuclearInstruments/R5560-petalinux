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
#include <pthread.h>
#include <math.h>
#include <time.h>

#define SEGMENT_SIZE 0x10000000
#define PAGE_SIZE ((size_t)getpagesize())
#define PAGE_MASK ((uint64_t)(long)~(PAGE_SIZE - 1))
#define REG_SIZE 4

void *R5550RegArea;

inline uint32_t ReadReg(void *mm, uint32_t address);
inline int WriteReg(void *mm, uint32_t address, uint32_t v);
void *OpenMap();
int CloseMap();

void *connection_handler(void *socket_desc);
int NiBridgeBasicTCPServer();

uint32_t ReadFIFO(void *mm, uint32_t address, uint32_t status_address, uint32_t len, int timeout_ms, uint32_t *v, uint32_t *word_read, bool blocking);

int WriteMem(void *mm, uint32_t address, uint32_t len, uint32_t *v);

int ReadMem(void *mm, uint32_t address, uint32_t len, uint32_t *v);

int main(int argc, char **argv)
{
    int i;
    printf("Starting nibridge daemon\n");
    R5550RegArea = OpenMap();
    if (R5550RegArea==NULL)
    {
        printf("Error connecting daemon do Avalon Bus. Daemon die\n");
        return -1;
    }
    printf("Device Model       %8x\n", ReadReg(R5550RegArea, 0x03FFFFFF));	
    printf("Firmware Build:    %8x\n", ReadReg(R5550RegArea, 0x03FFFFFE));	
    printf("Firmware Unique:   %8x\n", ReadReg(R5550RegArea, 0x03FFFFFD));	
	
	printf("Write Test\n");
    for (i=0;i<64;i++)
        WriteReg(R5550RegArea, 0x03FE0000+ i, i);
        
    for (i=0;i<64;i++)
    {
        printf("%2x ", ReadReg(R5550RegArea, 0x03FE0000+i));	        
        if ((i+1)%16==0)
            printf("\n");
    }
    printf("\n");    
    
    NiBridgeBasicTCPServer();
    return 0;
}


int NiBridgeBasicTCPServer()
{
    int socket_desc, client_sock, c;
	struct sockaddr_in server, client;
	// Create socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_desc == -1) {
		printf("Could not create socket\n");
	}
	//puts("Socket created");
	// Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8888);
	// Bind
	if(bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) < 0) {
		// print the error message
		printf("bind failed. Error\n");
		return 1;
	}
	//puts("bind done");
	// Listen
	listen(socket_desc, 3);
	// Accept and incoming connection
	//puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);
	// Accept and incoming connection
	//puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);
	pthread_t thread_id;
	while((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
		//puts("Connection accepted");
		if(pthread_create( &thread_id, NULL,  connection_handler, (void*) &client_sock) < 0) {
			printf("could not create thread\n");
			return 1;
		}
		// Now join the thread, so that we dont terminate before the thread
		// pthread_join( thread_id, NULL);
		//puts("Handler assigned");
	}
	if (client_sock < 0) {
		printf("accept failed\n");
		return 1;
	}
    
    return 0;
}


void *connection_handler(void *socket_desc) {
    int i;
    int HEADER_LEN=4;
    int read_size;
    uint32_t cmd_header;
    uint32_t cmd_address;    
    uint32_t cmd_len;    
    uint32_t cmd_flags;
    uint8_t cmd_op;
    uint32_t v;
    int res;
    int sock = *(int*)socket_desc;
        
	char *buffer;		
	buffer=(char *) malloc ( 0x100000 );
	
    do
	{
		read_size = recv(sock, buffer, HEADER_LEN*4, 0);
		if (read_size==HEADER_LEN*4)
		{
		    cmd_header=*((uint32_t *)(&buffer[0]));
		    cmd_flags=*((uint32_t *)(&buffer[4]));	
		    cmd_address=*((uint32_t *)(&buffer[8]));		    	    
		    cmd_len=*((uint32_t *)(&buffer[12]));		    	    
		    printf("HEADER:    %8x\n",cmd_header);
		    printf("FLAGS:     %8x\n",cmd_flags);
		    printf("ADDRESS:   %8x\n",cmd_address);
		    printf("LEN:       %8x\n",cmd_len);	
   		    printf("\n");				    
		    cmd_op = cmd_flags & 0xFF;	    		    		    

            switch(cmd_op)
            {
                case 0:
                    read_size = recv(sock, buffer, REG_SIZE, 0);
                    if (read_size == REG_SIZE)
                    {
                        res = WriteReg(R5550RegArea, cmd_address, *((uint32_t *)buffer));
                    }
                    break;
                    
                case 1:
                    v = ReadReg(R5550RegArea, cmd_address);        
                    memcpy(buffer, &v, sizeof(uint32_t));
                    read_size = send(sock, buffer, REG_SIZE, 0);
                    break;

                case 2:
                    read_size = recv(sock, buffer, cmd_len * REG_SIZE, 0);
                    if (read_size == cmd_len * REG_SIZE)
                    {
                        WriteMem(R5550RegArea, cmd_address, cmd_len, (uint32_t *)buffer);
                    }
                    break;
                    
                case 3:
                    ReadMem(R5550RegArea, cmd_address, cmd_len, (uint32_t *)buffer);              
                    read_size = send(sock, buffer, cmd_len * REG_SIZE, 0);
                    break;                    
                    
                case 4:
                    {
                        uint32_t status_address;
                        uint32_t timeout;                        
                        uint32_t blocking;                                                
                        uint32_t word_read;                                 
                        read_size = recv(sock, buffer, 8, 0);   
              		    status_address=*((uint32_t *)(&buffer[0]));             
            		    timeout=*((uint32_t *)(&buffer[4])) & 0xFFFFFF;              		    
		                blocking=(*((uint32_t *)(&buffer[4]))>>31) & 0x1;     
		                              		    
                        printf("STATUS_ADDR:    %8x\n",status_address);
            		    printf("TIMEOUT_MS:     %d\n",timeout);
            		    printf("BLOCKING:       %x\n",blocking);            		    

                        ReadFIFO(R5550RegArea, cmd_address, status_address, cmd_len, timeout,  (uint32_t *)buffer, &word_read, blocking); 
                        read_size = send(sock,((char*) &word_read), REG_SIZE, 0);
                        read_size = send(sock, buffer, REG_SIZE * word_read,0);                    
                    break;                    
                    }
                    
            }
		}
	}	while (read_size!=0);
		    
    free(buffer);

}

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


uint32_t ReadFIFO(void *mm, uint32_t address, uint32_t status_address, uint32_t len, int timeout_ms, uint32_t *v, uint32_t *word_read, bool blocking)
{
    time_t start_t, end_t;
    double diff_t;
    int tx_rim = len;
    int i;
    int q=0;
    *word_read=0;    
    if (address > (SEGMENT_SIZE>>2))
    {
        return -1;
    }

    time(&start_t);
    while(tx_rim>0)
    {
        uint32_t status_raw = ReadReg(mm, status_address);           
        uint32_t status_waval = (status_raw >> 8) & 0xFFFFFF;
        if (status_waval)
        {
            int tx_size = tx_rim > status_waval ? status_waval : tx_rim;
            for (i =0;i<tx_size;i++)
            {
                v[q++] = *(volatile uint32_t *)(mm + (address*4));
            }
            tx_rim -= tx_size;
            *word_read=q;
        }
        else
        {
            if (!blocking) return 1;
            time(&end_t);
            diff_t = difftime(end_t, start_t);
            if (diff_t*1000.0 > timeout_ms)
                return -2;
            usleep(10);
        }
        
    }
    
    return 0;
}





