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
 * @file modules/ins/ins_temp_ctrl.c
 *
 * Bebop2 INS temperature control
 *
 * Controls the 6 heating resistors in the Bebop2 to keep the MPU6050
 * gyro/accel INS sensors at a constant temperature (125000ns period)
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "std.h"
#include "mcu_periph/uart.h"
#include "pprzlink/messages.h"
#include "subsystems/datalink/downlink.h"
#include "ins_temp_ctrl.h"

uint8_t ins_temp_ctrl_ok = 0;
int pwm_heat_duty_fd = 0;

#ifndef INS_TEMP_TARGET
#define INS_TEMP_TARGET 50
#endif

void ins_temp_ctrl_periodic(void)
{
  float temp_current, error;
  static float sum_error = 0;
  uint32_t output = 0;

  temp_current = imu_bebop.mpu.temp;

  if (ins_temp_ctrl_ok == 1) {
    /* minimal PI algo without dt from Ardupilot */
    error = (float) ((INS_TEMP_TARGET) - temp_current);

    /* Don't accumulate errors if the integrated error is superior
     * to the max duty cycle(pwm_period)
     */
    if ((fabsf(sum_error) * INS_TEMP_CTRL_KI < INS_TEMP_CTRL_DUTY_MAX)) {
        sum_error = sum_error + error;
    }

    output = INS_TEMP_CTRL_KP * error + INS_TEMP_CTRL_KI * sum_error;

    if (output > INS_TEMP_CTRL_DUTY_MAX) {
      output = INS_TEMP_CTRL_DUTY_MAX;
    } else if (output < 0) {
      output = 0;
    }

     if (dprintf(pwm_heat_duty_fd, "%u", output) < 0)
       /* could not set duty cycle */
     {}
  }

#ifdef SENSOR_SYNC_SEND
  uint16_t duty_cycle;
  duty_cycle = (uint16_t) ((uint32_t) output / (INS_TEMP_CTRL_DUTY_MAX/100));

  RunOnceEvery(INS_TEMP_CTRL_PERIODIC_FREQ, DOWNLINK_SEND_TMP_STATUS(DefaultChannel, DefaultDevice, &duty_cycle, &temp_current));
#endif
}

void ins_temp_ctrl_init(void)
{
  int pwm_heat_run_fd, ret;

  pwm_heat_run_fd = open("/sys/class/pwm/pwm_6/run", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (pwm_heat_run_fd < 0) {
    /* could not open run */
    return;
  }

  pwm_heat_duty_fd = open("/sys/class/pwm/pwm_6/duty_ns", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (pwm_heat_duty_fd < 0) {
    /* could not open duty */
    close(pwm_heat_run_fd);
    return;
  }

  ret = write(pwm_heat_duty_fd, "0", 1);
  if (ret != 1) {
    /* could not set duty cycle */
    goto error;
  }

  ret = write(pwm_heat_run_fd, "0", 1);
  if (ret != 1) {
    /* could not disable */
    goto error;
  }

  ret = write(pwm_heat_run_fd, "1", 1);
  if (ret != 1) {
    /* could not enable */
    goto error;
  }
  
  ins_temp_ctrl_ok = 1;
  return;
  
error:
    close(pwm_heat_run_fd);
    close(pwm_heat_duty_fd);
}
