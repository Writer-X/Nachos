#include"syscall.h"
int main()
{
    OpenFileId Fp;
    char buffer[50];
    int sz;
    Create("test");
    Fp = Open("test");
    Write("Hello world!",12,Fp);
    sz = Read(buffer,6,Fp);
    Close(Fp);
    Exit(0);
}