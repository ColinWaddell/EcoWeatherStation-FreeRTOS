#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

typedef enum _time_status
{
    TIME_STATUS_NO_TIME,
    TIME_STATUS_UPDATING,
    TIME_STATUS_TIME_LOCKED
} time_status;

void clock_init();

#endif