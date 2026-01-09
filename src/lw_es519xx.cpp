/*
   lw_es519xx.cpp

   (from sigrok source code, adapted for embedded use)

   (c) 2023-2024 linux-works labs
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <lw_eng_to_decimal.h>   // class to convert engineering notation to pure decimal (ascii)

#include "lw_es519xx.h"

//#define DEBUG



//
// public ctor
//

lw_ES519XX::lw_ES519XX (void)
{
}



// private
bool lw_ES519XX::_packet_valid (uint8_t *buf)
{
  int s = packet_size;

  if ( (buf[s - 2] != '\r') || (buf[s - 1] != '\n') ) {
#ifdef DEBUG
    Serial.print("no newline found\n");
#endif
    return 0;
  }

  // copy user buffer to our info
  strncpy((char *)raw_data, (char *)buf, s);

  // extract flags into info struct
  if (!_parse_flags()) {
#ifdef DEBUG
    Serial.print("xx parse flags error\n");
#endif
    return 0;
  }

  if (!_flags_valid()) {
#ifdef DEBUG
    Serial.print("xx flags not valid\n");
#endif
    return 0;
  }

  return 1;
}



// private
bool lw_ES519XX::_parse_value (void)
{
  int32_t i, num_digits;
  uint8_t *buf = raw_data;

  num_digits = 4 + ((packet_size == 14) ? 1 : 0);


  // Bytes 1-4 (or 5): Value (4 or 5 decimal digits)
  if (is_ol) {
#ifdef DEBUG1
    Serial.print("Over limit.");
#endif
    val_as_int = INT32_MAX; //INFINITY;
    return 1;

  } else if (is_ul) {
#ifdef DEBUG1
    Serial.print("Under limit.");
#endif
    val_as_int = INT32_MIN; //INFINITY;
    return 1;

  } else if (!isdigit(buf[1]) ||
             !isdigit(buf[2]) ||
             !isdigit(buf[3]) ||
             !isdigit(buf[4]) ||
             (num_digits == 5 && !isdigit(buf[5]))) {

#ifdef DEBUG1
    printf("Value contained invalid digits: %02x %02x %02x %02x (%c %c %c %c).",
           buf[1], buf[2], buf[3], buf[4],  buf[1], buf[2], buf[3], buf[4]);
#endif

    return 0;  // error
  }

  // get the numerical int and save in info struct
  val_as_int = (is_digit4) ? 1 : 0;
  for (i = 0; i < num_digits; i++) {
    val_as_int = 10 * val_as_int + (buf[i + 1] - '0');
  }

  // apply sign
  val_as_int *= is_sign ? -1 : 1;

  return 1;
}


// private
bool lw_ES519XX::_parse_range (void)
{
  int mode;
  int exponent = 0;

  uint8_t b = raw_data[0];       // first byte in raw stream
  int idx = b - '0';             // from ascii digit to binary digit

  if (idx < 0 || idx > 7) {
#ifdef DEBUG
    Serial.print("Invalid range byte / index: ");
    Serial.print(b, HEX);
    Serial.println(idx);
#endif
    return 0;  // error
  }

  // Parse range byte (depends on the measurement mode).
  if (is_voltage) {
    mode = 0; /* V */

  } else if (is_current && is_micro) {
    mode = 1; /* uA */

  } else if (is_current && is_milli) {
    mode = 2; /* mA */

  } else if (is_current && is_auto) {
    mode = 3; /* A */

  } else if (is_current && !is_auto) {
    mode = 4; /* Manual A */

#if 0
  } else if (is_rpm) {
    // Not a typo, it's really index 4 in exponents_2400_11b[][].
    mode = 4; /* RPM */
#endif

  } else if (is_resistance || is_continuity) {
    mode = 5; /* Resistance */

  } else if (is_frequency) {
    mode = 6; /* Frequency */

  } else if (is_capacitance) {
    mode = 7; /* Capacitance */

  } else if (is_diode) {
    mode = 8; /* Diode */

  } else if (is_duty_cycle) {
    mode = 0; /* Dummy, unused */

  } else {
#ifdef DEBUG
    Serial.print("Invalid mode, range byte ");
    Serial.println(b, HEX);
#endif
    return 0;  // error
  }


  // fix exponent, if needed
  if (is_duty_cycle) {
    exponent = -1;
  } else if (fivedigits) {
    exponent = exponents_19200_11b_5digits[mode][idx];
  } else if (packet_size == 14) {
    exponent = exponents_19200_14b[mode][idx];
  }

  // save exponent in info struct
  exp_as_int = exponent;

  // new api; takes in 2 integers (mantissa and exponent) and returns string ptr to ascii version
  char *new_s = _engToDecimal.convert(val_as_int, exp_as_int);

  // save string in info struct

  // exception: zero-lead any strings of the form ".123" -> "0.123"
  if (new_s[0] == '.') {
    strcpy(val_as_str, "0");
    strcat(val_as_str, new_s);
  } else {
    strcpy(val_as_str, new_s);
  }


#ifdef DEBUG
  Serial.print(val_as_str); Serial.print(" ");
  Serial.print(exp_as_int); Serial.print(" ");
  Serial.println(val_as_int);
#endif

  return 1;
}


// private
bool lw_ES519XX::_parse_flags (void)
{
  int function, status;
  uint8_t *buf = raw_data;

  function = 5 + ((packet_size == 14) ? 1 : 0);
  status = function + 1;

  // Status byte
  if (alt_functions) {
    is_sign  = (buf[status] & (1 << 3)) != 0;
    is_batt  = (buf[status] & (1 << 2)) != 0; /* Bat. low */
    is_ol    = (buf[status] & (1 << 1)) != 0; /* Overflow */
    is_ol   |= (buf[status] & (1 << 0)) != 0; /* Overflow */
  } else {
    is_judge = (buf[status] & (1 << 3)) != 0;
    is_sign  = (buf[status] & (1 << 2)) != 0;
    is_batt  = (buf[status] & (1 << 1)) != 0; /* Bat. low */
    is_ol    = (buf[status] & (1 << 0)) != 0; /* Overflow */
  }

  if (packet_size == 14) {
    // Option 1 byte
    is_max  = (buf[8] & (1 << 3)) != 0;
    is_min  = (buf[8] & (1 << 2)) != 0;
    is_rel  = (buf[8] & (1 << 1)) != 0;
    is_rmr  = (buf[8] & (1 << 0)) != 0;

    // Option 2 byte
    is_ul   = (buf[9] & (1 << 3)) != 0; /* Underflow */
    is_pmax = (buf[9] & (1 << 2)) != 0; /* Max. peak value */
    is_pmin = (buf[9] & (1 << 1)) != 0; /* Min. peak value */

    // Option 3 byte
    is_dc   = (buf[10] & (1 << 3)) != 0;
    is_ac   = (buf[10] & (1 << 2)) != 0;
    is_auto = (buf[10] & (1 << 1)) != 0;
    is_vahz = (buf[10] & (1 << 0)) != 0;



  } else if (alt_functions) {
    // Option 2 byte
    is_dc   = (buf[8] & (1 << 3)) != 0;
    is_auto = (buf[8] & (1 << 2)) != 0;
    is_apo  = (buf[8] & (1 << 0)) != 0;
    is_ac   = !is_dc;

  } else {
    // Option 1 byte
    if (baudrate == 2400) {
      is_pmax   = (buf[7] & (1 << 3)) != 0;
      is_pmin   = (buf[7] & (1 << 2)) != 0;
      is_vahz   = (buf[7] & (1 << 0)) != 0;

    } else if (fivedigits) {
      is_ul     = (buf[7] & (1 << 3)) != 0;
      is_pmax   = (buf[7] & (1 << 2)) != 0;
      is_pmin   = (buf[7] & (1 << 1)) != 0;
      is_digit4 = (buf[7] & (1 << 0)) != 0;

    } else if (clampmeter) {
      is_ul     = (buf[7] & (1 << 3)) != 0;
      is_vasel  = (buf[7] & (1 << 2)) != 0;
      is_vbar   = (buf[7] & (1 << 1)) != 0;

    } else {
      is_hold   = (buf[7] & (1 << 3)) != 0;
      is_max    = (buf[7] & (1 << 2)) != 0;
      is_min    = (buf[7] & (1 << 1)) != 0;
    }


    // Option 2 byte
    is_dc   = (buf[8] & (1 << 3)) != 0;
    is_ac   = (buf[8] & (1 << 2)) != 0;
    is_auto = (buf[8] & (1 << 1)) != 0;

    if (baudrate == 2400) {
      is_apo  = (buf[8] & (1 << 0)) != 0;
    } else {
      is_vahz = (buf[8] & (1 << 0)) != 0;
    }
  }


  // Function byte
  if (alt_functions) {
    switch (buf[function]) {
      case 0x3f: /* A */
        is_current = is_auto = 1;
        break;

      case 0x3e: /* uA */
        is_current = is_micro = is_auto = 1;
        break;

      case 0x3d: /* mA */
        is_current = is_milli = is_auto = 1;
        break;

      case 0x3c: /* V */
        is_voltage = 1;
        break;

      case 0x37: /* Resistance */
        is_resistance = 1;
        break;

      case 0x36: /* Continuity */
        is_continuity = 1;
        break;

      case 0x3b: /* Diode */
        is_diode = 1;
        break;

      case 0x3a: /* Frequency */
        is_frequency = 1;
        break;

      case 0x34: /* ADP0 */
      case 0x35: /* ADP0 */
        is_adp0 = 1;
        break;

      case 0x38: /* ADP1 */
      case 0x39: /* ADP1 */
        is_adp1 = 1;
        break;

      case 0x32: /* ADP2 */
      case 0x33: /* ADP2 */
        is_adp2 = 1;
        break;

      case 0x30: /* ADP3 */
      case 0x31: /* ADP3 */
        is_adp3 = 1;
        break;

      default:
#ifdef DEBUG
        Serial.print("Invalid function byte ");
        Serial.println(buf[function], HEX);
#endif
        return 0;
    }


  } else {

    // Note: Some of these mappings are fixed up later
    switch (buf[function]) {
      case 0x3b: /* V */
        is_voltage = 1;
        break;

      case 0x3d: /* uA */
        is_current = is_micro = is_auto = 1;
        break;

      case 0x3f: /* mA */
        is_current = is_milli = is_auto = 1;
        break;

      case 0x30: /* A */
        is_current = is_auto = 1;
        break;

      case 0x39: /* Manual A */
        is_current = 1;
        is_auto = 0; /* Manual mode */
        break;

      case 0x33: /* Resistance */
        is_resistance = 1;
        break;

      case 0x35: /* Continuity */
        is_continuity = 1;
        break;

      case 0x31: /* Diode */
        is_diode = 1;
        break;

      case 0x32: /* Frequency / RPM / duty cycle */
        if (packet_size == 14) {
          if (is_judge) {
            is_duty_cycle = 1;
          } else {
            is_frequency = 1;
          }

        } else {
          if (is_judge) {
            is_rpm = 1;
          } else {
            is_frequency = 1;
          }
        }
        break;

      case 0x36: /* Capacitance */
        is_capacitance = 1;
        break;

      case 0x34: /* Temperature */       // IMPORTANT: The digits always represent Celsius!
        is_temperature = 1;
        if (is_judge) {
          is_celsius = 1;
        } else {
          is_fahrenheit = 1;
        }
        break;

      case 0x3e: /* ADP0 */
        is_adp0 = 1;
        break;

      case 0x3c: /* ADP1 */
        is_adp1 = 1;
        break;

      case 0x38: /* ADP2 */
        is_adp2 = 1;
        break;

      case 0x3a: /* ADP3 */
        is_adp3 = 1;
        break;

      default:
#ifdef DEBUG
        Serial.print("Invalid function byte ");
        Serial.println(buf[function], HEX);
#endif
        return 0;
    }
  }


  if (is_vahz && (is_voltage || is_current)) {
    is_voltage = 0;
    is_current = 0;
    is_milli = is_micro = 0;

    if (packet_size == 14) {
      if (is_judge) {
        is_duty_cycle = 1;
      } else {
        is_frequency = 1;
      }

    } else {
      if (is_judge) {
        is_rpm = 1;
      } else {
        is_frequency = 1;
      }

    }
  }

  if (is_current && (is_micro || is_milli) && is_vasel) {
    is_current = is_auto = 0;
    is_voltage = 1;
  }

  return 1;
}


// private
bool lw_ES519XX::_handle_flags (void)
{
  // Measurement modes
  if (is_voltage) {
    strcpy(meaning, "Volts ");
  }

  if (is_current) {
    strcpy(meaning, "Amps ");
  }

  if (is_resistance) {
    strcpy(meaning, "Ohms ");
  }

  if (is_frequency) {
    strcpy(meaning, "Hz ");
  }

  if (is_capacitance) {
    strcpy(meaning, "Farads ");
  }

  if (is_temperature && is_celsius) {
    strcpy(meaning, "deg_C ");
  }

  if (is_temperature && is_fahrenheit) {
    strcpy(meaning, "deg_F ");
  }

  if (is_continuity) {
    strcpy(meaning, "Continuity ");
  }

  if (is_diode) {
    strcpy(meaning, "Diode_V ");
  }

  if (is_rpm) {
    strcpy(meaning, "Freq_rpm ");
  }

  if (is_duty_cycle) {
    strcpy(meaning, "pct_duty ");
  }


  // Measurement related flags
  if (is_ac) {
    strcat(meaning, "(AC)");
  }

  if (is_dc) {
    strcat(meaning, "(DC)");
  }

  if (is_auto) {
    strcat(meaning, "(Auto)");
  }

  if (is_diode) {
    strcat(meaning, "(Diode)");
  }

  if (is_max) {
    strcat(meaning, " MAX");
  }

  if (is_min) {
    strcat(meaning, " MIN");
  }

  if (is_rel) {
    strcat(meaning, " REL");
  }


  // Other flags
  if (is_judge) {
    //Serial.print(" Judge bit is set\n");
  }

  if (is_batt) {
    //Serial.print(" Battery is low\n");
  }

  if (is_ol) {
    //Serial.print(" Input overflow\n");
  }

  if (is_ul) {
    //Serial.print(" Input underflow\n");
  }

  if (is_pmax) {
    //Serial.print(" pMAX active, LCD shows max. peak value\n");
  }

  if (is_pmin) {
    //Serial.print(" pMIN active, LCD shows min. peak value\n");
  }

  if (is_vahz) {
    //Serial.print(" VAHZ active\n");
  }

  if (is_apo) {
    //Serial.print(" Auto-Power-Off enabled\n");
  }

  return 1;
}


// private
bool lw_ES519XX::_flags_valid (void)
{
  int count;

  // Does the packet have more than one multiplier?
  count  = (is_micro) ? 1 : 0;
  count += (is_milli) ? 1 : 0;

  if (count > 1) {
#ifdef DEBUG
    Serial.print("a) More than one multiplier detected in packet\n");
#endif
    return 0;
  }

  // Does the packet "measure" more than one type of value?
  count  = (is_voltage) ? 1 : 0;
  count += (is_current) ? 1 : 0;
  count += (is_resistance) ? 1 : 0;
  count += (is_frequency) ? 1 : 0;
  count += (is_capacitance) ? 1 : 0;
  count += (is_temperature) ? 1 : 0;
  count += (is_continuity) ? 1 : 0;
  count += (is_diode) ? 1 : 0;
  count += (is_rpm) ? 1 : 0;

  if (count > 1) {
#ifdef DEBUG
    Serial.print("b) More than one measurement type detected in packet.\n");
#endif
    return 0;
  }

  // Both AC and DC set?
  if (is_ac && is_dc) {
#ifdef DEBUG
    Serial.print("Both AC and DC flags detected in packet\n");
#endif
    return 0;
  }

  return 1;
}


// private
void lw_ES519XX::_init (void)
{
  is_judge = 0; is_voltage = 0; is_auto = 0; is_micro = 0; is_current = 0;
  is_milli = 0; is_resistance = 0; is_continuity = 0; is_diode = 0;
  is_frequency = 0; is_rpm = 0; is_capacitance = 0; is_duty_cycle = 0;
  is_temperature = 0; is_celsius = 0; is_fahrenheit = 0;
  is_adp0 = 0; is_adp1 = 0; is_adp2 = 0; is_adp3 = 0;
  is_sign = 0; is_batt = 0; is_ol = 0; is_pmax = 0; is_pmin = 0; is_apo = 0;
  is_dc = 0; is_ac = 0; is_vahz = 0; is_min = 0; is_max = 0; is_rel = 0; is_hold = 0;
  is_digit4 = 0; is_ul = 0; is_vasel = 0; is_vbar = 0; is_lpf1 = 0; is_lpf0 = 0; is_rmr = 0;
  alt_functions = 0; fivedigits = 0; clampmeter = 0; selectable_lpf = 0;
  digits = 0;

  baudrate = 19200;
  packet_size = 14;
  exp_as_int = 0;
  val_as_int = 0;
  memset(raw_data, 0, 32);
  memset(val_as_str, 0, 32);
  memset(meaning, 0, 32);

  return;
}


bool lw_ES519XX::parse (uint8_t *buf)
{
  _init();


#ifdef PRINT_HEX
  int rdlen = 14;

  Serial.println("HEX:" 1);

  // first display as hex numbers then ASCII
  for (char *p = buf; rdlen-- > 0; p++) {
    Serial.print(*p, HEX);
    if (*p < ' ') {
      *p = '.';   // replace any control chars
    }
  }
  printf("\n    \"%s\"\n\n", buf);
#endif


  // parse 'buf' into our class private struct
  if (!_packet_valid(buf)) {
    //Serial.println("packet invalid");
    return 0;
  }

  // get its value
  if (!_parse_value()) {
    //Serial.println("parse_value invalid");
    return 0;
  }

  // get range
  if (!_parse_range()) {
    //Serial.println("parse_range invalid");
    return 0;
  }

  // set flags
  if (!_handle_flags()) {
    //Serial.println("handle_flags invalid");
    return 0;
  }

  //Serial.println("parse success");

  return 1;  // success
}



// end lw_ES519XX.cpp
