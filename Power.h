// Copyright (C) 2024, Mark Qvist

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#if BOARD_MODEL == BOARD_TBEAM || BOARD_MODEL == BOARD_TBEAM_S_V1
  #include <XPowersLib.h>
  XPowersLibInterface* PMU = NULL;

  #ifndef PMU_WIRE_PORT
    #if BOARD_MODEL == BOARD_TBEAM_S_V1
      #define PMU_WIRE_PORT   Wire1
    #else
      #define PMU_WIRE_PORT   Wire
    #endif
  #endif

  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.14

  void disablePeripherals() {
    if (PMU) {
      // GNSS RTC PowerVDD
      PMU->enablePowerOutput(XPOWERS_VBACKUP);

      // LoRa VDD
      PMU->disablePowerOutput(XPOWERS_ALDO2);

      // GNSS VDD
      PMU->disablePowerOutput(XPOWERS_ALDO3);
    }
  }

  bool pmuInterrupt;
  void setPmuFlag()
  {
      pmuInterrupt = true;
  }
#elif BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1
  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.3
  #define BAT_V_CHG       4.48
  #define BAT_V_FLOAT     4.33
  #define BAT_SAMPLES     5
  const uint8_t pin_vbat = 35;
  float bat_p_samples[BAT_SAMPLES];
  float bat_v_samples[BAT_SAMPLES];
  uint8_t bat_samples_count = 0;
  int bat_discharging_samples = 0;
  int bat_charging_samples = 0;
  int bat_charged_samples = 0;
  bool bat_voltage_dropping = false;
  float bat_delay_v = 0;
  float bat_state_change_v = 0;
#elif BOARD_MODEL == BOARD_T3S3
  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.217
  #define BAT_V_CHG       4.48
  #define BAT_V_FLOAT     4.33
  #define BAT_SAMPLES     5
  const uint8_t pin_vbat = 1;
  float bat_p_samples[BAT_SAMPLES];
  float bat_v_samples[BAT_SAMPLES];
  uint8_t bat_samples_count = 0;
  int bat_discharging_samples = 0;
  int bat_charging_samples = 0;
  int bat_charged_samples = 0;
  bool bat_voltage_dropping = false;
  float bat_delay_v = 0;
  float bat_state_change_v = 0;
#elif BOARD_MODEL == BOARD_TDECK
  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.3
  #define BAT_V_CHG       4.48
  #define BAT_V_FLOAT     4.33
  #define BAT_SAMPLES     5
  const uint8_t pin_vbat = 4;
  float bat_p_samples[BAT_SAMPLES];
  float bat_v_samples[BAT_SAMPLES];
  uint8_t bat_samples_count = 0;
  int bat_discharging_samples = 0;
  int bat_charging_samples = 0;
  int bat_charged_samples = 0;
  bool bat_voltage_dropping = false;
  float bat_delay_v = 0;
  float bat_state_change_v = 0;
#elif BOARD_MODEL == BOARD_HELTEC32_V3
  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.3
  #define BAT_V_CHG       4.48
  #define BAT_V_FLOAT     4.33
  #define BAT_SAMPLES     7
  const uint8_t pin_vbat = 1;
  const uint8_t pin_ctrl = 37;
  float bat_p_samples[BAT_SAMPLES];
  float bat_v_samples[BAT_SAMPLES];
  uint8_t bat_samples_count = 0;
  int bat_discharging_samples = 0;
  int bat_charging_samples = 0;
  int bat_charged_samples = 0;
  bool bat_voltage_dropping = false;
  float bat_delay_v = 0;
  float bat_state_change_v = 0;
#elif BOARD_MODEL == BOARD_HELTEC_T114
  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.165
  #define BAT_V_CHG       4.48
  #define BAT_V_FLOAT     4.33
  #define BAT_SAMPLES     7
  const uint8_t pin_vbat = 4;
  const uint8_t pin_ctrl = 6;
  float bat_p_samples[BAT_SAMPLES];
  float bat_v_samples[BAT_SAMPLES];
  uint8_t bat_samples_count = 0;
  int bat_discharging_samples = 0;
  int bat_charging_samples = 0;
  int bat_charged_samples = 0;
  bool bat_voltage_dropping = false;
  float bat_delay_v = 0;
  float bat_state_change_v = 0;
#elif BOARD_MODEL == BOARD_TECHO
  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.16
  #define BAT_V_CHG       4.48
  #define BAT_V_FLOAT     4.33
  #define BAT_SAMPLES     7
  const uint8_t pin_vbat = 4;
  float bat_p_samples[BAT_SAMPLES];
  float bat_v_samples[BAT_SAMPLES];
  uint8_t bat_samples_count = 0;
  int bat_discharging_samples = 0;
  int bat_charging_samples = 0;
  int bat_charged_samples = 0;
  bool bat_voltage_dropping = false;
  float bat_delay_v = 0;
  float bat_state_change_v = 0;
#endif

uint32_t last_pmu_update = 0;
uint8_t pmu_target_pps = 1;
int pmu_update_interval = 1000/pmu_target_pps;
uint8_t pmu_rc = 0;
#define PMU_R_INTERVAL 5
void kiss_indicate_battery();

void measure_battery() {
  #if BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1 || BOARD_MODEL == BOARD_HELTEC32_V3 || BOARD_MODEL == BOARD_TDECK || BOARD_MODEL == BOARD_T3S3 || BOARD_MODEL == BOARD_HELTEC_T114 || BOARD_MODEL == BOARD_TECHO
    battery_installed = true;
    battery_indeterminate = true;

    #if BOARD_MODEL == BOARD_HELTEC32_V3
      float battery_measurement = (float)(analogRead(pin_vbat)) * 0.0041;
    #elif BOARD_MODEL == BOARD_T3S3
      float battery_measurement = (float)(analogRead(pin_vbat)) / 4095.0*6.7828;
    #elif BOARD_MODEL == BOARD_HELTEC_T114
      float battery_measurement = (float)(analogRead(pin_vbat)) * 0.017165;
    #elif BOARD_MODEL == BOARD_TECHO
      float battery_measurement = (float)(analogRead(pin_vbat)) * 0.007067;
    #else
      float battery_measurement = (float)(analogRead(pin_vbat)) / 4095.0*7.26;
    #endif

    bat_v_samples[bat_samples_count%BAT_SAMPLES] = battery_measurement;
    bat_p_samples[bat_samples_count%BAT_SAMPLES] = ((battery_voltage-BAT_V_MIN) / (BAT_V_MAX-BAT_V_MIN))*100.0;
    
    bat_samples_count++;
    if (!battery_ready && bat_samples_count >= BAT_SAMPLES) {
      battery_ready = true;
    }

    if (battery_ready) {

      battery_percent = 0;
      for (uint8_t bi = 0; bi < BAT_SAMPLES; bi++) {
        battery_percent += bat_p_samples[bi];
      }
      battery_percent = battery_percent/BAT_SAMPLES;
      
      battery_voltage = 0;
      for (uint8_t bi = 0; bi < BAT_SAMPLES; bi++) {
        battery_voltage += bat_v_samples[bi];
      }
      battery_voltage = battery_voltage/BAT_SAMPLES;
      
      if (bat_delay_v == 0) bat_delay_v = battery_voltage;
      if (bat_state_change_v == 0) bat_state_change_v = battery_voltage;
      if (battery_percent > 100.0) battery_percent = 100.0;
      if (battery_percent < 0.0) battery_percent = 0.0;

      if (bat_samples_count%BAT_SAMPLES == 0) {
        float bat_delay_diff = bat_state_change_v-battery_voltage;
        if (bat_delay_diff < 0) { bat_delay_diff *= -1; }

        if (battery_voltage < bat_delay_v && battery_voltage < BAT_V_FLOAT) {
          if (bat_voltage_dropping == false) {
            if (bat_delay_diff > 0.008) {
              bat_voltage_dropping = true;
              bat_state_change_v = battery_voltage;
              // SerialBT.printf("STATE CHANGE to DISCHARGE at delta=%.3fv. State change v is now %.3fv.\n", bat_delay_diff, bat_state_change_v);
            }
          }
        } else {
          if (bat_voltage_dropping == true) {
            if (bat_delay_diff > 0.01) {
              bat_voltage_dropping = false;
              bat_state_change_v = battery_voltage;
              // SerialBT.printf("STATE CHANGE to CHARGE at delta=%.3fv. State change v is now %.3fv.\n", bat_delay_diff, bat_state_change_v);
            }
          }
        }
        bat_samples_count = 0;
        bat_delay_v = battery_voltage;
      }

      if (bat_voltage_dropping && battery_voltage < BAT_V_FLOAT) {
        battery_state = BATTERY_STATE_DISCHARGING;
      } else {
        if (battery_percent < 100.0) {
          battery_state = BATTERY_STATE_CHARGING;
        } else {
          battery_state = BATTERY_STATE_CHARGED;
        }
      }

      #if MCU_VARIANT == MCU_NRF52
        if (bt_state != BT_STATE_OFF) { blebas.write(battery_percent); }
      #endif

      // if (bt_state == BT_STATE_CONNECTED) {
      //   SerialBT.printf("Bus voltage %.3fv. Unfiltered %.3fv.", battery_voltage, bat_v_samples[BAT_SAMPLES-1]);
      //   if (bat_voltage_dropping) { SerialBT.printf(" Voltage is dropping. Percentage %.1f%%.", battery_percent); }
      //   else                      { SerialBT.printf(" Voltage is not dropping. Percentage %.1f%%.", battery_percent); }
      //   if (battery_state == BATTERY_STATE_DISCHARGING) { SerialBT.printf(" Battery discharging. delay_v %.3fv", bat_delay_v); }
      //   if (battery_state == BATTERY_STATE_CHARGING) { SerialBT.printf(" Battery charging. delay_v %.3fv", bat_delay_v); }
      //   if (battery_state == BATTERY_STATE_CHARGED) { SerialBT.print(" Battery is charged."); }
      //   SerialBT.print("\n");
      // }
    }

  #elif BOARD_MODEL == BOARD_TBEAM || BOARD_MODEL == BOARD_TBEAM_S_V1
    if (PMU) {
      float discharge_current = 0;
      float charge_current    = 0;
      float ext_voltage       = 0;
      float ext_current       = 0;
      if (PMU->getChipModel() == XPOWERS_AXP192) {
        discharge_current       = ((XPowersAXP192*)PMU)->getBattDischargeCurrent();
        charge_current          = ((XPowersAXP192*)PMU)->getBatteryChargeCurrent();
        battery_voltage         = PMU->getBattVoltage()/1000.0;
        // battery_percent         = PMU->getBattPercentage()*1.0;
        battery_installed       = PMU->isBatteryConnect();
        external_power          = PMU->isVbusIn();
        ext_voltage             = PMU->getVbusVoltage()/1000.0;
        ext_current             = ((XPowersAXP192*)PMU)->getVbusCurrent();
      }
      else if (PMU->getChipModel() == XPOWERS_AXP2101) {
        battery_voltage         = PMU->getBattVoltage()/1000.0;
        // battery_percent         = PMU->getBattPercentage()*1.0;
        battery_installed       = PMU->isBatteryConnect();
        external_power          = PMU->isVbusIn();
        ext_voltage             = PMU->getVbusVoltage()/1000.0;
      }

      if (battery_installed) {
        if (PMU->isCharging()) {
          battery_state = BATTERY_STATE_CHARGING;
          battery_percent = ((battery_voltage-BAT_V_MIN) / (BAT_V_MAX-BAT_V_MIN))*100.0;
        } else {
          if (PMU->isDischarge()) {
            battery_state = BATTERY_STATE_DISCHARGING;
            battery_percent = ((battery_voltage-BAT_V_MIN) / (BAT_V_MAX-BAT_V_MIN))*100.0;
          } else {
            battery_state = BATTERY_STATE_CHARGED;
            battery_percent = 100.0;
          }
        }
      } else {
        battery_state = BATTERY_STATE_UNKNOWN;
        battery_percent = 0.0;
        battery_voltage = 0.0;
      }

      if (battery_percent > 100.0) battery_percent = 100.0;
      if (battery_percent < 0.0) battery_percent = 0.0;

      float charge_watts    = battery_voltage*(charge_current/1000.0);
      float discharge_watts = battery_voltage*(discharge_current/1000.0);
      float ext_watts       = ext_voltage*(ext_current/1000.0);

      battery_ready = true;

      // if (bt_state == BT_STATE_CONNECTED) {
      //   if (battery_installed) {
      //     if (external_power) {
      //       SerialBT.printf("External power connected, drawing %.2fw, %.1fmA at %.1fV\n", ext_watts, ext_current, ext_voltage);
      //     } else {
      //       SerialBT.println("Running on battery");
      //     }
      //     SerialBT.printf("Battery percentage %.1f%%\n", battery_percent);
      //     SerialBT.printf("Battery voltage %.2fv\n", battery_voltage);
      //     // SerialBT.printf("Temperature %.1f%\n", auxillary_temperature);

      //     if (battery_state == BATTERY_STATE_CHARGING) {
      //       SerialBT.printf("Charging with %.2fw, %.1fmA at %.1fV\n", charge_watts, charge_current, battery_voltage);
      //     } else if (battery_state == BATTERY_STATE_DISCHARGING) {
      //       SerialBT.printf("Discharging at %.2fw, %.1fmA at %.1fV\n", discharge_watts, discharge_current, battery_voltage);
      //     } else if (battery_state == BATTERY_STATE_CHARGED) {
      //       SerialBT.printf("Battery charged\n");
      //     }
      //   } else {
      //     SerialBT.println("No battery installed");
      //   }
      //   SerialBT.println("");
      // }
    }
    else {
      battery_ready = false;
    }
  #endif

  if (battery_ready) {
    pmu_rc++;
    if (pmu_rc%PMU_R_INTERVAL == 0) {
      kiss_indicate_battery();
    }
  }
}

void update_pmu() {
  if (millis()-last_pmu_update >= pmu_update_interval) {
    measure_battery();
    last_pmu_update = millis();
  }
}

bool init_pmu() {
  #if BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1 || BOARD_MODEL == BOARD_TDECK || BOARD_MODEL == BOARD_T3S3 || BOARD_MODEL == BOARD_TECHO
    pinMode(pin_vbat, INPUT);
    return true;
  #elif BOARD_MODEL == BOARD_HELTEC32_V3
    pinMode(pin_ctrl,OUTPUT);
    digitalWrite(pin_ctrl, LOW);
    return true;
  #elif BOARD_MODEL == BOARD_HELTEC_T114
    pinMode(pin_ctrl,OUTPUT);
    digitalWrite(pin_ctrl, HIGH);
    return true;
  #elif BOARD_MODEL == BOARD_TBEAM
    Wire.begin(I2C_SDA, I2C_SCL);

    if (!PMU) {
        PMU = new XPowersAXP2101(PMU_WIRE_PORT);
        if (!PMU->init()) {
            delete PMU;
            PMU = NULL;
        }
    }

    if (!PMU) {
        PMU = new XPowersAXP192(PMU_WIRE_PORT);
        if (!PMU->init()) {
            delete PMU;
            PMU = NULL;
        }
    }

    if (!PMU) {
        return false;
    }

    // Configure charging indicator
    PMU->setChargingLedMode(XPOWERS_CHG_LED_OFF);

    pinMode(PMU_IRQ, INPUT_PULLUP);
    attachInterrupt(PMU_IRQ, setPmuFlag, FALLING);

    if (PMU->getChipModel() == XPOWERS_AXP192) {

      // Turn off unused power sources to save power
      PMU->disablePowerOutput(XPOWERS_DCDC1);
      PMU->disablePowerOutput(XPOWERS_DCDC2);
      PMU->disablePowerOutput(XPOWERS_LDO2);
      PMU->disablePowerOutput(XPOWERS_LDO3);

      // Set the power of LoRa and GPS module to 3.3V
      // LoRa
      PMU->setPowerChannelVoltage(XPOWERS_LDO2, 3300);
      // GPS
      PMU->setPowerChannelVoltage(XPOWERS_LDO3, 3300);
      // OLED
      PMU->setPowerChannelVoltage(XPOWERS_DCDC1, 3300);

      // Turn on LoRa
      PMU->enablePowerOutput(XPOWERS_LDO2);

      // Turn on GPS
      //PMU->enablePowerOutput(XPOWERS_LDO3);

      // protected oled power source
      PMU->setProtectedChannel(XPOWERS_DCDC1);
      // protected esp32 power source
      PMU->setProtectedChannel(XPOWERS_DCDC3);
      // enable oled power
      PMU->enablePowerOutput(XPOWERS_DCDC1);

      PMU->disableIRQ(XPOWERS_AXP192_ALL_IRQ);

      PMU->enableIRQ(XPOWERS_AXP192_VBUS_REMOVE_IRQ |
                      XPOWERS_AXP192_VBUS_INSERT_IRQ |
                      XPOWERS_AXP192_BAT_CHG_DONE_IRQ |
                      XPOWERS_AXP192_BAT_CHG_START_IRQ |
                      XPOWERS_AXP192_BAT_REMOVE_IRQ |
                      XPOWERS_AXP192_BAT_INSERT_IRQ |
                      XPOWERS_AXP192_PKEY_SHORT_IRQ
                    );

    }
    else if (PMU->getChipModel() == XPOWERS_AXP2101) {

      // Turn off unused power sources to save power
      PMU->disablePowerOutput(XPOWERS_DCDC2);
      PMU->disablePowerOutput(XPOWERS_DCDC3);
      PMU->disablePowerOutput(XPOWERS_DCDC4);
      PMU->disablePowerOutput(XPOWERS_DCDC5);
      PMU->disablePowerOutput(XPOWERS_ALDO1);
      PMU->disablePowerOutput(XPOWERS_ALDO2);
      PMU->disablePowerOutput(XPOWERS_ALDO3);
      PMU->disablePowerOutput(XPOWERS_ALDO4);
      PMU->disablePowerOutput(XPOWERS_BLDO1);
      PMU->disablePowerOutput(XPOWERS_BLDO2);
      PMU->disablePowerOutput(XPOWERS_DLDO1);
      PMU->disablePowerOutput(XPOWERS_DLDO2);
      PMU->disablePowerOutput(XPOWERS_VBACKUP);

      // Set the power of LoRa and GPS module to 3.3V
      // LoRa
      PMU->setPowerChannelVoltage(XPOWERS_ALDO2, 3300);
      // GPS
      PMU->setPowerChannelVoltage(XPOWERS_ALDO3, 3300);
      PMU->setPowerChannelVoltage(XPOWERS_VBACKUP, 3300);

      // ESP32 VDD
      // ! No need to set, automatically open , Don't close it
      // PMU->setPowerChannelVoltage(XPOWERS_DCDC1, 3300);
      // PMU->setProtectedChannel(XPOWERS_DCDC1);
      PMU->setProtectedChannel(XPOWERS_DCDC1);

      // LoRa VDD
      PMU->enablePowerOutput(XPOWERS_ALDO2);

      // GNSS VDD
      //PMU->enablePowerOutput(XPOWERS_ALDO3);

      // GNSS RTC PowerVDD
      //PMU->enablePowerOutput(XPOWERS_VBACKUP);
    }

    PMU->enableSystemVoltageMeasure();
    PMU->enableVbusVoltageMeasure();
    PMU->enableBattVoltageMeasure();
    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU->disableTSPinMeasure();

    // Set the time of pressing the button to turn off
    PMU->setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);

    return true; 
  #elif BOARD_MODEL == BOARD_TBEAM_S_V1
    Wire1.begin(I2C_SDA, I2C_SCL);

    if (!PMU) {
        PMU = new XPowersAXP2101(PMU_WIRE_PORT);
        if (!PMU->init()) {
            delete PMU;
            PMU = NULL;
        }
    }

    if (!PMU) {
      return false;
    }

    /**
     * gnss module power channel
     * The default ALDO4 is off, you need to turn on the GNSS power first, otherwise it will be invalid during
     * initialization
     */
    PMU->setPowerChannelVoltage(XPOWERS_ALDO4, 3300);
    PMU->enablePowerOutput(XPOWERS_ALDO4);

    // lora radio power channel
    PMU->setPowerChannelVoltage(XPOWERS_ALDO3, 3300);
    PMU->enablePowerOutput(XPOWERS_ALDO3);

    // m.2 interface
    PMU->setPowerChannelVoltage(XPOWERS_DCDC3, 3300);
    PMU->enablePowerOutput(XPOWERS_DCDC3);

    /**
     * ALDO2 cannot be turned off.
     * It is a necessary condition for sensor communication.
     * It must be turned on to properly access the sensor and screen
     * It is also responsible for the power supply of PCF8563
     */
    PMU->setPowerChannelVoltage(XPOWERS_ALDO2, 3300);
    PMU->enablePowerOutput(XPOWERS_ALDO2);

    // 6-axis , magnetometer ,bme280 , oled screen power channel
    PMU->setPowerChannelVoltage(XPOWERS_ALDO1, 3300);
    PMU->enablePowerOutput(XPOWERS_ALDO1);

    // sdcard power channle
    PMU->setPowerChannelVoltage(XPOWERS_BLDO1, 3300);
    PMU->enablePowerOutput(XPOWERS_BLDO1);

    // PMU->setPowerChannelVoltage(XPOWERS_DCDC4, 3300);
    // PMU->enablePowerOutput(XPOWERS_DCDC4);

    // not use channel
    PMU->disablePowerOutput(XPOWERS_DCDC2); // not elicited
    PMU->disablePowerOutput(XPOWERS_DCDC5); // not elicited
    PMU->disablePowerOutput(XPOWERS_DLDO1); // Invalid power channel, it does not exist
    PMU->disablePowerOutput(XPOWERS_DLDO2); // Invalid power channel, it does not exist
    PMU->disablePowerOutput(XPOWERS_VBACKUP);

    // Configure charging
    PMU->setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    PMU->setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
    // TODO: Reset
    PMU->setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);

    // Set the time of pressing the button to turn off
    PMU->setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    PMU->setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);

    // disable all axp chip interrupt
    PMU->disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU->clearIrqStatus();

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU->disableTSPinMeasure();
    PMU->enableVbusVoltageMeasure();
    PMU->enableBattVoltageMeasure();


    return true; 
  #else
    return false;
  #endif
}
