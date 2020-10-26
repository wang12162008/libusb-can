#include <conio.h>
#if 0

#include <stdio.h>
#include "libusb.h"

typedef  struct  CAN_OBJ
{
    UINT	ID;
    UINT	TimeStamp;
    BYTE	TimeFlag;
    BYTE	SendType;///00 是正常发送 01 单次发送 02 自收自发03 单次自收自发

    BYTE	RemoteFlag;//是否是远程帧
    BYTE	ExternFlag;//是否是扩展帧
    BYTE	DataLen;
    BYTE	Data[8];

} CAN_OBJ,*PCAN_OBJ;
#define DEVS_MAX 16

static libusb_device_handle* devs[DEVS_MAX]= {0};
static struct libusb_transfer *bulkin_transfer= {NULL};
int rxnumber=0;
void LIBUSB_CALL bulk_in_fn(struct libusb_transfer *transfer)
{
    ///printf("bulk_in_fn1");
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
    {
        printf("img transfer status %d?\n", transfer->status);

        return;
    }

    int r = libusb_submit_transfer(transfer);   //提交一个传输并立即返回
    if (r < 0)
    {
        libusb_cancel_transfer(transfer);   //异步取消之前提交传输
        printf("r=%d\n",r);
    }
    rxnumber+=transfer->actual_length;
    ///printf("read %d\n",transfer->actual_length);
    ///printf("upload_img_B_over callback : %d \n",transfer->status);

}
static int usb_interrupt_write(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    int ret=libusb_interrupt_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;


}
static int usb_interrupt_read(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    /// port |= 0x80;
    int ret=libusb_interrupt_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;
    /*int ret=libusb_control_transfer(dev,port,0,0,0,bytes,len,timeout);
     return ret;*/


}
static int bulk_write(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    int ret=libusb_bulk_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;


}
int bulk_read(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    int ret=libusb_bulk_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;


}

int interrupt_read(libusb_device_handle *dev,char port,char* bytes,int sendlen,char* recv_bytes,int recv_len)
{
    if(usb_interrupt_write(dev,port,bytes,sendlen,100)<0)
    {
#if(PRINTF_LIBUSB_ERR)
        printf("error usb_interrupt_write %s\n", usb_strerror());
#endif
        return -1;
    }
    else
    {
        ///printf("success: usb_interrupt_write \n");
    }
    if(usb_interrupt_read(dev,0x80+port,recv_bytes,recv_len,100)<0)
    {
#if(PRINTF_LIBUSB_ERR)
        printf("error usb_interrupt_read %s\n", usb_strerror());
#endif
        return -2;
    }
    else
    {
        ///printf("success: usb_interrupt_read 0x%x\n",buf[0]);
    }
    return 1;
}

static int API_can_number(int index)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index])
        {
            char buf[64]= {0x12, 0x13, 0x01, 0x00};
            if(interrupt_read(devs[index],1,buf,4,buf,64)<0)
            {
                printf("error dr_Version \n");
                return 0;
            }
            else
            {
                ///printf("interrupt_read %x %x\n",buf[4],buf[5]);
            }
            return (buf[4]);
        }
    }
    return 0;
}

static int API_str_Serial_Num(int index,char* str,int len)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index])
        {
            char buf[64]= {0x12, 0x14, 0x01, 0x00};
            if(interrupt_read(devs[index],1,buf,4,str,len)<0)
            {
                printf("error str_Serial_Num \n");
                return 0;
            }
            else
            {
                ///printf("interrupt_read %x %x\n",buf[4],buf[5]);
            }
            int num = (str[2]&0xf)-1;
            for(int i=0; i<num&&i<len; i++)
            {
                str[i]=str[i+4];
            }
            str[num]=0;
            return 1;
        }
    }
    return 0;
}
static int API_canname(int index,char* str,int len)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index])
        {
            char buf[64]= {0x12, 0x15, 0x01, 0x00};
            if(interrupt_read(devs[index],1,buf,4,str,len)<0)
            {
                printf("error canname \n");
                return 0;
            }
            else
            {
                ///printf("interrupt_read %x %x\n",buf[4],buf[5]);
            }
            int num = (str[2]&0xf)-1;
            for(int i=0; i<num&& i<len; i++)
            {
                str[i]=str[i+4];
            }
            str[num]=0;
            return 1;
        }
    }
    return 0;
}
static int API_set_time0_time1(int index,int port,int time0,int time1)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index] && port<2)
        {
            char buf[64]= {0x12,0x03,0x04,0x00,0x00,0x11,0x22};
            if(port==1)
            {
                buf[4]|=0x10;
            }
            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            if(interrupt_read(devs[index],1,buf,7,(char*)recv_buf,64)<0)
            {
                printf("error setBaud \n");
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==buf[0] &&
                    recv_buf[1]==buf[1] &&
                    recv_buf[2]==0x81 &&
                    recv_buf[3]==0x00
              )
            {
                return 1;
            }
            else
            {
                printf("error setBaud %x %x %x %x\n",recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3]);
                return 0;
            }
        }
    }
    return 0;
}
static int API_setFilter(int index,int port,int mode,unsigned long code,unsigned long mask)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index] && port<2)
        {
            char buf[64]= {0x12,0x04,0x0a,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};
            if(port==1)
            {
                buf[4]|=0x10;
            }
            if(mode==0)
            {
                buf[4]|=0x40;
            }
            buf[5]=(code>>24)&0xff;
            buf[6]=(code>>16)&0xff;
            buf[7]=(code>>8)&0xff;
            buf[8]=(code)&0xff;
            buf[9]=(mask>>24)&0xff;
            buf[10]=(mask>>16)&0xff;
            buf[11]=(mask>>8)&0xff;
            buf[12]=(mask)&0xff;
            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            if(interrupt_read(devs[index],1,buf,13,(char*)recv_buf,64)<0)
            {
                printf("error %s \n",__FUNCTION__);
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==buf[0] &&
                    recv_buf[1]==buf[1] &&
                    recv_buf[2]==0x81 &&
                    recv_buf[3]==0x00
              )
            {
                return 1;
            }
            else
            {
                printf("error %s %x %x %x %x\n",__FUNCTION__,recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3]);
                return 0;
            }
            return 1;
        }
    }
    return 0;
}
static int API_closePort(int index,int port)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index] && port<2)
        {
            char buf[64]= {0x12,0x0f,0x12,0x00,0x00};
            if(port==1)
            {
                buf[4]|=0x10;
            }

            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            if(interrupt_read(devs[index],1,buf,5,(char*)recv_buf,64)<0)
            {
                printf("error %s \n",__FUNCTION__);
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==buf[0] &&
                    recv_buf[1]==buf[1] &&
                    recv_buf[2]==0x81 &&
                    recv_buf[3]==0x00
              )
            {
                return 1;
            }
            else
            {
                printf("error %s %x %x %x %x\n",__FUNCTION__,recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3]);
                return 0;
            }
            return 1;
        }
    }
    return 0;
}
static int API_startPort(int index,int port)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index] && port<2)
        {
            char buf[64]= {0x12,0x0e,0x02,0x00,0x00};
            if(port==1)
            {
                buf[4]|=0x10;
            }
            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            if(interrupt_read(devs[index],1,buf,5,(char*)recv_buf,64)<0)
            {
                printf("error %s \n",__FUNCTION__);
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==buf[0] &&
                    recv_buf[1]==buf[1] &&
                    recv_buf[2]==0x81 &&
                    recv_buf[3]==0x00
              )
            {
                return 1;
            }
            else
            {
                printf("error %s %x %x %x %x\n",__FUNCTION__,recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3]);
                return 0;
            }
            return 1;

        }
    }
    return 0;
}
static int set_tx_usb_buf(int port,char* buf,CAN_OBJ* can_obj)
{
    unsigned long id = can_obj->ID;
    buf[0]=0;
    buf[1]=0;
    buf[2]=0xdd;
    buf[3]=0;
    buf[4]=0;

    buf[5]=can_obj->SendType;
    buf[6]=0;
    if(can_obj->ExternFlag)
    {
        buf[6]|=0x80;
    }
    if(can_obj->RemoteFlag)
    {
        buf[6]|=0x40;
    }
    if(port)
    {
        buf[6]|=0x10;
    }

    if(can_obj->ExternFlag)
    {
        id <<= 3;
    }
    else
    {
        id <<= 5;
    }
    buf[6] |= (can_obj->DataLen&0xf);

    buf[7] =((unsigned char*)&id)[0];
    buf[8] =((unsigned char*)&id)[1];
    buf[9] =((unsigned char*)&id)[2];
    buf[10] =((unsigned char*)&id)[3];

    for(int i=0; i<can_obj->DataLen; i++)
    {
        buf[11+i] = can_obj->Data[i];
    }
    return 19;
}

static int API_send(int index,int port,CAN_OBJ* can_objs,int sendlen,int timeout)
{
    static int alltime=0;
    int start;
    /// SetThreadPriorityBoost(GetCurrentThread(),TRUE);
    ///  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index] && port<2)
        {
            char buf[64];
            int len=0;
            for(unsigned int i=0; i<3&&i<sendlen; i++)
            {
                len += set_tx_usb_buf(port,buf+len,&can_objs[i]);
            }
            if(len==0)
                return 0;


            if(bulk_write(devs[index],2,buf,len,100)<0)
            {
                printf("error %s bulk_write len=%d\n",__FUNCTION__,len);
                return -1;
            }
            ///if(usb_interrupt_read(devs[index],0x81,(char*)recv,64,-1)<0)
            if(timeout==0)
                return 3;
            start=clock();

            ///int ret=0;
            if(usb_interrupt_read(devs[index],0x81,(char*)buf,64,10)<0)

                ///if((ret=libusb_control_transfer(devs[index],0x81,0,0,0,(char*)recv,64,10))<0)
            {
                printf("send err \n");
                /// printf("error %s usb_interrupt_read %s\n",__FUNCTION__, usb_strerror());
                return -2;
            }
            if(buf[0]==0x13 && buf[2]>0)
            {

            }
            else
            {
                printf("send false!!!!\n");
                return -2;
            }



            int end = clock();

            alltime += end-start;
            printf("%d %d\n",end-start,alltime);
            return buf[2];

        }
    }
    return 0;
}
int main()
{
    libusb_device **devs;
    int r, i,cnt;

    r = libusb_init(NULL);

    if (r < 0)
        return r;
    /*	cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
    	return (int)cnt;*/

    libusb_device_handle * device = libusb_open_device_with_vid_pid(NULL, (uint16_t)0x471, (uint16_t)0x1200);
    libusb_set_auto_detach_kernel_driver(device, 1);

    int status = libusb_claim_interface(device, 0);
    if (status != LIBUSB_SUCCESS)
    {
        libusb_close(device);
        printf("libusb_claim_interface failed: %s\n", libusb_error_name(status));
    }
///init end
    char buf[64];
    bulkin_transfer = libusb_alloc_transfer(0);///分配一个异步传输
    if (!bulkin_transfer)
    {
        printf("bulkin_transfer ERR\n");
    }
    char userdata[64];
    libusb_fill_bulk_transfer(bulkin_transfer, device, 0x82, buf,
                              sizeof(buf), bulk_in_fn, userdata, 5000);
    r = libusb_submit_transfer(bulkin_transfer);   //提交一个传输并立即返回
    if (r < 0)
    {
        libusb_cancel_transfer(bulkin_transfer);   //异步取消之前提交传输
        printf("1");
    }









    ::devs[0] = device;
    printf("API_can_number=[%d]\n",API_can_number(0));
    printf("API_set_time0_time1=[%d]\n",API_set_time0_time1(0,0,0x00,0x14));
    printf("API_set_time0_time1=[%d]\n",API_set_time0_time1(0,1,0x00,0x14));
    printf("API_setFilter=[%d]\n",API_setFilter(0,0,0,0,0xffffffff));
    printf("API_setFilter=[%d]\n",API_setFilter(0,1,0,0,0xffffffff));

    printf("API_startPort=[%d]\n",API_startPort(0,0));
    printf("API_startPort=[%d]\n",API_startPort(0,1));

    CAN_OBJ obj=
    {
        0x1ff,///UINT	ID;
        0,///UINT	TimeStamp;
        0,///BYTE	TimeFlag;
        2,///BYTE	SendType;00 是正常发送 01 单次发送 02 自收自发03 单次自收自发

        0,///BYTE	RemoteFlag;//是否是远程帧
        0,///BYTE	ExternFlag;//是否是扩展帧
        8,///BYTE	DataLen;
        {1,2,3,4,5,6,7,8}///BYTE	Data[8];
    };
    CAN_OBJ obj3[3];
    obj3[0] = obj;
    obj3[1] = obj;
    obj3[2] = obj;
    int start = clock();
    int sendnumber=0;
    int recvnumber=0;
    for(sendnumber=0; sendnumber<100; sendnumber++)
    {
        API_send(0,0,obj3,3,10);
        /// recvnumber += bulk_read(device,0x82,buf,64,20);
        ///  recvnumber += bulk_read(device,0x82,buf,64,20);
        /// recvnumber += bulk_read(device,0x82,buf,64,20);
    }
    printf("send %d time = %dms\n",sendnumber*3,clock()-start);
    printf("recvnumber = %d \n",rxnumber/19);








    /*for(int i=0;i<1000;i++)
        {
        r = bulk_read(device,0x82,buf,64,200);
         printf("%d\n",r);
         if(!r)
            break;
        }
    */

    libusb_release_interface(device, 0);
    libusb_close(device);
    libusb_exit(NULL);

    return 1;
}
#endif // 0
#include <libusb.h>
#include <stdio.h>
#include <time.h>
#include "libusbwin32_zlg.h"
// Enables this example to work with a device running the
// libusb-win32 PIC Benchmark Firmware.
#define BENCHMARK_DEVICE

//////////////////////////////////////////////////////////////////////////////
// TEST SETUP (User configurable)

// Issues a Set configuration request
#define TEST_SET_CONFIGURATION

// Issues a claim interface request
#define TEST_CLAIM_INTERFACE

// Use the libusb-win32 async transfer functions. see
// transfer_bulk_async() below.
#define TEST_ASYNC

// Attempts one bulk read.
#define TEST_BULK_READ

// Attempts one bulk write.
#define TEST_BULK_WRITE

//////////////////////////////////////////////////////////////////////////////
// DEVICE SETUP (User configurable)

// Device vendor and product id.
#define MY_VID 0x471
#define MY_PID 0x1200

// Device configuration and interface id.
#define MY_CONFIG 1
#define MY_INTF 0

// Device endpoint(s)
#define EP_IN 0x81
#define EP_OUT 0x01

// Device of bytes to transfer.
#define BUF_SIZE 64

//////////////////////////////////////////////////////////////////////////////






#if 0
int main(void)
{
    libusbwin32_zlg zlg;
    int ret=zlg.open(0);
    printf("zlg.open %d\n",ret);
    ret = zlg.dr_Version();
    printf("dr_Version %x\n",ret);
    ret = zlg.fw_Version();
    printf("fw_Version %x\n",ret);
    ret = zlg.can_number();
    printf("fw_Version %x\n",ret);
    char buf[64];
    memset(buf,0,64);
    ret = zlg.str_Serial_Num(buf,64);
    printf("str_Serial_Num [%s]\n",buf);
    memset(buf,0,64);
    ret = zlg.canname(buf,64);
    printf("str_Serial_Num [%s]\n",buf);
    ret = zlg.setBaud(0,1000000);
    printf("setBaud 0 [%d]\n",ret);
    ret = zlg.setBaud(1,1000000);
    printf("setBaud 1 [%d]\n",ret);
    ret = zlg.setFilter(0,0,0,0xffffffff);
    printf("setFilter 0  [%d]\n",ret);
    ret = zlg.setFilter(1,0,0,0xffffffff);
    printf("setFilter 1  [%d]\n",ret);

    ret = zlg.startPort(0);
    printf("startPort 0  [%d]\n",ret);
    ret = zlg.startPort(1);
    printf("startPort 1  [%d]\n",ret);

    vector<CAN_OBJ> objs;
    CAN_OBJ obj=
    {
        0x1ff,///UINT	ID;
        0,///UINT	TimeStamp;
        0,///BYTE	TimeFlag;
        2,///BYTE	SendType;00 是正常发送 01 单次发送 02 自收自发03 单次自收自发

        0,///BYTE	RemoteFlag;//是否是远程帧
        0,///BYTE	ExternFlag;//是否是扩展帧
        8,///BYTE	DataLen;
        {1,2,3,4,5,6,7,8}///BYTE	Data[8];
    };
    objs.push_back(obj);
    objs.push_back(obj);
    objs.push_back(obj);
    ///Sleep(100);
    int start = clock();
    int xxx;
#define TEST_NUM 5
    for(xxx=0; xxx<10; xxx++)
    {

        int clk = clock();
        for(int test=100; test<100+TEST_NUM; test++)
        {
            objs[0].ID=test;
            objs[1].ID=test;
            objs[2].ID=test;
            ret = zlg.send(1,objs,10);
            /// Sleep(10);
            /// printf("%d %d\n",test,clock()-clk);
            clk = clock();
        }
        clk = clock();
        for(int test=0; test<TEST_NUM; test++)
        {
            objs[0].ID=test;
            objs[1].ID=test;
            objs[2].ID=test;
            ret = zlg.send(0,objs,10);
            /// Sleep(10);
            /// printf("%d %d\n",test,clock()-clk);
            clk = clock();
        }
        ///Sleep(100);


        continue;
        vector<CAN_OBJ> rxobjs;
        int num = zlg.read(0,rxobjs,18,10);
        ///  printf("can 0 read %d\n",num);
        for(unsigned int i=0; i<rxobjs.size(); i++)
        {
            int time =0;
            if(i>0)
                time = rxobjs[i].TimeStamp-rxobjs[i-1].TimeStamp;
            /** printf("%d id=%x %x %x %x %x %x %x %x %x\n",time,rxobjs[i].ID,
                                rxobjs[i].Data[0],
                                rxobjs[i].Data[1],
                                rxobjs[i].Data[2],
                                rxobjs[i].Data[3],
                                rxobjs[i].Data[4],
                                rxobjs[i].Data[5],
                                rxobjs[i].Data[6],
                                rxobjs[i].Data[7]
                                );*/
        }

        rxobjs.clear();
        num = zlg.read(1,rxobjs,18,10);
        /// printf("can 1 read %d\n",num);
        for(unsigned int i=0; i<rxobjs.size(); i++)
        {
            int time =0;
            if(i>0)
                time = rxobjs[i].TimeStamp-rxobjs[i-1].TimeStamp;
            /** printf("%d id=%x %x %x %x %x %x %x %x %x\n",time,rxobjs[i].ID,
                                rxobjs[i].Data[0],
                                rxobjs[i].Data[1],
                                rxobjs[i].Data[2],
                                rxobjs[i].Data[3],
                                rxobjs[i].Data[4],
                                rxobjs[i].Data[5],
                                rxobjs[i].Data[6],
                                rxobjs[i].Data[7]

                                );*/
        }
    }

    printf("%d used time %d ms\n",xxx*TEST_NUM*2*3,clock()-start);

    Sleep(1000);
    printf("%d %d\n",zlg.getNumber(0),zlg.getNumber(1));
    Sleep(1000);
    ret = zlg.closePort(0);
    printf("closePort 0  [%d]\n",ret);
    ret = zlg.closePort(1);
    printf("closePort 1  [%d]\n",ret);

    return 0;

}
#endif // 0


#include "zlgusbcan.h"
int main()
{

    zlgusbcan zlg;
    if(zlg.open(VCI_USBCAN2,0)==false)
    {
        printf("open false!\n");
    }
    int baud=1000000;
    zlg.init(0,baud);
    zlg.init(1,baud);

    char data[8]= {0,1,2,3,4,5,6,7};
    int st=clock();
    vector<zlgusbcan::tag_canframe> tx;
    for(int i=0; i<800; i++)
    {
        zlgusbcan::tag_canframe frame;
        frame.id = i;
        frame.RemoteFlag=0;
        frame.ExternFlag=0;
        frame.len=8;
        memcpy(frame.data,data,8);
        tx.push_back(frame);

       // zlg.write(1,i,data,8,0);
        //Sleep(1);
    }
    zlg.write(0,2,tx);
    zlg.write(1,2,tx);
    st = clock()-st;

    Sleep(100);
    vector<zlgusbcan::tag_canframe> rx;
    int time = clock();
    zlg.read(rx);
    time = clock()-time;
    printf("rx=%d %dms\n",rx.size(),time);
    for(unsigned int i=0; i<rx.size(); i++)
    {
        printf("%d %d %.4f 0x%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\n",rx[i].num,rx[i].len,rx[i].time,(unsigned int)rx[i].id,

               rx[i].data[0],
               rx[i].data[1],
               rx[i].data[2],
               rx[i].data[3],
               rx[i].data[4],
               rx[i].data[5],
               rx[i].data[6],
               rx[i].data[7]
              );
    }
    printf("rx=%d %dms\n",rx.size(),time);
    printf("send time %dms\n",st);

    zlg.close();


    return 0;
}




