/*
#include <stdio.h>
//#include <drivers/drv_my_telem2.h>
#include <AP_HAL/AP_HAL.H>

my_telem2::my_telem2()
{

}
void my_telem2::init(void)
{
    _telem2_fd = open(telem2_DEVICE_PATH, O_RDWR);
    PRINTF("----_telem2_fd :%d\n",_telem2_fd);
}

void my_telem2::update(void)
{
    read(_telem2_fd,(char*)(&_telem_cmd), sizeof(_telem_cmd));
    write(_telem2_fd,(char*)(&_telem_cmd), sizeof(_telem_cmd));
}
*/