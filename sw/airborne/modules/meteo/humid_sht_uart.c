/*
 * Copyright (C) 2008-2017 The Paparazzi team
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * @file modules/meteo/humid_sht_uart.c
 *
 * SHTxx sensor interface
 *
 * This reads the values for humidity and temperature from the SHTxx sensor through an uart.
 *
 */

#include "std.h"
#include "mcu_periph/gpio.h"
#include "mcu_periph/uart.h"
#include "pprzlink/messages.h"
#include "subsystems/datalink/downlink.h"
#include "humid_sht_uart.h"

// sd-log
#if SHT_SDLOG
#include "modules/loggers/sdlog_chibios.h"
#include "subsystems/gps.h"
bool log_sht_started;
#endif

uint16_t humidsht, tempsht;
float fhumidsht, ftempsht;
bool humid_sht_available;

void calc_sht(uint16_t hum, uint16_t tem, float *fhum , float *ftem);
void humid_sht_uart_parse(uint8_t c);

void calc_sht(uint16_t hum, uint16_t tem, float *fhum , float *ftem)
{
  // calculates temperature [ C] and humidity [%RH]
  // input : humi [Ticks] (12 bit)
  //             temp [Ticks] (14 bit)
  // output: humi [%RH]
  //             temp [ C]

  const float C1 = -4.0;            // for 12 Bit
  const float C2 = 0.0405;          // for 12 Bit
  const float C3 = -0.0000028;      // for 12 Bit
  const float T1 = 0.01;            // for 14 Bit @ 5V
  const float T2 = 0.00008;         // for 14 Bit @ 5V
  float rh;                         // rh:      Humidity [Ticks] 12 Bit
  float t;                          // t:       Temperature [Ticks] 14 Bit
  float rh_lin;                     // rh_lin:  Humidity linear
  float rh_true;                    // rh_true: Temperature compensated humidity
  float t_C;                        // t_C   :  Temperature [ C]

  rh = (float)hum;                  //converts integer to float
  t = (float)tem;                   //converts integer to float

  t_C = t * 0.01 - 39.66;           //calc. Temperature from ticks to [°C] @ 3.5V
  rh_lin = C3 * rh * rh + C2 * rh + C1; //calc. Humidity from ticks to [%RH]
  rh_true = (t_C - 25) * (T1 + T2 * rh) + rh_lin; //calc. Temperature compensated humidity [%RH]
  if (rh_true > 100) { rh_true = 100; } //cut if the value is outside of
  if (rh_true < 0.1) { rh_true = 0.1; } //the physical possible range
  *ftem = t_C;                      //return temperature [ C]
  *fhum = rh_true;                  //return humidity[%RH]
}

void humid_sht_uart_periodic(void)
{
#if 0
  uint8_t error = 0, checksum;

  if (humid_sht_status == SHT_IDLE) {
    /* init humidity read */
    s_connectionreset();
    s_start_measure(HUMI);
    humid_sht_status = SHT_MEASURING_HUMID;
  } else if (humid_sht_status == SHT_MEASURING_HUMID) {
    /* get data */
    error += s_read_measure(&humidsht, &checksum);

    if (error != 0) {
      s_connectionreset();
      s_start_measure(HUMI);    //restart
      //LED_TOGGLE(2);
    } else {
      error += s_start_measure(TEMP);
      humid_sht_status = SHT_MEASURING_TEMP;
    }
  } else if (humid_sht_status == SHT_MEASURING_TEMP) {
    /* get data */
    error += s_read_measure(&tempsht, &checksum);

    if (error != 0) {
      s_connectionreset();
      s_start_measure(TEMP);    //restart
      //LED_TOGGLE(2);
    } else {
      calc_sht(humidsht, tempsht, &fhumidsht, &ftempsht);
      humid_sht_available = true;
      s_connectionreset();
      s_start_measure(HUMI);
      humid_sht_status = SHT_MEASURING_HUMID;
      DOWNLINK_SEND_SHT_STATUS(DefaultChannel, DefaultDevice, &humidsht, &tempsht, &fhumidsht, &ftempsht);
      humid_sht_available = false;

#if SHT_SDLOG
  if (pprzLogFile != -1) {
    if (!log_sht_started) {
      sdLogWriteLog(pprzLogFile, "SHT75: Humid(pct) Temp(degC) GPS_fix TOW(ms) Week Lat(1e7deg) Lon(1e7deg) HMSL(mm) gspeed(cm/s) course(1e7deg) climb(cm/s)\n");
      log_sht_started = true;
    }
    sdLogWriteLog(pprzLogFile, "sht75: %9.4f %9.4f    %d %d %d   %d %d %d   %d %d %d\n",
		  fhumidsht, ftempsht,
		  gps.fix, gps.tow, gps.week,
		  gps.lla_pos.lat, gps.lla_pos.lon, gps.hmsl,
		  gps.gspeed, gps.course, -gps.ned_vel.z);
  }
#endif

    }
  }
#endif
}

/* airspeed_otf_parse */
void humid_sht_uart_parse(uint8_t c)
{
  static uint8_t msg_cnt = 0;
  static uint8_t data[6];
  uint16_t i, chk = 0;

  if (msg_cnt > 0) {
    data[msg_cnt++] = c;
    if (msg_cnt == 6) {
      tempsht = data[1] | (data[2] << 8);
      humidsht = data[3] | (data[4] << 8);
      for (i = 1; i < 5; i++)
        chk += data[i];
      if (data[5] == (chk & 0xFF)) {
        calc_sht(humidsht, tempsht, &fhumidsht, &ftempsht);
        DOWNLINK_SEND_SHT_STATUS(DefaultChannel, DefaultDevice, &humidsht, &tempsht, &fhumidsht, &ftempsht);
	  }
      msg_cnt = 0;
    }
  }
  else if (c == 0xFF)
    msg_cnt = 1;


#if 0
  static uint8_t otf_status = OTF_UNINIT, otf_idx = 0, otf_crs_idx;
  static char otf_inp[64];
  static uint32_t counter;
  static int16_t course[3];
  static int32_t altitude;
  static uint8_t checksum;

  switch (otf_status) {

    case OTF_WAIT_START:
      if (c == OTF_START) {
        otf_status++;
        otf_idx = 0;
      } else {
        otf_status = OTF_UNINIT;
      }
      break;

    case OTF_WAIT_COUNTER:
      if (isdigit((int)c)) {
        if (otf_idx == 0) {
//FIXME        otf_timestamp = getclock();
        }
        otf_inp[otf_idx++] = c;
      } else {
        if ((otf_idx == 5) && (c == OTF_LIMITER)) {
          otf_inp[otf_idx] = 0;
          counter = atoi(otf_inp);
          otf_idx = 0;
          otf_crs_idx = 0;
          otf_status++;
        } else {
          otf_status = OTF_UNINIT;
        }
      }
      break;

    case OTF_WAIT_ANGLES:
      if (isdigit((int)c) || (c == '-') || (c == '.')) {
        otf_inp[otf_idx++] = c;
      } else {
        if ((otf_idx > 1) && (otf_idx < 9) && (c == OTF_LIMITER)) {
          otf_inp[otf_idx] = 0;
          course[otf_crs_idx] = (int16_t)(100. * atof(otf_inp));
          otf_idx = 0;
          if (otf_crs_idx++ == 2) {
            otf_status++;
          }
        } else {
          otf_status = OTF_UNINIT;
        }
      }
      break;

    case OTF_WAIT_ALTITUDE:
      if (isdigit((int)c) || (c == '-') || (c == '.')) {
        otf_inp[otf_idx++] = c;
      } else {
        if ((otf_idx > 1) && (otf_idx < 9) && (c == OTF_LIMITER)) {
          otf_inp[otf_idx] = 0;
          altitude = (int32_t)(100. * atof(otf_inp));
          otf_idx = 0;
          otf_status++;
        } else {
          otf_status = OTF_UNINIT;
        }
      }
      break;

    case OTF_WAIT_CHECKSUM:
      if (isxdigit((int)c)) {
        otf_inp[otf_idx++] = c;
      } else {
        if ((otf_idx == 2) && (c == OTF_END)) {
          otf_inp[otf_idx] = 0;
          checksum = strtol(otf_inp, NULL, 16);
          otf_idx = 0;
          int32_t foo = 0;
          DOWNLINK_SEND_AEROPROBE(DefaultChannel, DefaultDevice, &counter, &course[0], &course[1], &course[2],
              &altitude, &foo, &foo, &checksum);
        }
        otf_status = OTF_UNINIT;
      }
      break;

    default:
      otf_status = OTF_UNINIT;
      break;
  }
#endif
}

void humid_sht_uart_init(void)
{
}

void humid_sht_uart_event(void)
{
  while (MetBuffer()) {
    uint8_t ch = MetGetch();
    humid_sht_uart_parse(ch);
  }
}
