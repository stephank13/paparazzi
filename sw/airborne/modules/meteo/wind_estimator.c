/*
 * Copyright (C) C. DW
 *
 * This file is part of paparazzi
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
 * along with paparazzi; see the file COPYING.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
/**
 * @file "modules/stereocam/droplet/stereocam_droplet.c"
 * @author C. DW
 *
 */

//#include "modules/stereocam/droplet/stereocam_droplet.h"

// Know waypoint numbers and blocks
#include "generated/flight_plan.h"
#include "firmwares/rotorcraft/navigation.h"

// Downlink
#ifndef DOWNLINK_DEVICE
#define DOWNLINK_DEVICE DOWNLINK_AP_DEVICE
#endif
#include "pprzlink/messages.h"
#include "subsystems/datalink/downlink.h"

int wind_est_run = 0;

void wind_est_init(void) {
  wind_est_run = 0;
}

void wind_est_start(void) {
  wind_est_run = 1;
}
void wind_est_stop(void) {
  wind_est_run = 0;
}

void wind_est_periodic(void) {

  static float heading = 0;

  if (wind_est_run == 1) {
    heading += 0.5;
    if (heading > 360) {
      heading = 0;
    }
    nav_set_heading_rad(RadOfDeg(heading));
  }
}

