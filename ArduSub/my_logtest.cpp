#include "Sub.h"

void Sub::update_my_logtest()
{
    if(should_log(MASK_LOG_LOG_TEST))
    {
        Log_Write_mytestlog();
    }
}