#include <miosix.h>
using namespace miosix;
int main()
{
    for(;;)
    {
        ledOn();
        Thread::sleep(2000);
        ledOff();
        Thread::sleep(2000);
    }
}
