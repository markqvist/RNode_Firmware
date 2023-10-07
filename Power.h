#if BOARD_MODEL == BOARD_TBEAM
  #include <axp20x.h>
  AXP20X_Class PMU;

  #define BAT_V_MIN       3.15
  #define BAT_V_MAX       4.14

  void disablePeripherals() {
    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
  }
#elif BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1
  #define BAT_C_SAMPLES   7
  #define BAT_D_SAMPLES   2
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
#endif

uint32_t last_pmu_update = 0;
uint8_t pmu_target_pps = 1;
int pmu_update_interval = 1000/pmu_target_pps;
uint8_t pmu_rc = 0;
#define PMU_R_INTERVAL 5
void kiss_indicate_battery();

void measure_battery() {
  #if BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1
    battery_installed = true;
    battery_indeterminate = true;
    bat_v_samples[bat_samples_count%BAT_SAMPLES] = (float)(analogRead(pin_vbat)) / 4095*2*3.3*1.1;
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
      if (battery_percent > 100.0) battery_percent = 100.0;
      if (battery_percent < 0.0) battery_percent = 0.0;

      if (bat_samples_count%BAT_SAMPLES == 0) {
        if (battery_voltage < bat_delay_v && battery_voltage < BAT_V_FLOAT) {
          bat_voltage_dropping = true;
        } else {
          bat_voltage_dropping = false;
        }
        bat_samples_count = 0;
      }

      if (bat_voltage_dropping && battery_voltage < BAT_V_FLOAT) {
        battery_state = BATTERY_STATE_DISCHARGING;
      } else {
        #if BOARD_MODEL == BOARD_RNODE_NG_21
          battery_state = BATTERY_STATE_CHARGING;
        #else
          battery_state = BATTERY_STATE_DISCHARGING;
        #endif
      }



      // if (bt_state == BT_STATE_CONNECTED) {
      //   SerialBT.printf("Bus voltage %.3fv. Unfiltered %.3fv.", battery_voltage, bat_v_samples[BAT_SAMPLES-1]);
      //   if (bat_voltage_dropping) {
      //     SerialBT.printf(" Voltage is dropping. Percentage %.1f%%.\n", battery_percent);
      //   } else {
      //     SerialBT.print(" Voltage is not dropping.\n");
      //   }
      // }
    }

  #elif BOARD_MODEL == BOARD_TBEAM
    float discharge_current = PMU.getBattDischargeCurrent();
    float charge_current    = PMU.getBattChargeCurrent();
    battery_voltage         = PMU.getBattVoltage()/1000.0;
    // battery_percent         = PMU.getBattPercentage()*1.0;
    battery_installed       = PMU.isBatteryConnect();
    external_power          = PMU.isVBUSPlug();
    float ext_voltage       = PMU.getVbusVoltage()/1000.0;
    float ext_current       = PMU.getVbusCurrent();

    if (battery_installed) {
      if (PMU.isChargeing()) {
        battery_state = BATTERY_STATE_CHARGING;
        battery_percent = ((battery_voltage-BAT_V_MIN) / (BAT_V_MAX-BAT_V_MIN))*100.0;
      } else {
        if (discharge_current > 0.0) {
          battery_state = BATTERY_STATE_DISCHARGING;
          battery_percent = ((battery_voltage-BAT_V_MIN) / (BAT_V_MAX-BAT_V_MIN))*100.0;
        } else {
          battery_state = BATTERY_STATE_CHARGED;
          battery_percent = 100.0;
        }
      }
    } else {
      battery_state = BATTERY_STATE_DISCHARGING;
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
  #if BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1
    pinMode(pin_vbat, INPUT);
    return true;
  #elif BOARD_MODEL == BOARD_TBEAM
    Wire.begin(I2C_SDA, I2C_SCL);
    if (PMU.begin(Wire, AXP192_SLAVE_ADDRESS) == AXP_FAIL) return false;

    // Configure charging indicator
    PMU.setChgLEDMode(AXP20X_LED_OFF);

    // Turn off unused power sources to save power
    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_DCDC2, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_EXTEN, AXP202_OFF);

    // Set the power of LoRa and GPS module to 3.3V
    PMU.setLDO2Voltage(3300);   //LoRa VDD
    PMU.setLDO3Voltage(3300);   //GPS  VDD
    PMU.setDCDC1Voltage(3300);  //3.3V Pin next to 21 and 22 is controlled by DCDC1

    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_ON);

    // Turn on SX1276
    PMU.setPowerOutPut(AXP192_LDO2, AXP202_ON);

    // Turn off GPS
    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);

    pinMode(PMU_IRQ, INPUT_PULLUP);
    attachInterrupt(PMU_IRQ, [] {
      // pmu_irq = true;
    }, FALLING);

    PMU.adc1Enable(AXP202_VBUS_VOL_ADC1 |
                   AXP202_VBUS_CUR_ADC1 |
                   AXP202_BATT_CUR_ADC1 |
                   AXP202_BATT_VOL_ADC1,
                   AXP202_ON);

    PMU.enableIRQ(AXP202_VBUS_REMOVED_IRQ |
                  AXP202_VBUS_CONNECT_IRQ |
                  AXP202_BATT_REMOVED_IRQ |
                  AXP202_BATT_CONNECT_IRQ,
                  AXP202_ON);
    PMU.clearIRQ();

    return true; 
  #else
    return false;
  #endif
}
