#include "zlgusbcan.h"
#include <time.h>

const char head[] =//"date Tue May 12 02:24:23 pm 2015""\n"
    "base hex  timestamps absolute""\n"
    "internal events logged""\n"
    "   0.000000 Start of measurement""\n"
    "   0.001807 CAN 1 Status:chip status error active""\n"
    "   0.001962 CAN 2 Status:chip status error active""\n"
    ;
const char ends[]="End TriggerBlock";



zlgusbcan::zlgusbcan()
    :m_file(0)
{
    m_connect[0] = 0;
    m_connect[1] = 0;
    m_rxNumber[0] = 0;
    m_rxNumber[1] = 0;
    m_lastStartTime = 0;
}

zlgusbcan::~zlgusbcan()
{
    if(m_file)
        delete m_file;
    close();
}
bool zlgusbcan::open(int devtype,int index)
{
    m_devtype = devtype;
    m_index = index;
    if(VCI_OpenDevice(devtype,index,0)!=STATUS_OK)
    {
        printf("open %d false\n",devtype);
        return false;
    }
    return true;
}
bool zlgusbcan::open(string devtype,string index)
{
    m_devtype=0;
    for(unsigned int i=0; i<sizeof(zlgusbcan::m_devTypes)/sizeof(zlgusbcan::m_devTypes[0]); i++)
    {
        if(strcmp(devtype.c_str(),m_devTypes[i].name.c_str())==0)
        {
            m_devtype = m_devTypes[i].index;
            break;
        }
    }
    if(m_devtype)
    {
        m_index = atoi(index.c_str());
        return open(m_devtype,m_index);
    }
    printf("open false %s %s",devtype.c_str(),index.c_str());
    return false;
}

bool zlgusbcan::init_E(int number,int baud,unsigned char Filter,unsigned long AccCode,unsigned long AccMask,int workMode)
{

    const DWORD GCanBrTab[10] =
    {
        0x060003, 0x060004, 0x060007,
        0x1C0008, 0x1C0011, 0x160023,
        0x1C002C, 0x1600B3, 0x1C00E0,
        0x1C01C1
    };
    unsigned long reg =0;
    if(baud == 1000000)
        reg =GCanBrTab[0];
    if(baud == 800000)
        reg =GCanBrTab[1];
    if(baud == 500000)
        reg =GCanBrTab[2];
    if(baud == 250000)
        reg =GCanBrTab[3];
    if(baud == 125000)
        reg =GCanBrTab[4];
    if(baud == 100000)
        reg =GCanBrTab[5];
    if(baud == 50000)
        reg =GCanBrTab[6];
    if(baud == 20000)
        reg =GCanBrTab[7];
    if(baud == 10000)
        reg =GCanBrTab[8];
    if(baud == 5000)
        reg =GCanBrTab[9];

    if (VCI_SetReference(m_devtype,m_index, number, 0, &reg) != STATUS_OK)
    {
        printf("set band false\n");
        return false;
    }

    VCI_INIT_CONFIG init_config;
    init_config.Mode=workMode;
    if(VCI_InitCAN(m_devtype,m_index,number,&init_config)!=STATUS_OK)
    {
        printf("init false\n");
        return false;
    }

    if (Filter!=2)
    {
        VCI_FILTER_RECORD filterRecord;
        filterRecord.ExtFrame=Filter;
        DWORD IDtemp;
        IDtemp=AccCode;
        //_stscanf_s(m_strStartID, _T("%d"), &IDtemp);
        filterRecord.Start = IDtemp;
        IDtemp=AccMask;
        //_stscanf_s(m_strEndID, _T("%d"), &IDtemp);
        filterRecord.End= IDtemp;
        if (filterRecord.Start>filterRecord.End)
        {
            printf("filterRecord false\n");
            return false;
        }
        //填充滤波表格
        VCI_SetReference(m_devtype, m_index, number, 1, &filterRecord);
        //使滤波表格生效
        if (VCI_SetReference(m_devtype, m_index, number, 2, NULL)!=STATUS_OK)
        {
            printf("set filterRecord false\n");
            return false;
        }
    }
    m_connect[number]=1;


    if(VCI_StartCAN(m_devtype,m_index,number)==1)
    {

    }
    else
    {
        printf("start false\n");
    }






    return true;
}


bool zlgusbcan::init(unsigned int number,unsigned int baud,unsigned char Filter,unsigned long AccCode,unsigned long AccMask)
{
    if(m_devtype==VCI_USBCAN_E_U || m_devtype==VCI_USBCAN_2E_U)
    {
        return init_E(number,baud,Filter,AccCode,AccMask);
    }
    if(number>=sizeof(m_connect)/sizeof(m_connect[0]))
    {
        return false;
    }
    typedef struct
    {
        unsigned int baud,tim0,tim1;
    } tag_tim01;
    const tag_tim01 tim01[]=
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

    VCI_INIT_CONFIG init_config;
    init_config.AccCode=AccCode;
    init_config.AccMask=AccMask;
    init_config.Filter=Filter;
    init_config.Mode=0;///0:正常 1：只听
    unsigned int i=0;
    for(i=0; i<sizeof(tim01)/sizeof(tim01[0]); i++)
    {
        if(baud == tim01[i].baud)
        {
            printf("i=%d baud=%d\n",i,tim01[i].baud);
            break;
        }

    }
    init_config.Timing0=(UCHAR)tim01[i].tim0;
    init_config.Timing1=(UCHAR)tim01[i].tim1;
   // VCI_BOARD_INFO infor;
   // VCI_ReadBoardInfo(m_devtype,m_index,&infor);
//   printf("time01 %x %x-----------\n",init_config.Timing0,init_config.Timing1);
    if(VCI_InitCAN(m_devtype,m_index,number,&init_config)!=STATUS_OK)
    {
        printf("init %d %d false\n",number,baud);
        return false;
    }
    if(VCI_StartCAN(m_devtype,m_index,number)==1)
    {

    }
    else
    {
        printf("start %d %d false\n",number,baud);
        return false;
    }


    m_connect[number]=1;


    return true;
}
bool zlgusbcan::init(string number,string baud)
{
    int mbaud = atoi(baud.c_str());
    mbaud *= 1000;
    printf("mbaud %d\n",mbaud);
    bool ret = false;
    if(strcmp(number.c_str(),"0")==0)
    {
        ret = init(0,mbaud);
    }
    else if(strcmp(number.c_str(),"1")==0)
    {
        ret = init(1,mbaud);
    }
    else if(strcmp(number.c_str(),"all")==0)
    {
        ret = init(0,mbaud);
        if(ret == true)
            ret = init(1,mbaud);
    }
    return ret;
}
int zlgusbcan::read(vector<tag_canframe>& frame)
{
    int ret=read(0,frame);
    ret+=read(1,frame);
    sort(frame.begin(),frame.end());//用<比较

    if(m_lastStartTime==0 && frame.size())
    {
        unsigned long mint=0xffffffff;
        for(unsigned int i=0; i<frame.size(); i++)
        {
            unsigned int time = frame[i].time*10000;
            if(time<mint)
                mint = time;
        }
        m_lastStartTime = mint;
        for(unsigned int i=0; i<frame.size(); i++)
        {
            frame[i].time=frame[i].time-mint/10000.f;
        }
    }
    if(m_file)
    {
        for(unsigned int i=0; i<frame.size(); i++)
        {
            char buf[500];
            sprintf(buf,"%.6f 1  %xx        Rx   d 8 %02x %02x %02x %02x %02x %02x %02x %02x\n",frame[i].time,(unsigned int)frame[i].id,
                    frame[i].data[0],frame[i].data[1],frame[i].data[2],frame[i].data[3],
                    frame[i].data[4],frame[i].data[5],frame[i].data[6],frame[i].data[7]
                   );
            m_file->write(buf);
        }
    }

    return ret;

}
int zlgusbcan::read(unsigned int number,vector<tag_canframe>& frame)
{
    VCI_CAN_OBJ frameinfo[1000];
    VCI_ERR_INFO errinfo;

    if(number<sizeof(m_connect)/sizeof(m_connect[0]))
        if(m_connect[number])
        {
            int len=0;
            do
            {
                if(m_devtype==VCI_USBCAN_E_U || m_devtype==VCI_USBCAN_2E_U)
                {
                    len = VCI_GetReceiveNum(m_devtype,m_index,number);
                    if(len<=0)
                        return 0;
                    if(len>=1000)
                        len=VCI_Receive(m_devtype,m_index,number,frameinfo,1000,200);
                    else
                        len=VCI_Receive(m_devtype,m_index,number,frameinfo,len,200);
                }
                else
                {
                    len=VCI_Receive(m_devtype,m_index,number,frameinfo,1000,10);
                }



                if(len<=0)
                {
                    //注意：如果没有读到数据则必须调用此函数来读取出当前的错误码，
                    //千万不能省略这一步（即使你可能不想知道错误码是什么）
                    VCI_ReadErrInfo(m_devtype,m_index,number,&errinfo);
                }
                else
                {
                    m_rxNumber[number]+=len;
                    tag_canframe fra;
                    fra.num = number;
                    for(int i=0; i<len; i++)
                    {
                        fra.time = (frameinfo[i].TimeStamp-m_lastStartTime)/10000.0f;
                        fra.id = frameinfo[i].ID;
                        fra.len =frameinfo[i].DataLen;
                        fra.data[0] = frameinfo[i].Data[0];
                        fra.data[1] = frameinfo[i].Data[1];
                        fra.data[2] = frameinfo[i].Data[2];
                        fra.data[3] = frameinfo[i].Data[3];
                        fra.data[4] = frameinfo[i].Data[4];
                        fra.data[5] = frameinfo[i].Data[5];
                        fra.data[6] = frameinfo[i].Data[6];
                        fra.data[7] = frameinfo[i].Data[7];
                        frame.push_back(fra);



                        /*printf("%.3f %.3f\t%1d\t%x\t%x %x %x %x %x %x %x %x\n",
                               fra.time,fra.time-clock()/1000.0f,fra.num,fra.id,
                               fra.data[0],
                               fra.data[1],
                               fra.data[2],
                               fra.data[3],
                               fra.data[4],
                               fra.data[5],
                               fra.data[6],
                               fra.data[7]
                              );*/
                    }
                }
            }
            while(len==1000);
        }
    return 0;
}
int zlgusbcan::read(unsigned int number,unsigned long* id,unsigned char* data)
{
    VCI_CAN_OBJ frameinfo;
    VCI_ERR_INFO errinfo;

    if(number<sizeof(m_connect)/sizeof(m_connect[0]))
        if(m_connect[number])
        {
            int len=0;

            if(m_devtype==VCI_USBCAN_E_U || m_devtype==VCI_USBCAN_2E_U)
            {
                len = VCI_GetReceiveNum(m_devtype,m_index,number);
                if(len<=0)
                    return 0;
                len=VCI_Receive(m_devtype,m_index,number,&frameinfo,1,200);

            }
            else
            {
                len=VCI_Receive(m_devtype,m_index,number,&frameinfo,1,10);
            }



            if(len<=0)
            {
                //注意：如果没有读到数据则必须调用此函数来读取出当前的错误码，
                //千万不能省略这一步（即使你可能不想知道错误码是什么）
                VCI_ReadErrInfo(m_devtype,m_index,number,&errinfo);
            }
            else
            {
                *id = frameinfo.ID;
                len = frameinfo.DataLen;
                for(int i=0; i<len; i++)
                    data[i] = frameinfo.Data[i];
                return len;
            }
        }
    return 0;
}
bool zlgusbcan::write(unsigned int number,unsigned long id,void* data,int datalen,int ExternFlag,int RemoteFlag,int SendType)
{
    if(number<sizeof(m_connect)/sizeof(m_connect[0]))
        if(m_connect[number])
        {
            VCI_CAN_OBJ frameinfo;
            frameinfo.DataLen=datalen;
            memcpy(&frameinfo.Data,data,datalen);
            frameinfo.RemoteFlag=RemoteFlag;
            frameinfo.ExternFlag=ExternFlag;
            frameinfo.ID=id;
            frameinfo.SendType=SendType;

            if(m_devtype==VCI_USBCAN_E_U || m_devtype==VCI_USBCAN_2E_U)
            {
                int m_sendTimeout = 200;
                VCI_SetReference(m_devtype,m_index,number,4,&m_sendTimeout);//设置发送超时
            }

            if(VCI_Transmit(m_devtype,m_index,number,&frameinfo,1)==1)
            {
                return true;
            }
            else
            {
                printf("write false\n");
                return false;
            }
        }

    return false;
}
bool zlgusbcan::setFileSave()
{
    m_file = new myFile("new.asc","wa+");

    time_t nowtime = time(NULL);
    struct tm* newtime;
    newtime = gmtime(&nowtime);

//m_file->write("date Tue May 12 02:24:23 pm 2015""\n");
    m_file->write("date ");
    m_file->write(asctime(newtime));
//m_file->write("\n");
    m_file->write(head);
    return false;
}
int zlgusbcan::write(unsigned int number,unsigned int type,vector<tag_canframe>& frame)
{
if(number<sizeof(m_connect)/sizeof(m_connect[0]))
        if(m_connect[number])
        {
            VCI_CAN_OBJ* pframeinfo=new VCI_CAN_OBJ[frame.size()];
            for(int i=0;i<frame.size();i++)
            {
                pframeinfo[i].DataLen=frame[i].len;
            memcpy(&pframeinfo[i].Data,frame[i].data,frame[i].len);
            pframeinfo[i].RemoteFlag=frame[i].RemoteFlag;
            pframeinfo[i].ExternFlag=frame[i].ExternFlag;
            pframeinfo[i].ID=frame[i].id&0x3fffffff;
            pframeinfo[i].SendType=type;

            }


            printf("send\n");
            if(VCI_Transmit(m_devtype,m_index,number,&pframeinfo[0],frame.size())>0)
            {
                delete pframeinfo;
                return true;
            }
            else
            {
                delete pframeinfo;
                printf("write false\n");
                return false;
            }
        }

    return false;

}

void zlgusbcan::close()
{
    if(m_file)
    {
        m_file->write(ends);
        time_t t = time(0);
        char tmp[64];
        //strftime( tmp, sizeof(tmp), "%Y%m%d-%X.asc",localtime(&t) );
        strftime( tmp, sizeof(tmp), "%Y%m%d_%X.asc",localtime(&t) );
        for(int i=0; i<64; i++)
        {
            if(tmp[i]==':')
                tmp[i] = '-';
        }
        m_file->close();
        rename(m_file->m_name,tmp);
        m_file = 0;
    }

    m_connect[0] = 0;
    m_connect[1] = 0;
    m_rxNumber[0] = 0;
    m_rxNumber[1] = 0;
    m_lastStartTime = 0;
    VCI_CloseDevice(m_devtype,m_index);
}

zlgusbcan::tag_devType zlgusbcan::m_devTypes[5]=
{
    {"VCI_USBCAN2",		4},
    {"VCI_USBCAN1",		3},
    {"VCI_USBCAN2A",	4},
    {"VCI_USBCAN_E_U",	20},
    {"VCI_USBCAN_2E_U", 21},
};
