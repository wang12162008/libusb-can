#include <windows.h>
#include <stdio.h>
#include "ControlCAN.H"
#include "libusb.h"

/*#define MY_VID 0x1416
#define MY_PID 0x0001*/

#define ID_ERR 4

#define MY_VID (0x471+ID_ERR)
#define MY_PID (0x1200+ID_ERR)

#define CAN_BUF_MAX 16000
#define DEVS_MAX 16
#define CAN_OBJ VCI_CAN_OBJ
typedef struct
{
    unsigned char res;///
    unsigned char mode;///
    unsigned char Timing0;///
    unsigned char Timing1;///
    unsigned long FilterId;
    unsigned long FilterMask;
} __attribute__ ((packed)) tag_setup_can;

class mini_queue
{
public:
    int in;
    int out;
    int maxlen;
    CAN_OBJ* pbuf;
    mini_queue(int len)
    {
        in=0;
        out=0;
        maxlen = len;
        pbuf = new CAN_OBJ[len];

    }
    ~mini_queue()
    {
        delete []pbuf;
    }
    void clear()
    {
        in=0;
        out=0;
    }
    int size()
    {
        if(in==out)
            return 0;
        else
        {
            if(in>out)
                return in-out;
            else
                return maxlen+in-out;
        }

    }
    int isFull(void)
    {
        int buf = in+1;
        if(buf>=maxlen)
            buf=0;
        if(buf==out)
            return 1;
        else
            return 0;

    }
    int push(CAN_OBJ& obj)
    {
        if(isFull()==0)
        {
            pbuf[in]=obj;
            in++;
            if(in>=maxlen)
                in=0;
            return 1;
        }
        return 0;
    }
    int pop(CAN_OBJ& obj)
    {
        if(in!=out)
        {
            obj=pbuf[out];
            out++;
            if(out>=maxlen)
                out=0;
            return 1;

        }
        return 0;
    }

};
typedef struct
{
    unsigned char time;
unsigned char errcode;
unsigned char rx_error_count;
unsigned char tx_error_count;


} tag_can_err;
typedef struct
{
    libusb_device_handle* dev;
    struct libusb_transfer *bulk_in_transfer;
    tag_setup_can setup_can[2];
    tag_can_err err[2];
    unsigned char bulk_buf[64];
    int start;
    mini_queue *p_canbuf[2];

} tag_devs;
static tag_devs m_devs[DEVS_MAX];



__inline__ int usb_interrupt_write(libusb_device_handle *dev,char port,unsigned char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    libusb_interrupt_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;
}
__inline__ int usb_interrupt_read(libusb_device_handle *dev,char port,unsigned char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    libusb_interrupt_transfer(dev,port|0x80,bytes,len,&relen,timeout);
    return relen;
}
__inline__ int bulk_read(libusb_device_handle *dev,char port,unsigned char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    libusb_bulk_transfer(dev,port|0x80,bytes,len,&relen,timeout);
    return relen;
}
__inline__ int bulk_write(libusb_device_handle *dev,char port,unsigned char* bytes,int len,unsigned int timeout)
{
    int relen=0;
    libusb_bulk_transfer(dev,port,bytes,len,&relen,timeout);
    return relen;
}
__inline__ int interrupt_read(libusb_device_handle *dev,char port,unsigned char* bytes,int sendlen,unsigned char* recv_bytes,int recv_len)
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
    int len =usb_interrupt_read(dev,0x80+port,recv_bytes,recv_len,100);
    if(len<0)
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
    return len;
}
static int set_tx_usb_buf(int port,unsigned char* buf,CAN_OBJ* can_obj)
{
    unsigned long id = can_obj->ID;
    if(can_obj->ExternFlag)
        id|=0x80000000;
    if(can_obj->RemoteFlag)
        id|=0x40000000;
    buf[0] = id;
    buf[1] = id>>8;
    buf[2] = id>>16;
    buf[3] = id>>24;
    buf[4] = can_obj->DataLen|(port<<4);
    buf[5]=can_obj->SendType;

    for(int i=0; i<can_obj->DataLen; i++)
    {
        buf[6+i] = can_obj->Data[i];
    }
    return 14;
}
static void read_bulk_once(int index,unsigned char* rxbuf,unsigned int rxbufLen)
{
    for(unsigned int i=0; i<rxbufLen; i+=17)
    {
        if((rxbufLen-i)>=17)
        {
            if((rxbuf[8+i]&0x80)==0)///rx frame
            {
                CAN_OBJ obj;
                obj.TimeStamp = rxbuf[0+i]+rxbuf[1+i]*0x100+rxbuf[2+i]*0x10000+rxbuf[3+i]*0x1000000;
                obj.TimeFlag=1;
                obj.SendType=0;
                unsigned long id = rxbuf[4+i]+rxbuf[5+i]*0x100+rxbuf[6+i]*0x10000+rxbuf[7+i]*0x1000000;
                obj.ID = id&0x3fffffff;
                obj.DataLen =rxbuf[8+i]&0xf;
                obj.ExternFlag =(id&0x80000000)? 1:0;
                obj.RemoteFlag = (id&0x40000000)? 1:0;
                //printf("datalen %x %x\n",rxbuf[8+i],rxbuf[9+i]);
                for(int j=0; j<8; j++)
                {
                    obj.Data[j] = rxbuf[i+j+9];
                }
                if((rxbuf[8+i]&0x10)==0)///port 0
                {
                    m_devs[index].p_canbuf[0]->push(obj);

                }
                else///port 1
                {
                    m_devs[index].p_canbuf[1]->push(obj);
                }
            }
            else///error frame
            {

                unsigned long time =rxbuf[0+i]+rxbuf[1+i]*0x100+rxbuf[2+i]*0x10000+rxbuf[3+i]*0x1000000;
                char port=((rxbuf[8+i]&0x10)==0)?0:1;
                m_devs[index].err[port].time=time;
                m_devs[index].err[port].errcode =rxbuf[i+9];
                m_devs[index].err[port].rx_error_count =rxbuf[i+10];
                m_devs[index].err[port].tx_error_count =rxbuf[i+11];
                printf("can err %x\n",m_devs[index].err[port].errcode);

            }
        }
        else
        {
            break;
        }
    }

}
/**static int callback_cnt=0;
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
    read_bulk_once((int)transfer->user_data,m_devs[(int)transfer->user_data].bulk_buf,transfer->actual_length);
    ///printf("read %d\n",transfer->actual_length);
    ///printf("upload_img_B_over callback : %d \n",transfer->status);
    if(transfer->actual_length)
        callback_cnt++;

}*/
/**static void start_bulk_in(int index)
{
    if(index<DEVS_MAX)
    {
        m_devs[index].bulk_in_transfer = libusb_alloc_transfer(0);///分配一个异步传输
        if (!m_devs[index].bulk_in_transfer)
        {
            printf("bulkin_transfer ERR\n");
        }
        libusb_fill_bulk_transfer(m_devs[index].bulk_in_transfer, m_devs[index].dev, 0x82, m_devs[index].bulk_buf,
                                  sizeof(m_devs[index].bulk_buf), bulk_in_fn, (void*)index, 0);
        int r = libusb_submit_transfer(m_devs[index].bulk_in_transfer);   //提交一个传输并立即返回
        if (r < 0)
        {
            libusb_cancel_transfer(m_devs[index].bulk_in_transfer);   //异步取消之前提交传输
            printf("libusb_submit_transfer ERR\n");
        }
    }
}
static void stop_bulk_in(int index)
{
    if(index<DEVS_MAX)
    {
        libusb_cancel_transfer(m_devs[index].bulk_in_transfer);   //异步取消之前提交传输
    }
}*/
/**static int API_open_dev_sys(int index)
{
    printf("0x%x:0x%x\n",MY_VID,MY_PID);
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].start==0)
        {
            libusb_device **devs;
            int r;
            int i = 0;

            r = libusb_init(NULL);
            if (r < 0)
                return r;

            libusb_get_device_list(NULL, &devs);

            libusb_device *dev=0;
            int num=0;
            while ((dev = devs[i++]) != NULL)
            {
                struct libusb_device_descriptor desc;
                int r = libusb_get_device_descriptor(dev, &desc);
                if (r < 0)
                {
                    fprintf(stderr, "failed to get device descriptor");
                    return -1;
                }

                if (desc.idVendor == MY_VID
                        && desc.idProduct == MY_PID)
                {

                    if(num==index)
                    {
                        int r=libusb_open(dev,&m_devs[index].dev);
                        printf("finded dev %d\n",r);
                        break;

                    }
                    num++;

                }
                else
                {



                    printf("find VID=%x PID=%x\n",desc.idVendor,desc.idProduct);
                }
            }
            libusb_free_device_list(devs, 1);

            if(m_devs[index].dev)
            {

                libusb_set_auto_detach_kernel_driver(m_devs[index].dev, 1);

                int status = libusb_claim_interface(m_devs[index].dev, 0);
                if (status != LIBUSB_SUCCESS)
                {
                    libusb_close(m_devs[index].dev);
                    m_devs[index].dev = 0;
                    printf("libusb_claim_interface failed: %s\n", libusb_error_name(status));
                }

                if(!m_devs[index].p_canbuf[0] && !m_devs[index].p_canbuf[1])
                {
                    m_devs[index].p_canbuf[0] = new mini_queue(CAN_BUF_MAX);
                    m_devs[index].p_canbuf[1] = new mini_queue(CAN_BUF_MAX);
                }
                if(!m_devs[index].p_canbuf[0] || !m_devs[index].p_canbuf[1])
                {
                    return -10;
                }
                start_bulk_in(index);///异步方式
                m_devs[index].start=1;
                return 1;
            }
        }
    }
    return 0;
}*/
static int API_open_dev(int index)
{
    printf("0x%x:0x%x\n",MY_VID,MY_PID);
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].start==0)
        {
            libusb_device **devs;
            int r;
            int i = 0;

            r = libusb_init(NULL);
            if (r < 0)
                return r;

            libusb_get_device_list(NULL, &devs);

            libusb_device *dev=0;
            int num=0;
            while ((dev = devs[i++]) != NULL)
            {
                struct libusb_device_descriptor desc;
                int r = libusb_get_device_descriptor(dev, &desc);
                if (r < 0)
                {
                    fprintf(stderr, "failed to get device descriptor");
                    return -1;
                }

                if (desc.idVendor == MY_VID
                        && desc.idProduct == MY_PID)
                {

                    if(num==index)
                    {
                        int r=libusb_open(dev,&m_devs[index].dev);
                        printf("find VID=%x PID=%x\n",desc.idVendor,desc.idProduct);
                        printf("finded dev %d[%s]\n",r,libusb_error_name(r));
                        break;

                    }
                    num++;

                }
                else
                {



                    printf("find VID=%x PID=%x\n",desc.idVendor,desc.idProduct);
                }
            }
            libusb_free_device_list(devs, 1);

            if(m_devs[index].dev)
            {
                printf("auto detach %d\n",libusb_set_auto_detach_kernel_driver(m_devs[index].dev, 1));
                printf("detach %d\n",libusb_detach_kernel_driver(m_devs[index].dev, 0));
                printf("detach %d\n",libusb_detach_kernel_driver(m_devs[index].dev, 1));
                printf("set_configuration %d\n",libusb_set_configuration(m_devs[index].dev,1));

                int active=libusb_kernel_driver_active(m_devs[index].dev, 0);

                if(active!=1)
                {


                    int status = libusb_claim_interface(m_devs[index].dev, 0);
                    if (status != LIBUSB_SUCCESS)
                    {
                        libusb_close(m_devs[index].dev);
                        m_devs[index].dev = 0;
                        printf("libusb_claim_interface failed: %s[%d]\n", libusb_error_name(status),status);
                    }
                }

                if(!m_devs[index].p_canbuf[0] && !m_devs[index].p_canbuf[1])
                {
                    m_devs[index].p_canbuf[0] = new mini_queue(CAN_BUF_MAX);
                    m_devs[index].p_canbuf[1] = new mini_queue(CAN_BUF_MAX);
                }
                if(!m_devs[index].p_canbuf[0] || !m_devs[index].p_canbuf[1])
                {
                    return -10;
                }

                m_devs[index].start=1;
                return 1;
            }
            else
            {
                printf("open dev false!\n");
            }
        }
    }
    return 0;
}
static int API_dr_Version(int index)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(!m_devs[index].dev)
            return 0;
        unsigned char buf[64]= {0x12, 0x01, 0x01, 0x00};
        if(interrupt_read(m_devs[index].dev,1,buf,4,buf,64)<0)
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
static int API_fw_Version(int index)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(!m_devs[index].dev)
            return 0;
        unsigned char buf[64]= {0x12, 0x12, 0x01, 0x00};
        if(interrupt_read(m_devs[index].dev,1,buf,4,buf,64)<0)
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
/**
static int API_close_usb_sys(int index)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev)
        {
            stop_bulk_in(index);
            libusb_release_interface(m_devs[index].dev, 0);
            libusb_close(m_devs[index].dev);
            m_devs[index].dev = NULL;

            if(m_devs[index].p_canbuf[0])
                delete m_devs[index].p_canbuf[0];
            if(m_devs[index].p_canbuf[1])
                delete m_devs[index].p_canbuf[1];
            m_devs[index].start=0;
            return 1;

        }
    }
    return -1;
}*/
static int API_close_usb(int index)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev)
        {
            libusb_release_interface(m_devs[index].dev, 0);
            libusb_close(m_devs[index].dev);
            m_devs[index].dev = NULL;

            if(m_devs[index].p_canbuf[0])
                delete m_devs[index].p_canbuf[0];
            if(m_devs[index].p_canbuf[1])
                delete m_devs[index].p_canbuf[1];
            m_devs[index].start=0;
            return 1;

        }
    }
    return -1;
}
static int API_can_number(int index)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev)
        {
            unsigned char buf[64]= {0x12, 0x13, 0x01, 0x00};
            if(interrupt_read(m_devs[index].dev,1,buf,4,buf,64)<0)
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
    return -1;
}
static int API_str_Serial_Num(int index, char* str,int len)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev)
        {
            unsigned char buf[64]= {0x12, 0x14, 0x01, 0x00};
            if(interrupt_read(m_devs[index].dev,1,buf,4,(unsigned char*)str,len)<0)
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
/**static int API_canname(int index,unsigned char* str,int len)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev)
        {
            unsigned char buf[64]= {0x12, 0x15, 0x01, 0x00};
            if(interrupt_read(m_devs[index].dev,1,buf,4,str,len)<0)
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

    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev && port<2)
        {
            unsigned char buf[64]= {0x12,0x03,0x04,0x00,0x00,0x11,0x22};
            if(port==1)
            {
                buf[4]|=0x10;
            }
            buf[5] = time0;
            buf[6] = time1;

            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            ///printf("time0=%x  time1=%x\n",time0,time1);
            if(interrupt_read(m_devs[index].dev,1,buf,7,recv_buf,64)<0)
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
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev && port<2)
        {
            unsigned char buf[64]= {0x12,0x04,0x0a,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};
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
            if(interrupt_read(m_devs[index].dev,1,buf,13,recv_buf,64)<0)
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
}*/
static int API_closePort(int index,int port)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev && port<2)
        {
            unsigned char buf[64]= {0x12,0x0f,0x12,0x00,0x00};
            buf[0]=(port<<6);

            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            if(interrupt_read(m_devs[index].dev,1,buf,1,recv_buf,64)<0)
            {
                printf("error %s \n",__FUNCTION__);
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==1 &&
                    recv_buf[1]==buf[0]
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
static int API_get_setup(int index,int port,tag_setup_can* setup)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev && port<2)
        {
            unsigned char buf[64]= {0x12,0x0f,0x12,0x00,0x00};
            buf[0]=(port<<6)+2;

            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            int len=interrupt_read(m_devs[index].dev,1,buf,1,recv_buf,64);
            if(len<0)
            {
                printf("error %s \n",__FUNCTION__);
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==1 &&
                    recv_buf[1]==buf[0]
              )
            {

                memcpy(setup,(tag_setup_can*)(&recv_buf[1]),sizeof(tag_setup_can));
                /* for(int jjj=0;jjj<13;jjj++)
                 {
                     printf("[%x]",recv_buf[jjj]);
                 }
                 printf("\n");*/
                /* printf("%dmode=%d Timing0=0x%x Timing1=0x%x FilterId=0x%x FilterMask=0x%x\n",len,
                        setup->mode,setup->Timing0,setup->Timing1,setup->FilterId,setup->FilterMask);*/
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
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev && port<2)
        {
            unsigned char buf[64]= {0x12,0x0e,0x02,0x00,0x00};
            buf[0]=((port<<6)+1);
            memcpy(&buf[1],&m_devs[index].setup_can[port].mode,sizeof(tag_setup_can));

            for(unsigned int i=0; i<sizeof(tag_setup_can)-1; i++)
            {
                printf("[%x]",buf[i]);
            }
            printf("\n");
            unsigned char recv_buf[64];
            memset(recv_buf,0,64);
            if(interrupt_read(m_devs[index].dev,1,buf,sizeof(tag_setup_can)+1,recv_buf,64)<0)
            {
                printf("error %s \n",__FUNCTION__);
                return 0;
            }
            else
            {

            }
            if(recv_buf[0]==1 &&
                    recv_buf[1]==buf[0]
              )
            {
                tag_setup_can setup;
                API_get_setup(index,port,&setup);
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
int API_read(int index,int port,CAN_OBJ* objs,int maxLen)
{

        do
        {
            unsigned char buf[64];
            int len=bulk_read(m_devs[index].dev,2,buf,64,1);
            if(len)
                read_bulk_once(index,buf,len);
            else
                break;
        }
        while(1);




    int i=0;
    if(index<DEVS_MAX && port<2)
    {
        for(i=0; i<maxLen; i++)
        {
            if(m_devs[index].p_canbuf[port]->pop(objs[i])==0)
            {
                break;
            }

        }
    }

    return i;
}
static int API_getNumber(int index,int port)
{
    if(index<DEVS_MAX && port<2)
    {
        return m_devs[index].p_canbuf[port]->size();
    }
    return 0;
}
static int API_clearBuf(int index,int port)
{
    if(index<DEVS_MAX && port<2)
    {
        m_devs[index].p_canbuf[port]->clear();
        return 1;
    }
    return 0;
}
/**
最多发送4个
*/
static int API_send(int index,int port,CAN_OBJ* can_objs,unsigned int sendlen,unsigned int timeout)
{
    if(index>=0&& index<DEVS_MAX)
    {
        if(m_devs[index].dev && port<2)
        {
            unsigned char buf[64];
            int len=0;
            if(sendlen>4)
            {
                sendlen=4;
            }
            for(unsigned int i=0; i<4&&i<sendlen; i++)
            {
                len += set_tx_usb_buf(port,buf+len,&can_objs[i]);
            }
            if(len==0)
                return 0;
            // int start=clock();
            if(bulk_write(m_devs[index].dev,2,buf,len,0)<0)
            {
                printf("error %s bulk_write len=%d\n",__FUNCTION__,len);
                return -1;
            }
            unsigned char recv[64]= {0,0,0,0,0,0,0};

            if(usb_interrupt_read(m_devs[index].dev,0x81,recv,64,100)<0)
                //if(usb_interrupt_read(m_devs[index].dev,0x81,recv,64,-1)<0)
            {
                printf("usb_interrupt_read err\n");

                /// printf("error %s usb_interrupt_read %s\n",__FUNCTION__, libusb_strerror());
                return -2;
            }
            int ret = (recv[1]&0xf)+(recv[2]&0xf)+(recv[3]&0xf)+(recv[4]&0xf);
            if(recv[0]==0x13 && ret==sendlen)
            {
               ///printf("send success!!!!%x %x %x %x %x\n",recv[0],recv[1],recv[2],recv[3],recv[4]);

            }
            else
            {
                printf("send false!!!!%x %x %x %x %x\n",recv[0],recv[1],recv[2],recv[3],recv[4]);
                return -2;
            }
            // printf("%d\n",(int)(clock()-start));
            return ret;
        }
    }
    return 0;
}







EXTERNC DWORD __stdcall VCI_OpenDevice(DWORD DeviceType,DWORD DeviceInd,DWORD Reserved)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        int ret=API_open_dev(DeviceInd);
        if(ret>0)
        {
            return STATUS_OK;
        }
        printf("API_open_dev %d\n",ret);
    }
    return STATUS_ERR;
}
EXTERNC DWORD __stdcall VCI_CloseDevice(DWORD DeviceType,DWORD DeviceInd)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        API_closePort(DeviceInd,0);
        API_closePort(DeviceInd,1);
        if(API_close_usb(DeviceInd)>0)
        {
            return STATUS_OK;
        }
    }
    return STATUS_ERR;

}

EXTERNC DWORD __stdcall VCI_InitCAN(DWORD DeviceType, DWORD DeviceInd, DWORD CANInd, PVCI_INIT_CONFIG pInitConfig)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {
            m_devs[DeviceInd].setup_can[CANInd].mode=pInitConfig->Mode;
            m_devs[DeviceInd].setup_can[CANInd].Timing0=pInitConfig->Timing0;
            m_devs[DeviceInd].setup_can[CANInd].Timing1=pInitConfig->Timing1;
            m_devs[DeviceInd].setup_can[CANInd].FilterId=pInitConfig->AccCode;
            m_devs[DeviceInd].setup_can[CANInd].FilterMask=pInitConfig->AccMask;
            //printf("time01 %x %x-----------\n",pInitConfig->Timing0,pInitConfig->Timing1);
            return STATUS_OK;
        }
    }
    return STATUS_ERR;
}
EXTERNC DWORD __stdcall VCI_ReadBoardInfo(DWORD DeviceType,DWORD DeviceInd,PVCI_BOARD_INFO pInfo)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX)
        {
            pInfo->can_Num = API_can_number(DeviceInd);
            pInfo->dr_Version = API_dr_Version(DeviceInd);
            pInfo->fw_Version = API_fw_Version(DeviceInd);
            pInfo->hw_Version = 0x100;
            pInfo->in_Version = 0x100;
            pInfo->irq_Num = 0;
            strcpy(pInfo->str_hw_Type,"usb");
            API_str_Serial_Num(DeviceInd,pInfo->str_Serial_Num,sizeof(pInfo->str_Serial_Num));
            return STATUS_OK;
        }
    }
    return STATUS_ERR;

}
EXTERNC DWORD __stdcall VCI_ReadErrInfo(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd,PVCI_ERR_INFO pErrInfo)
{
    return STATUS_ERR;
}
EXTERNC DWORD __stdcall VCI_ReadCANStatus(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd,PVCI_CAN_STATUS pCANStatus)
{
    return STATUS_ERR;
}

EXTERNC DWORD __stdcall VCI_GetReference(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd,DWORD RefType,PVOID pData)
{
    return STATUS_ERR;
}
EXTERNC DWORD __stdcall VCI_SetReference(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd,DWORD RefType,PVOID pData)
{
    return STATUS_ERR;
}

EXTERNC ULONG __stdcall VCI_GetReceiveNum(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {

            return API_getNumber(DeviceInd,CANInd);
        }
    }
    return 0;
}
EXTERNC DWORD __stdcall VCI_ClearBuffer(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {
            API_clearBuf(DeviceInd,CANInd);
            return STATUS_OK;
        }
    }
    return STATUS_ERR;
}

EXTERNC DWORD __stdcall VCI_StartCAN(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {
            if(API_startPort(DeviceInd,CANInd)>0)
            {
                return STATUS_OK;
            }


        }
    }
    return STATUS_ERR;
}
EXTERNC DWORD __stdcall VCI_ResetCAN(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {
            API_closePort(DeviceInd,CANInd);
            if(API_startPort(DeviceInd,CANInd)>0)
            {
                return STATUS_OK;
            }
        }
    }
    return STATUS_ERR;
}
EXTERNC ULONG __stdcall VCI_Transmit(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd,PVCI_CAN_OBJ pSend,ULONG Len)
{
    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {
            unsigned int number=0;
            for(unsigned int i=0; i<Len; )
            {
                int ret=API_send(DeviceInd,CANInd,&pSend[i],Len-i,10);
                if(ret>0)
                {
                    number+=ret;
                    i+=ret;
                }
                else
                {
                    break;
                }
            }

            return number;
        }
    }
    return STATUS_ERR;
}

EXTERNC ULONG __stdcall VCI_Receive(DWORD DeviceType,DWORD DeviceInd,DWORD CANInd,PVCI_CAN_OBJ pReceive,ULONG Len,INT WaitTime)
{

    if(DeviceType==VCI_USBCAN1||DeviceType==VCI_USBCAN2)
    {
        if(DeviceInd<DEVS_MAX && CANInd<2)
        {
            unsigned int number=0;
            int start = clock();

            number=API_read(DeviceInd,CANInd,pReceive,Len);
            if(number==Len || WaitTime<=0)
            {
                return Len;
            }
            else
            {
                while(number<Len && ((clock()-start)<WaitTime))
                {
                    number += API_read(DeviceInd,CANInd,pReceive,Len-number);
                }

            }
            return number;
        }
    }
    return STATUS_ERR;









    for(unsigned int i=0; i<Len; i++)
    {

    }
}


