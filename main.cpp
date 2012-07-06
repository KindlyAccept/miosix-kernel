
#include <cstdio>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include "miosix.h"
#include "kernel/process.h"
#include "interfaces/suspend_support.h"
#include "app_template/prog3.h"

using namespace std;
using namespace miosix;

void ledThread(void *)
{
    for(;;)
    {
        ledOn();
        Thread::sleep(200);
        ledOff();
        Thread::sleep(200);
    }
}

int main()
{
    Thread::create(ledThread,STACK_MIN);
    SuspendManager::startHibernationDaemon();
    iprintf("tick=%ull\n",getTick());
    if(firstBoot())
    {
        puts("First boot");
        //Watermarking
        memset(getBackupSramBase(),0xff,getBackupSramSize());
        char *buf=new char[1024];
        memset(buf,0xff,1024);
        Mram& mram=Mram::instance();
        mram.exitSleepMode();
        for(int i=0,j=0;i<mram.size()/1024;i++,j+=1024)
            mram.write(j,buf,1024);
        mram.enterSleepMode();
        delete[] buf;
    } else {
        puts("RTC boot");
        SuspendManager::resume();
        
        int ec;
        Process::wait(&ec);
        iprintf("Process terminated\n");
        if(WIFEXITED(ec))
        {
            iprintf("Exit code is %d\n",WEXITSTATUS(ec));
        } else if(WIFSIGNALED(ec)) {
            if(WTERMSIG(ec)==SIGSEGV) iprintf("Process segfaulted\n");
        }
    }
    
    ElfProgram prog(reinterpret_cast<const unsigned int*>(main_elf),main_elf_len);
    for(int i=0;;i++)
    {
        getchar();
        pid_t child=Process::create(prog);
        int ec;
        pid_t pid;
        if(i%2==0) pid=Process::wait(&ec);
        else pid=Process::waitpid(child,&ec,0);
        iprintf("Process %d terminated\n",pid);
        if(WIFEXITED(ec))
        {
            iprintf("Exit code is %d\n",WEXITSTATUS(ec));
        } else if(WIFSIGNALED(ec)) {
            if(WTERMSIG(ec)==SIGSEGV) iprintf("Process segfaulted\n");
        }
    }
}
