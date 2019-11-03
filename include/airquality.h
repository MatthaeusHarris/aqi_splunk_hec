#ifndef __AIRQUALITY__
#define __AIRQUALITY__

#include <Arduino.h>
#include <pms.h>

// epa bins are doubled to avoid floating point math
const uint16_t epa_pm2dot5_low[] = {0, 24, 70, 110, 300, 500, 700}; 
const uint16_t epa_pm2dot5_high[] = {24, 71, 111, 301, 501, 701, 0};
const uint16_t epa_pm10_low[] = {0, 108, 308, 508, 708, 848, 1008, 1208};
const uint16_t epa_pm10_high[] = {108, 308, 508, 708, 848, 1008, 1208, 0};
const uint16_t epa_aqi_low[] = {0, 51, 101, 151, 201, 301, 401}; // Not doubled, no floating point math necessary
const uint16_t epa_aqi_high[] = {50,100,150,200,300,400,500,0};

// Given a sorted array haystack, find the index of the first element greater than needle
uint16_t find_bin(const uint16_t *haystack, uint16_t needle);

// Given the PM2.5 and PM10 micrograms per cubic meter, return the calculated AQI per this algorithm:
// https://en.wikipedia.org/wiki/Air_quality_index#Computing_the_AQI
uint16_t calc_AQI(uint16_t pm2dot5, uint16_t pm10);


// Create PMS object


#endif