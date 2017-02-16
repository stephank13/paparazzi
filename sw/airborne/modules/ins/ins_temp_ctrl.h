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
 * @file modules/ins/ins_temp_ctrl.h
 *
 * Bebop2 INS temperature control
 *
 * Controls the 6 heating resistors in the Bebop2 to keep the MPU6050
 * gyro/accel INS sensors at a constant temperature (125000ns period)
 *
 */

#ifndef INS_TEMP_CTRL_H
#define INS_TEMP_CTRL_H

#include "std.h"

void ins_temp_ctrl_init(void);
void ins_temp_ctrl_periodic(void);

#define INS_TEMP_CTRL_DUTY_MAX 125000

#define INS_TEMP_CTRL_KP 20000
#define INS_TEMP_CTRL_KI 6


#endif /* INS_TEMP_CTRL_H */
