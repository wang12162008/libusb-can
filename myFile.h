#ifndef MYFILE_H
#define MYFILE_H
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <windows.h>
using namespace std;
class myFile
{
    public:
        FILE* m_pf;
        char m_name[500];
        myFile(const char* name,const char* opt);
        int read(void* pdata,int len);
        int write(void* pdata,int len);
        int write(const char* str);
        int seed(int pos);
        int size();
        void close();
        virtual ~myFile();
    protected:
    private:
};

#endif // MYFILE_H
