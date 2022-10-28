#if BOARD_MODEL == BOARD_TBEAM
  #include <axp20x.h>
  AXP20X_Class PMU;

  void disablePeripherals() {
    PMU.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
    PMU.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
  }
#elif BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1
  #define BAT_V_MIN  3.4
  #define BAT_V_MAX  4.2
  #define BAT_V_CHG  4.345
  #define BAT_V_CHGD 4.31
  const uint8_t pin_vbat = 35;
#endif

uint32_t last_pmu_update = 0;
uint8_t pmu_target_pps = 1;
int pmu_update_interval = 1000/pmu_target_pps;

void measure_battery() {
  #if BOARD_MODEL == BOARD_RNODE_NG_21 || BOARD_MODEL == BOARD_LORA32_V2_1
    battery_voltage = (float)(analogRead(pin_vbat)) / 4095*2*3.3*1.1;
    battery_percent = ((battery_voltage-BAT_V_MIN) / (BAT_V_MAX-BAT_V_MIN))*100.0;
    
    if (battery_percent > 100.0) battery_percent = 100.0;

    if (battery_voltage > BAT_V_CHG) {
      battery_state = BATTERY_STATE_CHARGING;
      // Serial.printf("Battery charging. Voltage=%.2fv, percentage: %.2f%\n", battery_voltage, battery_percent);
    } else if (battery_voltage > BAT_V_CHGD) {
      battery_state = BATTERY_STATE_CHARGED;
      // Serial.printf("Battery charged. Voltage=%.2fv, percentage: %.2f%\n", battery_voltage, battery_percent);
    } else {
      battery_state = BATTERY_STATE_DISCHARGING;
      // Serial.printf("Battery discharging. Voltage=%.2fv, percentage: %.2f%\n", battery_voltage, battery_percent);
    }

  #elif BOARD_MODEL == BOARD_TBEAM
    battery_voltage = 0.0;
    battery_percent = 0.0;
  #endif
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
