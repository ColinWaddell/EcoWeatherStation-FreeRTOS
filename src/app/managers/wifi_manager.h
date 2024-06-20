#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

typedef enum _wifi_status
{
    WIFI_STATUS_UNCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED
} wifi_status;

void wifi_init();

#endif