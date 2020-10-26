#ifndef ZLGUSBCAN_H
#define ZLGUSBCAN_H
#include <windows.h>
extern "C"
{
#include "ControlCAN.h"
}
#include "myFile.h"
#include <algorithm>
//#pragma comment(lib,"controlcan.lib")

#include <stdio.h>
#include <vector>
#include <string>
using namespace std;
class zlgusbcan
{
public:
    typedef struct devType
    {
       string name;
       int index;

    } tag_devType;
    static tag_devType m_devTypes[5];
    typedef struct canframe
    {
        int num;
        float time;
        unsigned long id;
        char len;
        char RemoteFlag;
        char ExternFlag;
        unsigned char data[8];
        bool operator()(const canframe* t1,const canframe* t2)
        {
            return t1->time < t2->time;
        }
        bool operator < (const canframe& ti) const
        {
            //  printf("Operator<:%d/n",ti.index);
            //return index < ti.index;
            return time < ti.time;
        }
    } tag_canframe;
    zlgusbcan();
    virtual ~zlgusbcan();
    int m_connect[2];
    int m_rxNumber[2];
    int m_devtype;
    int m_index;
    myFile *m_file;
    unsigned long m_lastStartTime;
    bool open(int devtype=VCI_USBCAN2,int index=0);
    bool open(string devtype="VCI_USBCAN2",string index="0");
    bool setFileSave();


    bool init(unsigned int number=0,unsigned int baud=250000,unsigned char Filter=2,unsigned long AccCode=0,unsigned long AccMask=0xffffffff);
    bool init(string number="0",string baud="250k");



    bool init_E(int number=0,int baud=250000,
                unsigned char Filter=2,unsigned long AccCode=0,
                unsigned long AccMask=0xffffffff,int workMode=0);
    int read(vector<tag_canframe>& frame);
    int write(unsigned int number,unsigned int type,vector<tag_canframe>& frame);
    int read(unsigned int number,unsigned long* id,unsigned char* data);
    bool write(unsigned int number,unsigned long id,void* data,
               int datalen=8,int ExternFlag=1,int RemoteFlag=0,int SendType=2);
    void close();
protected:
private:
    int read(unsigned int number,vector<tag_canframe>& frame);
};

#endif // ZLGUSBCAN_H
