#include "Sub.h"
//#include "GCS.h"
#if HAL_OS_POSIX_IO
#include <stdio.h>
#endif

extern const AP_HAL::HAL& hal;
//hal.uartA->begin(57600);
void Sub::update_my_uarttest()
{
    //hal.scheduler->delay(50);
    uint16_t numb = hal.uartA->available();
    for(uint16_t i=0;i<numb;i++)
    {
        const uint8_t c = (uint8_t)hal.uartA->read();
        hal.uartA->printf(" %o",c);
        if(i == (numb - 1)) {hal.uartA->printf("receive succeed");}
    }
    /*
    while (hal.uartA->available())
    {
         uint8_t data = (uint8_t)hal.uartA->read();
         hal.uartA->write(data);
    }
    */
}


/*
    hal.scheduler->delay(100); //Ensure that the uartA can be initialized
    if (hal.uartA == nullptr) {
        // that UART doesn't exist on this platform
        return;
    }
    hal.uartA->begin(57600);
    hal.uartA->printf(p,(my_heartbeat));
    UART_putString(uart_id, buf, length);
    comm_send_ch();
    gcs_chan[i].send_message(id);
    GCS_MAVLINK::send_message(enum ap_message id);
    receive new packets
    if(hal.uartA->available())
    {
        hal.uartA->printf("i recept\n",
                     "uartA",(double)(AP_HAL::millis() * 0.001f));
    }
    */