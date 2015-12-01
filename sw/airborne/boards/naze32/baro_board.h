
/*
 * board specific functions for the Naze32 board
 *
 */

#ifndef BOARDS_NAZE32_BARO_H
#define BOARDS_NAZE32_BARO_H

// only for printing the baro type during compilation
#define BARO_BOARD BARO_BOARD_MS5611_I2C

extern void baro_event(void);

#define BaroEvent baro_event

#endif /* BOARDS_NAZE32_BARO_H */
