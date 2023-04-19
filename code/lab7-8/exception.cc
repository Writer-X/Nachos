// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

Thread* thread;
AddrSpace *space;

/**
 * AdvancePC();指令寄存器自增函数
 */
void AdvancePC()
{
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
    machine->WriteRegister(PCReg, machine->ReadRegister(PCReg)+4);
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
}

/**
 * StartProcess(int n);线程要执行的代码（函数）及函数所需的参数
 */
void 
StartProcess(int n)
{
    currentThread->space = space;
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    machine->Run();
    ASSERT(FALSE);
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    if(which == SyscallException)
    {
        switch(type)
        {
            case SC_Halt:{
                DEBUG('a', "Shutdown, initiated by user program.\n");
   	            interrupt->Halt();
                break;
            }
            case SC_Exec:{
                printf("Execute system call of Exec()");
                printf("CurrentThreadId:%d\n",currentThread->space->getSpaceID());
                char filename[50];
                int addr = machine->ReadRegister(4); //获取 Exec() 的参数 filename 在内存中的地址
                int i = 0;
                do{
                    //从内存中读取文件名
                    machine->ReadMem(addr+i,1,(int*)&filename[i]);
                }while(filename[i++]!='\0');
                printf("Exec(%s):\n",filename);
                //根据 name 的指向，打开“.noff”格式的可执行文件；
                OpenFile *executable = fileSystem->Open(filename);
                if(executable == NULL)
                {
                    printf("Unable to open file %s\n",filename);
                    return;
                }
                //创建其对应用户程序的地址空间“AddrSpace”实例；
                space = new AddrSpace(executable);
                delete executable;
                //执行系统调用的线程“Fork”得到新的线程，将该线程与该“AddrSpace”实例进行映射（绑定）；
                thread = new Thread("forked thread");
                //启动该新线程，即对"AddrSpace"对象初始化寄存器组，调用“machine->Run()”开始执行；
                thread->Fork(StartProcess,1);
                //执行系统调用的线程将“SpaceId”返回；
                machine->WriteRegister(2,space->getSpaceID());
                AdvancePC();//指令寄存器自增
                break;
            }
            case SC_Join:{
                printf("Execute system call of Join() ");
                printf("CurrentThreadId:%d\n",currentThread->space->getSpaceID());            
                int spaceId = machine->ReadRegister(4);
                currentThread->Join(spaceId);  

                machine->WriteRegister(2,currentThread->waitProcessExitCode);
                AdvancePC();
                break;
            }
            case SC_Exit:{
                printf("Execute system call of Exit() ");
                printf("CurrentThreadId:%d\n",currentThread->space->getSpaceID());
                int ExitStatus = machine->ReadRegister(4);
                machine->WriteRegister(2,ExitStatus);
                currentThread->setExitStatus(ExitStatus);
                if(ExitStatus == 99)
                {
                    printf("getTerminatedList has been empty\n");
                    scheduler->emptyList(scheduler->getTerminatedList());
                }
                delete currentThread->space;
                currentThread->Finish();   //终结当前线程
                AdvancePC();
                break;
            }
            case SC_Yield:{
                printf("Execute system call of Yield() ");
                printf("CurrentThreadId:%d\n",currentThread->space->getSpaceID());
                currentThread->Yield();
                AdvancePC();
                break;
            }
            case SC_Fork:{

            }
            case SC_Create:{
                int addr = machine->ReadRegister(4);
                char filename[128];
                for(int i = 0; i < 128; i++){
                    machine->ReadMem(addr+i,1,(int *)&filename[i]);
                    if(filename[i] == '\0') break;
                }
                int fileDescriptor = OpenForWrite(filename);
                if(fileDescriptor == -1) printf("create file %s failed!\n",filename);
                else printf("create file %s succeed, the file id is %d\n",filename,fileDescriptor);
                Close(fileDescriptor);
                AdvancePC();
                break;
            }
            case SC_Open:{
                int addr = machine->ReadRegister(4);
                char filename[128];
                for(int i = 0; i < 128; i++){
                    machine->ReadMem(addr+i,1,(int *)&filename[i]);
                    if(filename[i] == '\0') break;
                    }
                int fileDescriptor = OpenForWrite(filename);
                if(fileDescriptor == -1) printf("Open file %s failed!\n",filename);
                else printf("Open file %s succeed, the file id is %d\n",filename,fileDescriptor);                
                machine->WriteRegister(2,fileDescriptor);
                AdvancePC();
                break;
            }
            case SC_Write:{
                // 读取寄存器信息
                int addr = machine->ReadRegister(4);
                int size = machine->ReadRegister(5);      
                int fileId = machine->ReadRegister(6);      
    
                // 打开文件
                OpenFile *openfile = new OpenFile(fileId);
                ASSERT(openfile != NULL);

                // 读取具体数据
                char buffer[128];
                for(int i = 0; i < size; i++){
                    machine->ReadMem(addr+i,1,(int *)&buffer[i]);
                    if(buffer[i] == '\0') break;
                }
                buffer[size] = '\0';

                // 写入数据
                int writePos;
                if(fileId == 1) writePos = 0;
                else writePos = openfile->Length();
                // 在 writePos 后面进行数据添加
                int writtenBytes = openfile->WriteAt(buffer,size,writePos);
                if(writtenBytes == 0) printf("write file failed!\n");
                else printf("\"%s\" has wrote in file %d succeed!\n",buffer,fileId);
                AdvancePC();
                break;
            }
            case SC_Read:{
                // 读取寄存器信息
                int addr = machine->ReadRegister(4);
                int size = machine->ReadRegister(5);       // 字节数
                int fileId = machine->ReadRegister(6);      // fd

                // 打开文件读取信息
                char buffer[size+1];
                OpenFile *openfile = new OpenFile(fileId);
                int readnum = openfile->Read(buffer,size);

                for(int i = 0; i < size; i++)
                    if(!machine->WriteMem(addr,1,buffer[i])) printf("This is something Wrong.\n");
                buffer[size] = '\0';
                printf("read succeed, the content is \"%s\", the length is %d\n",buffer,size);
                machine->WriteRegister(2,readnum);
                AdvancePC();
                break;
            }
            case SC_Close:{
                int fileId = machine->ReadRegister(4);
                Close(fileId);
                printf("File %d closed succeed!\n",fileId);
                AdvancePC();
                break;
            }
            default:{
                printf("Unexpected syscall %d %d\n", which, type);
	            ASSERT(FALSE);
            }
        }
    }
    else
    {
        printf("Unexpected user mode exception %d %d\n", which, type);
	    ASSERT(FALSE);       
    }
}
