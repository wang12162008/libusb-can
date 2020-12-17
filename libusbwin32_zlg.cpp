#include "libusbwin32_zlg.h"
#include "libusb.h"
#include <time.h>
///  usblib to usblib-win32
#define usb_dev_handle libusb_device_handle

int usb_interrupt_write(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    int ret=libusb_interrupt_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;


}
int usb_interrupt_read(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    /// port |= 0x80;
    int ret=libusb_interrupt_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;
    /*int ret=libusb_control_transfer(dev,port,0,0,0,bytes,len,timeout);
     return ret;*/


}
int bulk_write(libusb_device_handle *dev,char port,char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    int ret=libusb_bulk_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;
}


///end usblib to usblib-win32
#define PRINTF_LIBUSB_ERR 0
#define CAN_BUF_MAX 16000
#define DEVS_MAX 16
char bulkbuf[64];
usb_dev_handle* devs[DEVS_MAX]= {0};
typedef struct
{
    int start;
    CAN_OBJ* p_canbuf[2];
    int canbuf_in[2];
    int canbuf_out[2];
} tag_thread_dev;
tag_thread_dev thread_devs[DEVS_MAX];
void push_thread_devs(int index,int port,CAN_OBJ obj)
{
    if(index<DEVS_MAX&& port<2)
    {
        int next_in = thread_devs[index].canbuf_in[port]+1;
        if(next_in>=CAN_BUF_MAX)
        {
            next_in=0;
        }
        if(next_in!=thread_devs[index].canbuf_out[port])
        {
            thread_devs[index].p_canbuf[port][thread_devs[index].canbuf_in[port]]=obj;

            thread_devs[index].canbuf_in[port] = next_in;
        }

    }
}
int pop_thread_devs(int index,int port,CAN_OBJ* obj)
{
    if(index<DEVS_MAX&& port<2)
    {
        if(thread_devs[index].canbuf_out[port]!=thread_devs[index].canbuf_in[port])
        {
            int next_out = thread_devs[index].canbuf_out[port]+1;
            if(next_out>=CAN_BUF_MAX)
            {
                next_out=0;
            }
            *obj = thread_devs[index].p_canbuf[port][thread_devs[index].canbuf_out[port]];
            thread_devs[index].canbuf_out[port] = next_out;
            return 1;
        }
    }
    return 0;
}

void read_bulk_once(int index,char* rxbuf,int rxbufLen)
{
    for(unsigned int i=0; i<rxbufLen; i+=19)
    {
        if((rxbufLen-i)>=19)
        {
            if(rxbuf[4+i]==0)///rx frame
            {
                CAN_OBJ obj;
                obj.TimeStamp = rxbuf[0+i]+rxbuf[1+i]*0x100+rxbuf[2+i]*0x10000+rxbuf[3+i]*0x1000000;
                obj.TimeFlag=1;
                obj.SendType=0;
                obj.DataLen =rxbuf[6+i]&0xf;
                obj.ID =rxbuf[7+i]+rxbuf[8+i]*0x100+rxbuf[9+i]*0x10000+rxbuf[10+i]*0x1000000;
                if(rxbuf[6+i]&0x80)
                {
                    obj.ExternFlag = 1;
                    obj.ID>>=3;
                }
                else
                {
                    obj.ExternFlag = 0;
                    obj.ID>>=5;
                }
                obj.RemoteFlag = (rxbuf[6+i]&0x40)?1:0;
                for(int j=0; j<8; j++)
                {
                    obj.Data[j] = rxbuf[i+j+11];
                }
                if((rxbuf[6+i]&0x10)==0)///port 0
                {
                    push_thread_devs(index,0,obj);


                }
                else///port 1
                {
                    push_thread_devs(index,1,obj);
                }
            }
            else///error frame
            {

            }
        }
        else
        {
            break;
        }
    }
    /// printf("rx %d %f\n",rxbuf.size(),rxbuf.size()/19.0f);
    return 1;
}
struct libusb_transfer *bulkin_transfer[DEVS_MAX]= {NULL};
static void LIBUSB_CALL bulk_in_fn(struct libusb_transfer *transfer)
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
    read_bulk_once((int)transfer->user_data,bulkbuf,transfer->actual_length);

    ///printf("read %d\n",transfer->actual_length);
    ///printf("upload_img_B_over callback : %d \n",transfer->status);

}
void start_thread(int index)
{
    if(index<DEVS_MAX)
    {
        bulkin_transfer[index] = libusb_alloc_transfer(0);///分配一个异步传输
        if (!bulkin_transfer)
        {
            printf("bulkin_transfer ERR\n");
        }
        libusb_fill_bulk_transfer(bulkin_transfer[index], devs[index], 0x82, bulkbuf,
                                  sizeof(bulkbuf), bulk_in_fn, (void*)index, 5000);
        int r = libusb_submit_transfer(bulkin_transfer[index]);   //提交一个传输并立即返回
        if (r < 0)
        {
            libusb_cancel_transfer(bulkin_transfer[index]);   //异步取消之前提交传输
            printf("1");
        }
    }
}



static int API_open_dev(int index)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        libusb_device **devs;
        int r;
        int i = 0;
        ssize_t cnt;
        int finded=0;

        r = libusb_init(NULL);
        if (r < 0)
            return r;

        cnt = libusb_get_device_list(NULL, &devs);

        libusb_device *dev=0;
        int num=0;
        while ((dev = devs[i++]) != NULL)
        {
            struct libusb_device_descriptor desc;
            int r = libusb_get_device_descriptor(dev, &desc);
            if (r < 0)
            {
                fprintf(stderr, "failed to get device descriptor");
                return;
            }

            if (desc.idVendor == VID
                    && desc.idProduct == PID)
            {

                if(num==index)
                {
                    libusb_open(dev,&::devs[index]);
                    break;

                }
                num++;

            }
        }
        libusb_free_device_list(devs, 1);

        if(::devs[index])
        {
            libusb_set_auto_detach_kernel_driver(::devs[index], 1);

            int status = libusb_claim_interface(::devs[index], 0);
            if (status != LIBUSB_SUCCESS)
            {
                libusb_close(::devs[index]);
                printf("libusb_claim_interface failed: %s\n", libusb_error_name(status));
            }

            if(!thread_devs[index].p_canbuf[0] && !thread_devs[index].p_canbuf[1])
            {
                thread_devs[index].p_canbuf[0] = malloc(sizeof(CAN_OBJ)*CAN_BUF_MAX);
                thread_devs[index].p_canbuf[1] = malloc(sizeof(CAN_OBJ)*CAN_BUF_MAX);
            }
            if(!thread_devs[index].p_canbuf[0] || !thread_devs[index].p_canbuf[1])
            {
                return -10;
            }
            start_thread(index);
        }
    }
    return 0;
}
static int interrupt_read(usb_dev_handle *dev,char port,unsigned char* bytes,int sendlen,unsigned char* recv_bytes,int recv_len)
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


int set_tx_usb_buf(int port,char* buf,CAN_OBJ* can_obj)
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
int API_dr_Version(int index)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {

        if(!devs[index])
            return 0;
        char buf[64]= {0x12, 0x01, 0x01, 0x00};
        if(interrupt_read(devs[index],1,buf,4,buf,64)<0)
        {
            printf("error dr_Version \n");
            return 0;
        }
        else
        {
            ///printf("interrupt_read %x %x\n",buf[4],buf[5]);
        }
        return (buf[5]*0x100+buf[4]);
    }
    return 0;
}
int API_fw_Version(int index)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(!devs[index])
            return 0;
        char buf[64]= {0x12, 0x12, 0x01, 0x00};
        if(interrupt_read(devs[index],1,buf,4,buf,64)<0)
        {
            printf("error fw_Version \n");
            return 0;
        }
        else
        {
            ///printf("interrupt_read %x %x\n",buf[4],buf[5]);
        }
        return (buf[5]*0x100+buf[4]);
    }
    return 0;
}
void API_close_usb(int index)
{
    if(index>=0&& index<sizeof(devs)/sizeof(devs[0]))
    {
        if(devs[index])
        {
            libusb_release_interface(devs[index], 0);
            libusb_close(devs[index]);
            devs[index] = NULL;

            if(thread_devs[index].p_canbuf[0])
                free(thread_devs[index].p_canbuf[0]);
            if(thread_devs[index].p_canbuf[1])
                free(thread_devs[index].p_canbuf[1]);
            thread_devs[index].start=0;



        }
    }
}
int API_can_number(int index)
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
int API_str_Serial_Num(int index,char* str,int len)
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
int API_canname(int index,char* str,int len)
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
int API_set_time0_time1(int index,int port,int time0,int time1)
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
            buf[5] = time0;
            buf[6] = time1;
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
int API_setFilter(int index,int port,int mode,unsigned long code,unsigned long mask)
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
int API_closePort(int index,int port)
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
int API_startPort(int index,int port)
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
/**
API_read read max 300 frame
port0objs/port1objs CAN_OBJ[300]
*/
int API_read(int index,int port,CAN_OBJ* objs,int maxLen)
{
    int i=0;
    for(i=0; i<maxLen; i++)
    {
        if(pop_thread_devs(index,port,&objs[i])==0)
        {
            break;
        }
    }
    return i;
}
int API_send(int index,int port,CAN_OBJ* can_objs,int sendlen,int timeout)
{
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

            if(bulk_write(devs[index],2,buf,len,0)<0)
            {
                printf("error %s bulk_write len=%d\n",__FUNCTION__,len);
                return -1;
            }
            unsigned char recv[64]= {0,0,0};
            /* int ret=0;
             while((ret=usb_interrupt_read(devs[index],0x81,(char*)recv,64,0))<0)
             {

             }
              if(ret<0)
             */
            ///if(usb_interrupt_read(devs[index],0x81,(char*)recv,64,-1)<0)
            int start=clock();
            if(usb_interrupt_read(devs[index],0x81,(char*)recv,64,-1)<0)
            {
                printf("usb_interrupt_read err\n");

                /// printf("error %s usb_interrupt_read %s\n",__FUNCTION__, libusb_strerror());
                return -2;
            }
            if(recv[0]==0x13 && recv[2]>0)
            {

            }
            else
            {
                printf("send false!!!!\n");
                return -2;
            }
            printf("%d\n",clock()-start);
            return recv[2];
        }
    }
    return 0;
}
libusbwin32_zlg::libusbwin32_zlg()
{
    m_index =-1;
    pRxbuf_can0 = new mini_queue(100000);
    pRxbuf_can1 = new mini_queue(100000);
}

libusbwin32_zlg::~libusbwin32_zlg()
{
    API_close_usb(m_index);
    delete pRxbuf_can0;
    delete pRxbuf_can1;
}
int libusbwin32_zlg::open(int index)
{
    m_index = index;
    return API_open_dev(index);
}
int libusbwin32_zlg::dr_Version(void)
{
    return API_dr_Version(m_index);
}
int libusbwin32_zlg::fw_Version(void)
{
    return API_fw_Version(m_index);
}
int libusbwin32_zlg::can_number(void)
{
    return API_can_number(m_index);

}
int libusbwin32_zlg::str_Serial_Num(char* str,int len)
{
    return API_str_Serial_Num(m_index,str,len);
}
int libusbwin32_zlg::canname(char* str,int len)
{
    return API_canname(m_index,str,len);
}
int libusbwin32_zlg::setBaud(int port,int baud)
{
    if(port>1)
    {
        printf("error setBaud port=%d\n",port);
        return 0;
    }

    int tim0=0;
    int tim1=0;
    typedef struct
    {
        int baud,tim0,tim1;
    } tag_tim01;
    static const tag_tim01 tim01[]=
    {
        {1000000, 0x00,0x14},		//1000Kbps
        {800000, 0x00,0x16},		// 800Kbps
        {500000, 0x00,0x1C},		// 500Kbps
        {250000, 0x01,0x1C},		// 250Kbps
        {125000, 0x03,0x1C},		// 125Kbps
        {100000, 0x04,0x1C},	    // 100Kbps
        {50000,  0x09,0x1C},	 // 50Kbps
        {20000, 0x18,0x1C},		 // 20Kbps
        {10000, 0x31,0x1C},		 // 10Kbps
        {5000, 0xBF,0xFF},		  // 5Kbps
    };
    for(unsigned int i=0; i<sizeof(tim01)/sizeof(tim01[0]); i++)
    {
        if(baud == tim01[i].baud)
        {
            tim0 = tim01[i].tim0;
            tim1 = tim01[i].tim1;
            break;
        }
    }
    if(tim0==0 && tim1==0)
    {
        printf("error setBaud band=%d\n",baud);
        return 0;
    }
    return API_set_time0_time1(m_index,port,tim0,tim1);


}
/**
mode 0: single; 1:double
**/
int libusbwin32_zlg::setFilter(int port,int mode,unsigned long code,unsigned long mask)
{
    return API_setFilter(m_index,port,mode,code,mask);
}
int libusbwin32_zlg::closePort(int port)
{
    return API_closePort(m_index,port);
}
int libusbwin32_zlg::startPort(int port)
{
    int ret = API_startPort(m_index,port);
    if(ret>0)
    {
        if(port==1)
        {
            pRxbuf_can1->clear();

        }
        else
        {
            pRxbuf_can0->clear();
        }
    }
    return ret;
}
int libusbwin32_zlg::recv(void)
{
    CAN_OBJ obj[300];
    int len=0;
    len=API_read(m_index,0,obj,300);
    for(int i=0; i<len; i++)
    {
        pRxbuf_can0->push(obj[i]);
    }
    len=API_read(m_index,1,obj,300);
    for(int i=0; i<len; i++)
    {
        pRxbuf_can1->push(obj[i]);
    }
    return 1;
}


int libusbwin32_zlg::send(int port,vector<CAN_OBJ>& can_objs,unsigned int timeout)
{
    int ret=0;
    for(int i=0; i<can_objs.size(); i+=3)
    {
        CAN_OBJ objs[3];
        int j;
        for(j=0; j<3; j++)
        {
            if(can_objs.size()-i-j)
            {
                objs[j] = can_objs[i+j];
            }
            else
            {
                break;
            }
        }
        if(j>0)
        {
            ///printf("send %d\n",j);
            int len=API_send(m_index,port,objs,j,timeout);
            if(len>0)
            {
                ret += len;
            }
        }
    }
    return ret;
}
int libusbwin32_zlg::getNumber(int port)
{
    recv();
    if(port==0)
        return pRxbuf_can0->size();
    else
        return pRxbuf_can1->size();
}
int libusbwin32_zlg::read(int port,vector<CAN_OBJ>& can_objs,unsigned int len,unsigned int timeout)
{
    unsigned int number=0;
    unsigned int ms=clock();
    while(number<len && (clock()-ms)<timeout)
    {
        number = getNumber(port);
    }
    CAN_OBJ obj;
    if(port==0)
    {
        for(int i=0; i<number; i++)
        {
            pRxbuf_can0->pop(obj);
            can_objs.push_back(obj);
        }
    }
    else
    {
        for(int i=0; i<number; i++)
        {
            pRxbuf_can1->pop(obj);
            can_objs.push_back(obj);
        }
    }
    return number;
}
