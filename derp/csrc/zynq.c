/*
 * This test application is to read/write data directly from/to the device 
 * from userspace. 
 * 
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include "zynqlib.h"


uint32_t frame_size = 640*480;

void commandIF(volatile Conf* conf,void* pipebuf_ptr) {
    char pipe_name[50];
    char str[13];
    char helpStr[300];
    char cmd;
    char cmd2;
    uint32_t camid;
    uint32_t addr;
    uint32_t value;
    uint32_t snap_cnt=0;

    // TODO fix pipe read/pipe write
    sprintf(helpStr,"%s","Entering Interactive mode!\nCommands:\n");
    sprintf(helpStr,"%s%s",helpStr,"\tPipe reg read:\tr <regNum> \n");
    sprintf(helpStr,"%s%s",helpStr,"\tPipe reg write:\tw <regNum> <value> \n");
    sprintf(helpStr,"%s%s",helpStr,"\tCam reg read:\tcr <camid> <addr>\n");
    sprintf(helpStr,"%s%s",helpStr,"\tCam reg write:\tcw <camid> <addr> <value>\n");
    sprintf(helpStr,"%s%s",helpStr,"\tSave to disk:\ts\n");
    sprintf(helpStr,"%s%s",helpStr,"\tHelp:\t\th\n");
    sprintf(helpStr,"%s%s",helpStr,"\tStop cmd:\t<Anything else>\n");
    printf("%s",helpStr);
    while(1) {
        printf(">"); fflush(stdout);
        if(fgets(str,12,stdin)!=NULL) {
            if(sscanf(str,"%c",&cmd) && cmd=='h') {
                printf("%s",helpStr);
            }
            else if(sscanf(str,"%c%c %d %x %x",&cmd,&cmd2,&camid,&addr,&value)
                && cmd=='c' && cmd2=='w' && (camid==0||camid==1)) {
                write_cam_safe(conf, camid, ((addr<<8) | value));
                printf("WROTE 0x%.2x to addr 0x%.2x on CAM%d\n",value,addr,camid);
            }
            else if(sscanf(str,"%c%c %d %x",&cmd,&cmd2,&camid,&addr)
                    && cmd=='c' && cmd2=='r' && (camid<MMIO_CAM_NUM)) {
                value = read_cam_reg(conf,camid,addr);
                printf("READ 0x%.2x from addr 0x%.2x on CAM%d\n",value,addr,camid);
            }
            else if(sscanf(str,"%c %d %x",&cmd,&addr,&value)
                && cmd=='w' && (addr<MMIO_PIPE_SIZE)) {
                write_pipe_reg(conf, addr, value);
                printf("WROTE 0x%.2x to pipe reg 0x%.2x\n",value,addr);
            }
            else if(sscanf(str,"%c %d",&cmd,&addr)
                && cmd=='r' && (addr<MMIO_PIPE_SIZE)) {
                value = read_pipe_reg(conf, addr);
                printf("READ 0x%.2x from pipe reg 0x%.2x\n",value,addr);
            }
            else if(sscanf(str,"%c",&cmd) && cmd=='s') {
                sprintf(pipe_name,"/tmp/snap%d.raw",snap_cnt++);
                saveImage(pipe_name,pipebuf_ptr,frame_size*4);
            }
            else if(sscanf(str,"%c",&cmd) && cmd=='d') {
                print_debug_regs(conf);
            }
            else {
                printf("Exiting Interactive Mode!\n");
                fflush(stdout);
                break;
            }
            fflush(stdout);
        }
        else {
            printf("Exiting Interactive Mode!\n");
            break;
        }
    }

}

int main(int argc, char *argv[]) {

    if(argc!=4){
        printf("Format: processimage <seconds> <gain> <thresh>\n");
        exit(1);
    }

    uint32_t time = atoi(argv[1]);
    uint32_t gain = atoi(argv[2]);
    uint32_t thresh = atoi(argv[3]);

    unsigned gpio_addr = MMIO_STARTADDR;
    
    unsigned tribuf0_addr = 0x30008000;
    int tribuf0_size = frame_size*3;
    
    unsigned  tribuf1_addr = tribuf0_addr + tribuf0_size;
    int tribuf1_size = frame_size*3;
    
    unsigned  tribuf2_addr = tribuf1_addr + tribuf1_size;
    int tribuf2_size = frame_size*4*3;
    
    unsigned page_size = sysconf(_SC_PAGESIZE);
    
    int fd = open ("/dev/mem", O_RDWR);
    if (fd < 1) {
        perror(argv[0]);
        return -1;
    }

    void* tribuf2_ptr = mmap(NULL, tribuf2_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, tribuf2_addr);
    if (tribuf2_ptr == MAP_FAILED) {
        printf("FAILED mmap for tribuf2 %x\n",tribuf2_addr);
        exit(1);
    }


    for(uint32_t i=0;i<frame_size*4*3; i++ ) {
        *(unsigned char*)(tribuf2_ptr+i)= i%256; 
    }
    
    // mmap the device into memory 
    // This mmaps the control region (the MMIO for the control registers).
    void * gpioptr = mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, gpio_addr);
    if (gpioptr == MAP_FAILED) {
        printf("FAILED mmap for gpio_addr %x\n",gpio_addr);
        exit(1);
    }
   
    volatile Conf * conf = (Conf*) gpioptr;

    //write_mmio(conf, MMIO_TRIBUF_ADDR(0), tribuf0_addr,0);
    //write_mmio(conf, MMIO_FRAME_BYTES(0), frame_size,0);
    //write_mmio(conf, MMIO_TRIBUF_ADDR(1), tribuf1_addr,0);
    //write_mmio(conf, MMIO_FRAME_BYTES(1), frame_size,0);
    write_mmio(conf, MMIO_TRIBUF_ADDR(2), tribuf2_addr,0);
    write_mmio(conf, MMIO_FRAME_BYTES(2), frame_size*4,0);
    // Start stream
    //write_pipe_reg(conf,0,thresh);
    //write_cam_safe(conf,0,gain&0xFF);
    printf("STARTING STREAM\n");
    fflush(stdout);
    
    write_mmio(conf, MMIO_CMD, CMD_START,0);
    
    commandIF(conf,tribuf2_ptr);
    
    write_mmio(conf, MMIO_CMD, CMD_STOP,0);
    printf("STOPPING STREAM\n");
    fflush(stdout);
    sleep(1);
  return 0;
}



