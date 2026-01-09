/*
   lw_es519xx.h

   (from sigrok source code, adapted for embedded use)

   (c) 2023-2024 linux-works labs
*/


#ifndef _LW_ES519XX_H_
#define _LW_ES519XX_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>


// import a helper class
#include "lw_eng_to_decimal.h"   // class to convert engineering notation to pure decimal (ascii)


#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (1)
#endif



class lw_ES519XX {

  public:

    // vars
    int      exp_as_int;
    int32_t  val_as_int;
    char     val_as_str[32];
    char     meaning[32];

    // public methods
    lw_ES519XX(void);  // ctor
    
    bool parse(uint8_t *);


  private:
    // vars
    lw_EngToDecimal _engToDecimal;

    // 19200 baud (ie, not 2400 or any other)
    // this is for the '14 byte' protocol (ut61e and similar)
    int exponents_19200_14b[9][8] = {
      {  -4,  -3,  -2, -1, -5,  0,  0,  0 }, /* V */
      {  -8,  -7,   0,  0,  0,  0,  0,  0 }, /* uA */
      {  -6,  -5,   0,  0,  0,  0,  0,  0 }, /* mA */
      {  -3,   0,   0,  0,  0,  0,  0,  0 }, /* A */
      {  -4,  -3,  -2, -1,  0,  0,  0,  0 }, /* Manual A */
      {  -2,  -1,   0,  1,  2,  3,  4,  0 }, /* Resistance */
      {  -2,  -1,   0,  0,  1,  2,  3,  4 }, /* Frequency */
      { -12, -11, -10, -9, -8, -7, -6, -5 }, /* Capacitance */
      {  -4,   0,   0,  0,  0,  0,  0,  0 }, /* Diode */
    };

    int exponents_19200_11b_5digits[9][8] = {
      {  -4,  -3,  -2, -1, -5,  0,  0,  0 }, /* V */
      {  -8,  -7,   0,  0,  0,  0,  0,  0 }, /* uA */
      {  -6,  -5,   0,  0,  0,  0,  0,  0 }, /* mA */
      {   0,  -3,   0,  0,  0,  0,  0,  0 }, /* A */
      {  -4,  -3,  -2, -1,  0,  0,  0,  0 }, /* Manual A */
      {  -2,  -1,   0,  1,  2,  3,  4,  0 }, /* Resistance */
      {  -1,   0,   0,  1,  2,  3,  4,  0 }, /* Frequency */
      { -12, -11, -10, -9, -8, -7, -6, -5 }, /* Capacitance */
      {  -4,   0,   0,  0,  0,  0,  0,  0 }, /* Diode */
    };


    bool     is_judge = 0, is_voltage = 0, is_auto = 0, is_micro = 0, is_current = 0;
    bool     is_milli = 0, is_resistance = 0, is_continuity = 0, is_diode = 0;
    bool     is_frequency = 0, is_rpm = 0, is_capacitance = 0, is_duty_cycle = 0;
    bool     is_temperature = 0, is_celsius = 0, is_fahrenheit = 0;
    bool     is_adp0 = 0, is_adp1 = 0, is_adp2 = 0, is_adp3 = 0;
    bool     is_sign = 0, is_batt = 0, is_ol = 0, is_pmax = 0, is_pmin = 0, is_apo = 0;
    bool     is_dc = 0, is_ac = 0, is_vahz = 0, is_min = 0, is_max = 0, is_rel = 0, is_hold = 0;
    bool     is_digit4 = 0, is_ul = 0, is_vasel = 0, is_vbar = 0, is_lpf1 = 0, is_lpf0 = 0, is_rmr = 0;
    uint32_t baudrate = 0;
    int      packet_size = 0;
    bool     alt_functions = 0, fivedigits = 0, clampmeter = 0, selectable_lpf = 0;
    uint8_t  digits = 0;
    uint8_t  raw_data[32];


    // private methods

    void _init(void);
    bool _packet_valid(uint8_t *);
    bool _parse_value(void);
    bool _parse_range(void);
    bool _parse_flags(void);
    bool _flags_valid(void);
    bool _handle_flags(void);

};


#endif // _LW_ES519XX_H_

// end lw_es519xx.h
