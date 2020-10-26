#include "myFile.h"
BOOL FindFirstFileExists(LPCTSTR lpPath, DWORD dwFilter)
{
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(lpPath, &fd);
    BOOL bFilter = (FALSE == dwFilter) ? TRUE : fd.dwFileAttributes & dwFilter;
    BOOL RetValue = ((hFind != INVALID_HANDLE_VALUE) && bFilter) ? TRUE : FALSE;
    FindClose(hFind);
    return RetValue;
}
BOOL FilePathExists(LPCTSTR lpPath)
{
    return FindFirstFileExists(lpPath, FALSE);
}
BOOL FolderExists(LPCTSTR lpPath)
{
    return FindFirstFileExists(lpPath, FILE_ATTRIBUTE_DIRECTORY);
}
myFile::myFile(const char* name,const char* opt)
{
    unsigned int i=0;
    strcpy(m_name,name);
    char dir[500];
    for(i=0;i<strlen(name);i++)
    {
        dir[i]=name[i];
        if(dir[i]=='/'||dir[i]=='\\')
        {
            dir[i+1]=0;
             if(FolderExists(dir)==false)
             {
                printf("[%s]\n",dir);
                mkdir(dir);
             }
        }
    }
    m_pf = fopen(name,opt);
}
int myFile::read(void* pdata,int len)
{
   if(m_pf==0)return 0;
   return fread(pdata,1,len,m_pf);
}
int myFile::write(void* pdata,int len)
{
   if(m_pf==0)return 0;
   return fwrite(pdata,1,len,m_pf);
}
int myFile::write(const char* str)
{
      fprintf(m_pf,str);
      return 0;
}

int myFile::size()
{
    if(m_pf==0)
    {
        printf("file error\n");
        return 0;
    }

    unsigned long pos= ftell (m_pf);
    fseek (m_pf, 0, SEEK_END);   ///将文件指针移动文件结尾
    unsigned long fsize= ftell (m_pf); ///求出当前文件指针距离文件开始的字节数
    fseek (m_pf, pos, SEEK_SET);
    return fsize;

}
int myFile::seed(int pos)
{
  fseek (m_pf, pos, SEEK_SET);
  return 0;
}
myFile::~myFile()
{
if(m_pf)delete m_pf;
close();

}
 void myFile::close()
 {
   if(m_pf)
    {

    int s = size();
    fclose(m_pf);//dtor
    m_pf = 0;
    if(s==0)
    {
        remove(m_name);
    }
    }
 }
