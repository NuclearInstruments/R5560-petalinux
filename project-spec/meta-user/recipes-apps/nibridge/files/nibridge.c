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

#include "zmq.h"
#include "zhelpers.h"

#define SEGMENT_SIZE 0x10000000
#define PAGE_SIZE ((size_t)getpagesize())
#define PAGE_MASK ((uint64_t)(long)~(PAGE_SIZE - 1))
#define REG_SIZE 4


//DMA SECTION






#define AXI_BUS_BA 0x43C10000
#define MAP_SIZE (1024*1024*1UL)
#define DEFAULT_BUFFER_SIZE 2048*(1024*8)

#define MAP_MASK (MAP_SIZE - 1)
#define CHAINS 1
#define DATA_BA 0x30000000
#define MAP_SIZE_DATA (256*1024*1024*1UL)


const uint32_t DMA_BA_ADDRESS[] = {0x0000, 0x1000};
const uint32_t DD3_ADDRESS[] = {0x00000000, 0x08000000};

#define XDATA_MOVER_AXIL_ADDR_BUFFER_STATUS_DATA 0x10
#define XDATA_MOVER_AXIL_BITS_BUFFER_STATUS_DATA 32
#define XDATA_MOVER_AXIL_ADDR_BUFFER_STATUS_CTRL 0x14
#define XDATA_MOVER_AXIL_ADDR_BUFFER_ACK_DATA    0x18
#define XDATA_MOVER_AXIL_BITS_BUFFER_ACK_DATA    32
#define XDATA_MOVER_AXIL_ADDR_RUN_DATA           0x38
#define XDATA_MOVER_AXIL_BITS_RUN_DATA           1
#define XDATA_MOVER_AXIL_ADDR_DDROFFSET_V_DATA   0x40
#define XDATA_MOVER_AXIL_BITS_DDROFFSET_V_DATA   32
#define XDATA_MOVER_AXIL_ADDR_BUFFER_SEQ_BASE    0x20
#define XDATA_MOVER_AXIL_ADDR_BUFFER_SEQ_HIGH    0x2f
#define XDATA_MOVER_AXIL_WIDTH_BUFFER_SEQ        64
#define XDATA_MOVER_AXIL_DEPTH_BUFFER_SEQ        2
#define XDATA_MOVER_AXIL_ADDR_BUFSIZE_BASE       0x30
#define XDATA_MOVER_AXIL_ADDR_BUFSIZE_HIGH       0x37
#define XDATA_MOVER_AXIL_WIDTH_BUFSIZE           32
#define XDATA_MOVER_AXIL_DEPTH_BUFSIZE           2
#define XDATA_MOVER_AXIL_ADDR_STAT_COUNTER_BASE  0x60
#define XDATA_MOVER_AXIL_ADDR_STAT_COUNTER_HIGH  0x7f
#define XDATA_MOVER_AXIL_WIDTH_STAT_COUNTER      64
#define XDATA_MOVER_AXIL_DEPTH_STAT_COUNTER      4




//END DMA SECTION
int TotalAllocatedBuffer=0;
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

void *context;
void *publisher;

int ZMQServer (void);

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
    
    ZMQServer();
    NiBridgeBasicTCPServer();
    
    
    
    zmq_ctx_destroy (context);
    return 0;
}




bool SSTHRunning = false;

void my_free (void *data, void *hint) {
    if (data!=NULL) {
        TotalAllocatedBuffer--;
        printf("Free Pointer: %8X\n", data);
        free(data);
    }

}



void my_copy(volatile unsigned char *dst, volatile unsigned char *src, int sz)
{
    if (sz & 63) {
        sz = (sz & -64) + 64;
    }
    asm volatile (
    "NEONCopyPLD_%=:                          \n"
    "    VLDM %[src]!,{d0-d7}                 \n"
    "    VSTM %[dst]!,{d0-d7}                 \n"
    "    SUBS %[sz],%[sz],#0x40                 \n"
    "    BGT NEONCopyPLD_%=                  \n"
    : [dst]"+r"(dst), [src]"+r"(src), [sz]"+r"(sz) : : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "cc", "memory");
}

void SockStreamer(void *lpParam)
{
    char *data_array;



    int iResult;
    int PackLen;

    int ListenSocket = -1;
    int ClientSocket = -1;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;

    uint64_t Timecode = 0;
    uint32_t *DataHT;
    uint32_t *DataHE;
    uint32_t PCounter = 0;

    uint64_t timecode_h = 0;
    uint64_t timecode_s = 0;

    SSTHRunning = false;




    int i,j;
    char *buffer;
    uint32_t transdata;
    uint32_t max_size_chunk;
    uint32_t address = DD3_ADDRESS[0];
    uint32_t size = DEFAULT_BUFFER_SIZE;
    uint32_t offset = 0;
    uint32_t count = 1;
    uint32_t run = 0;
    uint32_t chain = 0;
    uint32_t seg[CHAINS];
    uint32_t BState[32];
    uint64_t BIndex[32];
    uint32_t BSize[32];
    uint32_t read_result;

    uint64_t Stat_EVNT_IN;
    uint64_t Stat_EVNT_OUT;
    uint64_t Stat_EVNT_LOST;
    uint64_t Stat_EVNT_TDIST;
    struct timespec time_now;
    volatile uint64_t *u64memPointer;
    volatile uint8_t *u8memPointer;

    volatile void *map_base;
    volatile void *map_base_data;
    int fd_reg;



    if ((fd_reg = open("/dev/mem", O_RDWR | O_SYNC)) == -1) return;

    /* map one page */
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_reg, AXI_BUS_BA);
    if (map_base == (void *) -1) return;
    printf("Memory AXIBUS mapped at address %p.\n", map_base);

    map_base_data = mmap(0, MAP_SIZE_DATA, PROT_READ, MAP_SHARED, fd_reg, DATA_BA) ;
    if (map_base_data == (void *) -1) return;
    printf("Memory DATA mapped at address %p.\n", map_base_data);





    *((uint32_t *) (map_base + DMA_BA_ADDRESS[0] + XDATA_MOVER_AXIL_ADDR_DDROFFSET_V_DATA)) = DATA_BA;
    *((uint32_t *) (map_base + DMA_BA_ADDRESS[0] + XDATA_MOVER_AXIL_ADDR_RUN_DATA)) = 0x01;


    data_array = (char *) malloc(size);
    printf("Allocate Pointer: %8X\n", data_array);
    TotalAllocatedBuffer++;

    printf("Starting streaming socket\n");



    // Receive until the peer shuts down the connection
    //
    do {

        //if (TotalAllocatedBuffer<2) {
            bool transfered = false;
            for (chain = 0; chain < CHAINS; chain++) {
                read_result = *((volatile uint32_t *) (map_base + DMA_BA_ADDRESS[chain] +
                                                       XDATA_MOVER_AXIL_ADDR_BUFFER_STATUS_DATA));
                // printf("DMA[%05X]  Status Reg: %08x\n",DMA_BA_ADDRESS[chain], (unsigned int)read_result);

                for (i = 0; i < 8; i++) {
                    BState[i] = (read_result >> i) & 0x01;
                }

                for (i = 0; i < 8; i++) {
                    if (BState[i] == 1) {
                        BIndex[i] = *((volatile uint64_t *) (map_base + DMA_BA_ADDRESS[chain] +
                                                             XDATA_MOVER_AXIL_ADDR_BUFFER_SEQ_BASE +
                                                             i * sizeof(uint64_t)));
                        BSize[i] = *((volatile uint32_t *) (map_base + DMA_BA_ADDRESS[chain] +
                                                            XDATA_MOVER_AXIL_ADDR_BUFSIZE_BASE + i * sizeof(uint32_t)));
                        BSize[i] = BSize[i] / 2;
                        // printf("Buffer [%2d] \t\t Index: %16lx \t\t Size:0x%08x\n", i, BIndex[i],  BSize[i]);
                    }
                }


                for (i = 0; i < 8; i++) {
                    if (BState[i] == 1) {
                        transfered = true;
                        int file_write_size;
                        uint32_t ACQevents;
                        size = BSize[i] * sizeof(uint64_t) < DEFAULT_BUFFER_SIZE ? BSize[i] * sizeof(uint64_t) :
                               DEFAULT_BUFFER_SIZE;
                        address = DD3_ADDRESS[chain] + i * DEFAULT_BUFFER_SIZE;
                        printf("DMA[%05X] address = 0x%08x, size = 0x%08x, offset = 0x%08x, count = %u -- BUFFERS:%4d\n",
                               DMA_BA_ADDRESS[chain], address, size, offset, count, TotalAllocatedBuffer);
                        u64memPointer = (volatile uint64_t *) (map_base_data + address);
                        ACQevents = size / sizeof(uint64_t);

                        /*if (ACQevents>5)
                        {
                            printf("-------------------------------------------------\n");
                            for (j=0;j<6;j++)
                            {
                                printf("\t%08llX\n", u64memPointer[j]);
                            }
                            printf(".......\n");
                            for (j=-3;j<0;j++)
                            {
                                printf("\t%08llX\n", u64memPointer[size/sizeof(uint64_t) +j]);
                            }
                            printf("-------------------------------------------------\n");
                        }*/
                        //Eseguire la memcopy
                        //memcpy(data_array, u64memPointer, size);
                        my_copy(data_array, u64memPointer, size);
                        int size_sent = zmq_send (publisher, (char *) data_array, size, 0);

                        /*if (data_array != NULL) {
                            if (size > 0) {

                                zmq_msg_t message;
                                zmq_msg_init_data(&message, data_array, size, my_free, NULL);
                                zmq_msg_send(&message, publisher, 0);
                                zmq_msg_close(&message);
                            }
                        } else {
                            printf("TXBuffer Memory allocation failed!\n");
                        }*/

                        /*data_array[0] = 0xAB;
                        data_array[1] = 0xBA;
                        data_array[2] = 0xFF;
                        data_array[3] = 0x01;
                        memcpy((char*)&data_array[4], &ACQevents, sizeof(uint32_t));*/

                        //memcpy(&data_array[8], &PCounter, sizeof(uint32_t));

                        *((volatile uint32_t *) (map_base + DMA_BA_ADDRESS[chain] +
                                                 XDATA_MOVER_AXIL_ADDR_BUFFER_ACK_DATA)) = 0;
                        *((volatile uint32_t *) (map_base + DMA_BA_ADDRESS[chain] +
                                                 XDATA_MOVER_AXIL_ADDR_BUFFER_ACK_DATA)) = (1 << i);
                        *((volatile uint32_t *) (map_base + DMA_BA_ADDRESS[chain] +
                                                 XDATA_MOVER_AXIL_ADDR_BUFFER_ACK_DATA)) = 0;

                        PCounter++;

                    }
                }
            }

            if (transfered == false)
                usleep(1000);
       // }
       // else
       //     usleep(1000);
    } while (1);


    usleep(1000);
    // shutdown the connection since we're done


    printf("[STREAMING] Socket Shutdown!\n");



    //STOP DMA
    *((uint32_t *) (map_base + DMA_BA_ADDRESS[0] + XDATA_MOVER_AXIL_ADDR_RUN_DATA)) = 0x00;


    SSTHRunning = false;


    return 0;

}



void *ZMQWorker(void *data)
{
    printf("ZMQ main thread started \n");
    char *buffer = (char*) malloc(1024*1024*16*sizeof(char));
    uint32_t q=0;
    while (1) {
        
        int size = zmq_send (publisher, buffer, 1024*1024*16, 0);
        printf("Sent[%d]: %d\n",q++, size);
        
        /*
        //  Get values that will fool the boss
        int zipcode, temperature, relhumidity;
        zipcode     = 10001;
        temperature = temperature + 1;
        relhumidity = relhumidity+1;

        //  Send message to all subscribers
        char update [20];
        sprintf (update, "%05d %d %d", zipcode, temperature, relhumidity);
        s_send (publisher, update);*/
    }
    
    
    zmq_close (publisher);

   

}

int ZMQServer (void)
{
    printf("Starting ZMQ server \n");
    
    context = zmq_ctx_new ();
    publisher = zmq_socket (context, ZMQ_PUSH);

    int hwm = 10;
    int rc = zmq_setsockopt(publisher, ZMQ_SNDHWM, &hwm, sizeof(int));
    if (rc!=0)
    {
        printf("SetSockOption error %d -> %s\n", zmq_errno(), zmq_strerror(zmq_errno()));


    }
    rc = zmq_setsockopt(publisher, ZMQ_RCVHWM, &hwm, sizeof(int));
    if (rc!=0)
    {
        printf("SetSockOption error %d -> %s\n", zmq_errno(), zmq_strerror(zmq_errno()));


    }

    rc = zmq_bind (publisher, "tcp://*:5556");

    if (rc != 0)
    {
        printf("bind error %d -> %s\n", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }
    pthread_t thread_id;
    if(pthread_create( &thread_id, NULL,  SockStreamer, NULL) < 0) {
			printf("could not create thread for ZMQ\n");
			return 1;
        }
		
    
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
	buffer=(char *) malloc ( 0x1000000 );
	printf("[NIBRIDGE] New client Connected. \n");
    do
	{
		read_size = recv(sock, buffer, HEADER_LEN*4, 0);
		if (read_size==HEADER_LEN*4)
		{
		    cmd_header=*((uint32_t *)(&buffer[0]));
		    cmd_flags=*((uint32_t *)(&buffer[4]));	
		    cmd_address=*((uint32_t *)(&buffer[8]));		    	    
		    cmd_len=*((uint32_t *)(&buffer[12]));		    	    
		    /*printf("HEADER:    %8x\n",cmd_header);
		    printf("FLAGS:     %8x\n",cmd_flags);
		    printf("ADDRESS:   %8x\n",cmd_address);
		    printf("LEN:       %8x\n",cmd_len);	*/
   		    //printf("\n");				    
		    cmd_op = cmd_flags & 0xFF;	    		    		    
            
            if (cmd_len<0x1000000/4)
            {
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
                        read_size = send(sock, buffer, REG_SIZE, MSG_NOSIGNAL );
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
                        read_size = send(sock, buffer, cmd_len * REG_SIZE, MSG_NOSIGNAL );
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
		                                  		    
                            /*printf("STATUS_ADDR:    %8x\n",status_address);
                		    printf("TIMEOUT_MS:     %d\n",timeout);
                		    printf("BLOCKING:       %x\n",blocking);            		    */

                            ReadFIFO(R5550RegArea, cmd_address, status_address, cmd_len, timeout,  (uint32_t *)buffer, &word_read, blocking); 
                            read_size = send(sock,((char*) &word_read), REG_SIZE, MSG_NOSIGNAL );
                            
                            
                            if (read_size>0)
                                read_size = send(sock, buffer, REG_SIZE * word_read, MSG_NOSIGNAL );                    
                        break;                    
                        }
                        
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





