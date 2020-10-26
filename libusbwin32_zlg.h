#ifndef LIBUSBWIN32_ZLG_H
#define LIBUSBWIN32_ZLG_H
#include <windows.h>
#include <stdio.h>
#include <vector>
using namespace std;
#define VID 0x471
#define PID 0x1200
       typedef  struct  CAN_OBJ{
	UINT	ID;
	UINT	TimeStamp;
	BYTE	TimeFlag;
	BYTE	SendType;///00 是正常发送 01 单次发送 02 自收自发03 单次自收自发

	BYTE	RemoteFlag;//是否是远程帧
	BYTE	ExternFlag;//是否是扩展帧
	BYTE	DataLen;
	BYTE	Data[8];

}CAN_OBJ,*PCAN_OBJ;

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
     if(in==out)return 0;
     else
     {
         if(in>out)return in-out;
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
 void pop(CAN_OBJ& obj)
 {
     if(in!=out)
     {
         obj=pbuf[out];
         out++;
         if(out>=maxlen)
         out=0;


     }
 }

};

class libusbwin32_zlg
{
    public:
        mini_queue* pRxbuf_can0;
        mini_queue* pRxbuf_can1;
        libusbwin32_zlg();
        virtual ~libusbwin32_zlg();
        int open(int index);
        int dr_Version(void);
        int fw_Version(void);
        int can_number(void);
        int str_Serial_Num(char* str,int len);
        int canname(char* str,int len);
        int setBaud(int port,int baud);
        int setFilter(int port,int mode,unsigned long code,unsigned long mask);
        int startPort(int port);
        int closePort(int port);
        int recv(void);
        int send(int port,vector<CAN_OBJ>& can_objs,unsigned int timeout);
        int getNumber(int port);
        int read(int port,vector<CAN_OBJ>& can_objs,unsigned int len=1,unsigned int timeout=100);
    protected:
    private:
        ///usb_dev_handle *dev;
        int m_index;
        int set_tx_usb_buf(int port,char* buf,CAN_OBJ& can_obj);
};

#endif // LIBUSBWIN32_ZLG_H
