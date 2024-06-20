#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

typedef enum _data_status
{
    DATA_STATUS_EMPTY,
    DATA_STATUS_SEARCHING,
    DATA_STATUS_DOWNLOADED,
    DATA_STATUS_DOWNLOAD_FAILURE,
    DATA_STATUS_DECODER_FAILURE
} data_status;

typedef struct _rain_packet
{
    float rate;
    float daily;
} rain_packet;

typedef struct _wind_packet
{
    int16_t direction;
    float speed;
    float gust;
} wind_packet;

typedef struct _temperature_packet
{
    float currently;
    float feelslike;
} temperature_packet;

typedef struct _sun_packet
{
    String solar;
    int16_t uv_level;
} sun_packet;

typedef struct _atmosphere_packet
{
    float humidity;
    float dew_point;
} atmosphere_packet;

typedef struct _data_packet
{
    temperature_packet temperature;
    rain_packet rain;
    wind_packet wind;
    sun_packet sun;
    atmosphere_packet atmosphere;
} data_packet;

void data_init();

#endif