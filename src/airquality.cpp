#include "airquality.h"

uint16_t find_bin(const uint16_t *haystack, uint16_t needle) {
  uint16_t i = 0;
  while (haystack[i] < needle && haystack[i] != 0) {
    i++;
  }
  return i;
}

uint16_t calc_AQI(uint16_t pm2dot5, uint16_t pm10) {
  uint16_t pm2dot5_index, pm10_index, i_high_2dot5, i_low_2dot5, i_high_10, i_low_10;
  uint16_t c_high_2dot5, c_low_2dot5, c_high_10, c_low_10, aqi_10, aqi_2dot5;
  pm2dot5_index = find_bin(epa_pm2dot5_high, pm2dot5 * 2);
  pm10_index = find_bin(epa_pm10_high, pm10 * 2);
  
  c_low_2dot5 = epa_pm2dot5_low[pm2dot5_index] / 2;
  c_high_2dot5 = epa_pm2dot5_high[pm2dot5_index] / 2;
  i_high_2dot5 = epa_aqi_high[pm2dot5_index];
  i_low_2dot5 = epa_aqi_low[pm2dot5_index];
  aqi_2dot5 = ((i_high_2dot5 - i_low_2dot5)*(pm2dot5 - c_low_2dot5))/(c_high_2dot5 - c_low_2dot5)+i_low_2dot5;

  c_low_10 = epa_pm10_low[pm10_index] / 2;
  c_high_10 = epa_pm10_high[pm10_index] / 2;
  i_high_10 = epa_aqi_high[pm10_index];
  i_low_10 = epa_aqi_low[pm10_index];
  aqi_10 = ((i_high_10 - i_low_10)*(pm10 - c_low_10))/(c_high_10 - c_low_10)+i_low_10;
  
  if (aqi_10 > aqi_2dot5) {
    return aqi_10;
  } else {
    return aqi_2dot5;
  }
}