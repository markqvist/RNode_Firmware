// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license.

// Modifications and additions copyright 2023 by Mark Qvist
// Obviously still under the MIT license.

#include "Boards.h"

#if MODEM == LR1110
#include "lr1110.h"

// todo, why is this in modem?
#if MCU_VARIANT == MCU_ESP32
//  #if MCU_VARIANT == MCU_ESP32 and !defined(CONFIG_IDF_TARGET_ESP32S3)
//    #include "soc/rtc_wdt.h"
//  #endif

// todo, why is this in modem?
//https://github.com/espressif/esp-idf/issues/8855
#include "hal/wdt_hal.h"

  #define ISR_VECT IRAM_ATTR
#else
  #define ISR_VECT
#endif

int debug_print_enabled = 0;

volatile uint8_t TXCompl_flag = 0;

#define OP_RF_FREQ_6X               0x86
#define OP_SLEEP_6X                 0x84
#define OP_STANDBY_6X               0x80
#define OP_TX_6X                    0x83
#define OP_RX_6X                    0x82
#define OP_PA_CONFIG_6X             0x95
#define OP_SET_IRQ_FLAGS_6X         0x08 // also provides info such as
                                      // preamble detection, etc for
                                      // knowing when it's safe to switch
                                      // antenna modes
#define OP_CLEAR_IRQ_STATUS_6X      0x02
#define OP_GET_IRQ_STATUS_6X        0x12
#define OP_RX_BUFFER_STATUS_6X      0x13
#define OP_PACKET_STATUS_6X         0x14 // get snr & rssi of last packet
#define OP_CURRENT_RSSI_6X          0x15
#define OP_MODULATION_PARAMS_6X     0x8B // bw, sf, cr, etc.
#define OP_PACKET_PARAMS_6X         0x8C // crc, preamble, payload length, etc.
#define OP_STATUS_6X                0xC0
#define OP_TX_PARAMS_6X             0x8E // set dbm, etc
#define OP_PACKET_TYPE_6X           0x8A
#define OP_BUFFER_BASE_ADDR_6X      0x8F
#define OP_READ_REGISTER_6X         0x1D
#define OP_WRITE_REGISTER_6X        0x0D
#define OP_DIO3_TCXO_CTRL_6X        0x97
#define OP_DIO2_RF_CTRL_6X          0x9D
#define OP_CAD_PARAMS               0x88
#define OP_CALIBRATE_6X             0x89
#define OP_RX_TX_FALLBACK_MODE_6X   0x93
#define OP_REGULATOR_MODE_6X        0x96
#define OP_CALIBRATE_IMAGE_6X       0x98

#define MASK_CALIBRATE_ALL          0x7f

#define IRQ_TX_DONE_MASK_6X         0x01
#define IRQ_RX_DONE_MASK_6X         0x02
#define IRQ_HEADER_DET_MASK_6X      0x10
#define IRQ_PREAMBLE_DET_MASK_6X    0x04
#define IRQ_PAYLOAD_CRC_ERROR_MASK_6X 0x40
#define IRQ_ALL_MASK_6X             0b0100001111111111

#define MODE_LONG_RANGE_MODE_6X     0x01

#define OP_FIFO_WRITE_6X            0x0E
#define OP_FIFO_READ_6X             0x1E
#define REG_OCP_6X                0x08E7
#define REG_LNA_6X                0x08AC // no agc in sx1262
#define REG_SYNC_WORD_MSB_6X      0x0740
#define REG_SYNC_WORD_LSB_6X      0x0741
#define REG_PAYLOAD_LENGTH_6X     0x0702 // https://github.com/beegee-tokyo/SX126x-Arduino/blob/master/src/radio/sx126x/sx126x.h#L98
#define REG_RANDOM_GEN_6X         0x0819

#define MODE_TCXO_3_3V_6X           0x07
#define MODE_TCXO_3_0V_6X           0x06
#define MODE_TCXO_2_7V_6X           0x06
#define MODE_TCXO_2_4V_6X           0x06
#define MODE_TCXO_2_2V_6X           0x03
#define MODE_TCXO_1_8V_6X           0x02
#define MODE_TCXO_1_7V_6X           0x01
#define MODE_TCXO_1_6V_6X           0x00

#define MODE_STDBY_RC_6X            0x00
#define MODE_STDBY_XOSC_6X          0x01
#define MODE_FALLBACK_STDBY_RC_6X   0x20
#define MODE_IMPLICIT_HEADER        0x01
#define MODE_EXPLICIT_HEADER        0x00

#define SYNC_WORD_6X              0x1424

#define XTAL_FREQ_6X (double)32000000
#define FREQ_DIV_6X (double)pow(2.0, 25.0)
#define FREQ_STEP_6X (double)(XTAL_FREQ_6X / FREQ_DIV_6X)

#if defined(NRF52840_XXAA)
  extern SPIClass spiModem;
  #define SPI spiModem
#endif

//extern SPIClass SPI;

#define MAX_PKT_LENGTH           255

lr11xx::lr11xx() :
//  _spiSettings(8E6, MSBFIRST, SPI_MODE0),
  _spiSettings(8e8, MSBFIRST, SPI_MODE0),
  _ss(LORA_DEFAULT_SS_PIN), _reset(LORA_DEFAULT_RESET_PIN), _dio0(LORA_DEFAULT_DIO0_PIN), _busy(LORA_DEFAULT_BUSY_PIN), _rxen(LORA_DEFAULT_RXEN_PIN),
  _frequency(0),
  _txp(0),
  _sf(0x07),
  _bw(0x04),
  _cr(0x01),
  _ldro(0x00),
  _packetIndex(0),
  _packetIndexRX(0),
  _preambleLength(18),
  _implicitHeaderMode(0),
  _payloadLength(255),
  _crcMode(1),
  _fifo_tx_addr_ptr(0),
  _fifo_rx_addr_ptr(0),
  _packet{0},
  _packetRX{0},
  _preinit_done(false),
  _onReceive(NULL)
{
  // overide Stream timeout value
  setTimeout(0);
  modulationDirty = packetParamsDirty = 1;
}

// debug - info
uint8_t lOpState = 0;
uint8_t readOpState = 0;
// INT tracking
int  _db_pktLen;
int  _db_intcnt, _db_rxcnt;




bool lr11xx::preInit() {

  Serial.println("XXXXX");
  Serial.println("PreInit");
  Serial.println("XXXXX");

#if 0
  NRF_CLOCK->EVENTS_HFCLKSTARTED  = 0;
  NRF_CLOCK->TASKS_HFCLKSTART = 1;
  while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
  {
    // wait for start
  }
#endif

#if 1
  NRF_CLOCK->EVENTS_LFCLKSTARTED  = 0;
  NRF_CLOCK->TASKS_LFCLKSTART = 1;
  while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
  {
    // wait for start
  }
#endif

  // generate byte of random with built in 
  // Random Number Gen (RNG)
  NRF_RNG->EVENTS_VALRDY = 0;
  NRF_RNG->TASKS_START = 1;
  while (NRF_RNG->EVENTS_VALRDY == 0)
  {
    // wait for start
  }
  uint8_t rand1 = NRF_RNG->VALUE;
  NRF_RNG->TASKS_STOP = 1;

  Serial.print("lr1110 random ");
  Serial.println(rand1);


//    SPI.begin();

//---
//void Wm1110Hardware::reset()
// reset LR1110
//---
//ss high
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);

//nreset low 200us
//nreset high 200us
// delays 1ms here
// hangs currently
//  reset();
//busy should go low and radio in standby

//    waitOnBusy();

///// --------------------------
// PA setup

// tx power setup
// ral_lr11xx_bsp_get_tx_cfg -->
// lr11xx_get_tx_cfg - radio_firmware_updater - platformio
// HP and LP PA enabled below 2.4G
//case LR11XX_WITH_LF_LP_HP_PA:

// 2.4G is an option, when lower freq, then set:
//  pa_type = LR11XX_WITH_LF_LP_HP_PA;  // = 2
    //output_params->pa_ramp_time = LR11XX_RADIO_RAMP_48_US;  // = 2






///// --------------------------
// tcxo
// uint32_t startup_time_ms = smtc_modem_hal_get_radio_tcxo_startup_delay_ms( );   = return 30;
//   *xosc_cfg                = RAL_XOSC_CFG_TCXO_RADIO_CTRL;  // = 1
//    *supply_voltage          = LR11XX_SYSTEM_TCXO_CTRL_1_8V; // =  2
//    // tick is 30.52µs
//    *startup_time_in_tick = lr11xx_radio_convert_time_in_ms_to_rtc_step( startup_time_ms );
  // return ( uint32_t ) ( time_in_ms * LR11XX_RTC_FREQ_IN_HZ / 1000 );  
 // #define LR11XX_RTC_FREQ_IN_HZ 32768UL

#if 0
// tcxo notes
lr11xx_status_t lr11xx_system_set_tcxo_mode( const void* context, const lr11xx_system_tcxo_supply_voltage_t tune,
                                             const uint32_t timeout )
{
    const uint8_t cbuffer[LR11XX_SYSTEM_SET_TCXO_MODE_CMD_LENGTH] = {  // 6
        ( uint8_t ) ( LR11XX_SYSTEM_SET_TCXO_MODE_OC >> 8 ),   // 279 = 0x0117
        ( uint8_t ) ( LR11XX_SYSTEM_SET_TCXO_MODE_OC >> 0 ),
        ( uint8_t ) tune,
        ( uint8_t ) ( timeout >> 16 ),
        ( uint8_t ) ( timeout >> 8 ),
        ( uint8_t ) ( timeout >> 0 ),
    };
#endif
///// --------------------------


//  pinMode(_ss, OUTPUT);
//  digitalWrite(_ss, HIGH);

  Serial.println("==================");
  Serial.println("------------------");
  Serial.print("lr1110 _ss ");
  Serial.println(_ss);



#if 1
  #if BOARD_MODEL == BOARD_RNODE_NG_22 || BOARD_MODEL == BOARD_HELTEC_LORA32_V3 || BOARD_MODEL == BOARD_HELTEC_CAPSULE_V3 || BOARD_MODEL == BOARD_HELTEC_CAPSULE_V3 || BOARD_MODEL == BOARD_HELTEC_WIRELESS_PAPER_1_1
    SPI.begin(pin_sclk, pin_miso, pin_mosi, pin_cs);
  #else
    SPI.begin();
  #endif
#endif


// XXXXXXXXXXXXXXXXX  -- move these out so we can do after reset
#if 0

// SetDioAsRfSwitch
//C:\Users\cobra\AppData\Local\Arduino15\packages\Seeeduino\hardware\nrf52\1.1.8\libraries\LBM_WM1110\src\internal\lbm_hal\ral_lr11xx_bsp.c
// ral_lr11xx_bsp_get_rf_switch_cfg()
//uint8_t RfSwEn = 0b1111;  // enable  DIO10 - DIO9 - DIO8 - DIO7 - DIO6 - DIO5
uint8_t RfSwEn = 0b1111;  // enable  DIO10 - DIO8 - DIO7 - DIO6 - DIO5
// standby none
#if 0
// initial match to firmware / Wio tracker dev board
uint8_t rx_enable = 0b1;  // switch 0
uint8_t tx_enable = 0b11;  // switch 0 and 1
uint8_t tx_hp_enable = 0b10;  // switch 1
#else  // T1000-E 
uint8_t rx_enable = 0b1001; 
uint8_t tx_enable = 0b1011;  
uint8_t tx_hp_enable = 0b1010;  
#endif
#if 0
uint8_t gnss_enable = 0b100;  // switch 2
uint8_t wifi_enable = 0b1000;  // switch 3
#else
uint8_t gnss_enable = 0b0;  // switch 2 - internal GNSS
uint8_t wifi_enable = 0b0;  // switch 3 - Wifi scanner
#endif

uint8_t cmd5[13] = {10,0,0x1,0x12, 
  RfSwEn, 
  0, // none - standby

// 0x9, 0xb, 0xa ?  1001, 1011, 1010

  rx_enable,
  tx_enable,
  tx_hp_enable,
  0,  // unused
  gnss_enable,
  wifi_enable
  };

cmdTransfer("dio sw cfg", cmd5);

///// XXXXXXXX DISABLED

#if defined(BOARD_WIO_TRACK_1110_DEV)
// set tcxo mode
// tune = 2, 1.8V - wio tracker dev
// time 983 = 0x3d7
//uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x3, 0xd7};
// 1.6V, longer delay
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x3, 0xf2};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)
// 1.8V, shorter delay
uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x0, 0xa4};
#else // BOARD_SENSECAP_TRACKER_T1000E 
// tune = 0 , 1.6V tracker t1000-e
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x3, 0xd7};
// longer delay
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x6, 0xd7};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)
uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};

#endif

cmdTransfer("tcxo", cmd4);



//setpackettype
uint8_t cmd[10] = {3,0,0x2,0xe,2};

cmdTransfer("packettype lora", cmd);

// geterrors
uint8_t cmd3[10] = {2,3, 1,0xd};

cmdTransfer("errors?", cmd3);


// setmodulationparams

//uint8_t cmd2[10] = {6,0, 0x02, 0x0f, 0x07 /*SF*/, 0x05 /*BW*/, 0x04 /*CR*/, 0 /*LDR*/};
uint8_t cmd2[10] = {6,0, 0x02, 0x0f, _sf /*SF*/, _bw /*BW*/, _cr /*CR*/, _ldro /*LDR*/};
cmdTransfer("modparams", cmd2);

// geterrors
//uint8_t cmd3[10] = {2,3, 1,0xd};

cmdTransfer("errors", cmd3);


// setpacketparams

// setPAconfig

// setTXParams


// XXXXXXXXXXXXXXXXX  -- move these out so we can do after reset
#endif




  _preinit_done = true;
  return true;
}


void lr11xx::doSetup(void)
{

//Serial.println("Doing Setup  - dio  DCDC  - TCXO     >>>>>>>>>>>>");

/// todo
// early cal at freq before tcxo enable?
//calibrate_image(906000000);
// txco fails if this is done now


#if defined(BOARD_WIO_TRACK_1110_DEV)
// set tcxo mode
// tune = 2, 1.8V - wio tracker dev
// time 983 = 0x3d7
////uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x3, 0xd7};
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x3, 0xd7};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)  - 1.8V
uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x0, 0xa4};
#else // BOARD_SENSECAP_TRACKER_T1000E 
// tune = 0 , 1.6V tracker t1000-e
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x3, 0xd7};
// test with 0xa4 delay
uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};
// test with longer than 0xa4 delay, 1.8v
//uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x3, 0xd7};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)  - 1.8V
//uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x0, 0xa4};
#endif

cmdTransfer("tcxo", cmd4);


#if 0
// skip, has TCXO

// Config LF clk
// ConfigLfClock
// external DIO11
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b10};
// LF crystal, wait
uint8_t cmd8[6] = {3,0,0x1,0x16, 0b101};
// LF RC, no wait
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b0};
cmdTransfer("ConfigLfClock", cmd8);
#endif

// SetRegMode
// enable DC-DC for TX and when hi accuracy is needed
//uint8_t cmd7[6] = {3,0,0x1,0x10, 1};
// disable DC-DC - LDO only
uint8_t cmd7[6] = {3,0,0x1,0x10, 0};
cmdTransfer("setRegMode", cmd7);

#if 0
// SetLfClk
// use ext 32k crystal, wait and rel busy
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b110};
// RC osc, wait for 32k (defs)
uint8_t cmd8[6] = {3,0,0x1,0x16, 0b0};
cmdTransfer("LfClk", cmd8);
#endif


// SetDioAsRfSwitch
//C:\Users\cobra\AppData\Local\Arduino15\packages\Seeeduino\hardware\nrf52\1.1.8\libraries\LBM_WM1110\src\internal\lbm_hal\ral_lr11xx_bsp.c
// ral_lr11xx_bsp_get_rf_switch_cfg()
//uint8_t RfSwEn = 0b1111;  // enable  DIO10 - DIO9 - DIO8 - DIO7 - DIO6 - DIO5
// enable upper bits?
//uint8_t RfSwEn = 0b111111;  // enable  DIO10 - DIO9 - DIO8 - DIO7 - DIO6 - DIO5
// Docs don't list 9, we don't use 10
uint8_t RfSwEn = 0b01111;  // enable  DIO10 - DIO8 - DIO7 - DIO6 - DIO5
// standby none

// wio tracker dev
#if defined(BOARD_WIO_TRACK_1110_DEV)
uint8_t rx_enable = 0b1;  // switch 0
uint8_t tx_enable = 0b11;  // switch 0 and 1
uint8_t tx_hp_enable = 0b10;  // switch 1
#else // BOARD_SENSECAP_TRACKER_T1000E 
uint8_t rx_enable = 0b1001; 
uint8_t tx_enable = 0b1011;  
uint8_t tx_hp_enable = 0b1010;  
#endif
#if 0
uint8_t gnss_enable = 0b100;  // switch 2
uint8_t wifi_enable = 0b1000;  // switch 3
#else
uint8_t gnss_enable = 0b0;  // switch 2 - internal GNSS
uint8_t wifi_enable = 0b0;  // switch 3 - Wifi scanner
#endif

// setdioasrfswitch
uint8_t cmd5[12] = {10,0,0x1,0x12, 
  RfSwEn, 
  0, // none - standby
  rx_enable,
  tx_enable,
  tx_hp_enable,
  0,  // unused
  gnss_enable,
  wifi_enable,
  };

cmdTransfer("dio sw cfg", cmd5);

#if 0
//Move these up

// Config LF clk
// ConfigLfClock
// external DIO11
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b10};
// LF crystal, wait
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b101};
// LF RC, no wait
uint8_t cmd8[6] = {3,0,0x1,0x16, 0b0};
cmdTransfer("ConfigLfClock", cmd8);


// SetRegMode
// enable DC-DC for TX and when hi accuracy is needed
//uint8_t cmd7[6] = {3,0,0x1,0x10, 1};
// no DC-DC converter enable.
uint8_t cmd7[6] = {3,0,0x1,0x10, 0};
cmdTransfer("setRegMode", cmd7);
#endif

// ConfigLfClock XXX not used with TCXO XXXX
// use crystal, wait for ready
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b101};
// user ext 32k crystal, wait and rel busy
//uint8_t cmd8[6] = {3,0,0x1,0x16, 0b110};
//cmdTransfer("LfClk", cmd8);

#if 0
#if defined(BOARD_WIO_TRACK_1110_DEV)
// set tcxo mode
// tune = 2, 1.8V - wio tracker dev
// time 983 = 0x3d7
////uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x3, 0xd7};
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x3, 0xd7};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)  - 1.8V
uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x0, 0xa4};
#else // BOARD_SENSECAP_TRACKER_T1000E 
// tune = 0 , 1.6V tracker t1000-e
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x3, 0xd7};
// test with 0xa4 delay
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)
//uint8_t cmd4[10] = {6,0,0x1,0x17,0, 0, 0x0, 0xa4};
// shorter 5000uS (RadioLib default) 0xa4  (5000 / 30.52)  - 1.8V
uint8_t cmd4[10] = {6,0,0x1,0x17,2, 0, 0x0, 0xa4};
#endif

cmdTransfer("tcxo", cmd4);
#endif

#if 0
/// moved to before modulation params

//setpackettype
uint8_t cmd[10] = {3,0,0x2,0xe,2};

cmdTransfer("packettype lora", cmd);

// geterrors
uint8_t cmd3[10] = {2,3, 1,0xd};

cmdTransfer("errors?", cmd3);

#endif

// SyncWord / Private network (should be 0x12)
//uint8_t cmd6[5] = {3,0, 0x2,0x8, 0x0};
// public - should be 0x34
//uint8_t cmd6[5] = {3,0, 0x2,0x8, 0x1};
//cmdTransfer("PrvtNet", cmd6);
// SetLoRaSyncWord
//uint8_t cmd6[5] = {3,0, 0x2,0x2b, 0x14};
uint8_t cmd6[5] = {3,0, 0x2,0x2b, 0x12};
cmdTransfer("SyncWord", cmd6);

//todo
// not set in radiolib or meshtastic ???  try without
// SetRssiCalibration
// EVK defaults 600MHz - 2GHz
uint8_t cmd9[16] = {13,0, 0x2,0x29, 0x22, 0x32, 0x43, 0x45, 0x64,
    0x55, 0x66, 0x76, 0x6, 0x0, 0x0 };
cmdTransfer("SetRssiCal", cmd9);

#if 0
// setmodulationparams

//uint8_t cmd2[10] = {6,0, 0x02, 0x0f, 0x07 /*SF*/, 0x05 /*BW*/, 0x04 /*CR*/, 0 /*LDR*/};
uint8_t cmd2[10] = {6,0, 0x02, 0x0f, _sf /*SF*/, _bw /*BW*/, _cr /*CR*/, _ldro /*LDR*/};
cmdTransfer("modparams", cmd2);
#endif


// geterrors
//uint8_t cmd3[10] = {2,3, 1,0xd};

//cmdTransfer("errors", cmd3);


// setpacketparams

// setPAconfig

// setTXParams

}


// not valid - lr11xx
uint8_t ISR_VECT lr11xx::readRegister(uint16_t address)
{
  return singleTransfer(OP_READ_REGISTER_6X, address, 0x00);
}
// not valid - lr11xx
void lr11xx::writeRegister(uint16_t address, uint8_t value)
{
    singleTransfer(OP_WRITE_REGISTER_6X, address, value);
}
// not valid - lr11xx
uint8_t ISR_VECT lr11xx::singleTransfer(uint8_t opcode, uint16_t address, uint8_t value)
{
    waitOnBusy();

    uint8_t response;

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer((address & 0xFF00) >> 8);
    SPI.transfer(address & 0x00FF);
    if (opcode == OP_READ_REGISTER_6X) {
        SPI.transfer(0x00);
    }
    response = SPI.transfer(value);
    SPI.endTransaction();

    digitalWrite(_ss, HIGH);

    return response;
}



//int lr11xx::decode_stat(uint8_t stat, uint8_t print_it)
int lr11xx::decode_stat(uint8_t stat)
{
  int doErr = 0;

  Serial.print((stat & 1) ? "  Int - " : "  ___ - ");
  switch ((stat>>1) & 7)
  {
    case 0:
      Serial.println("CMD_FAIL");
      doErr = 1;
      break;
    case 1:
      Serial.println("CMD_PERR");
      break;
    case 2:
      Serial.println("CMD_OK");
      break;
    case 3:
      Serial.println("CMD_DAT");
      break;
  }
//  Serial.print(" - Int ");
//  Serial.println(stat & 1);

#if 0
  if(doErr) {
    Serial.println("get errors");

    uint8_t cmd2[8] = {5,0, 0x01, 0x0d, 0x0, 0x0,0x0};
    cmdTransfer("modparams", cmd2);
    Serial.print("Fail errors ");
    Serial.print(cmd2[5] & 0x1);
    Serial.println(cmd2[6]);

  }
#endif
  return doErr;
}

int lr11xx::decode_stat(const char *prt,uint8_t stat1,uint8_t stat2,uint8_t print_it)
{
  int doErr = 0;

  //if (!print_it) return 0;
  

//  Serial.print((stat & 1) ? "  Int - " : "  ___ - ");
  switch ((stat1>>1) & 7)
  {
    case 0:
      Serial.print(prt);
      Serial.println("        ---- > CMD_FAIL");
      doErr = 1;
      break;
    case 1:
      Serial.println("        ---- > CMD_PERR");
      break;
    case 2:
      if(print_it) Serial.println("CMD_OK");
      break;
    case 3:
      if(print_it) Serial.println("        ---- > CMD_DAT");
      break;
  }

  return doErr;

}


int lr11xx::decode_stat(uint8_t stat, uint8_t stat2)
{
  //int cmd_fail = decode_stat(stat, 1);
  int cmd_fail = decode_stat(stat);
  switch ((stat2 & 0xf0)>>4)
  {
    case 0:
      Serial.print("RST CLR");
      break;
    case 1:
      Serial.print("RST A-Anlg");
      break;
    case 2:
      Serial.print("RST A-Rpin");
      break;
    case 3:
      Serial.print("RST A-Sys");
      break;
    case 4:
      Serial.print("RST A-WDog");
      break;
    case 5:
      Serial.print("RST A-IOCD rstrt");
      break;
    case 6:
      Serial.print("RST A-RTC");
      break;
    default:
      Serial.print("RST ACT");
      break;
  }
  switch ((stat2>>1) & 7)
  {
    case 0:
      Serial.print("  Sleep");
      break;
    case 1:
      Serial.print("  STD RC");
      break;
    case 2:
      Serial.print("  STD Xtc");
      break;
    case 3:
      Serial.print("  FS");
      break;
    case 4:
      Serial.print("  RX");
      break;
    case 5:
      Serial.print("  TX");
      break;
    case 6:
      Serial.print("  RDO");
      break;
  }
  switch (stat2 & 1)
  {
    case 0:
      Serial.println("  bload");
      break;
    case 1:
      Serial.println("  flash");
      break;
  }
  return cmd_fail;
}


uint16_t ISR_VECT lr11xx::cmdTransfer(const char *prt, uint8_t *cmd)
{
  return cmdTransfer(prt, cmd, 0);
}


uint16_t ISR_VECT lr11xx::cmdTransfer(const char *prt, uint8_t *cmd, bool print_it)
{
  uint8_t cnt = cmd[0];
  uint8_t cnt_in = cmd[1];
  uint8_t response;
  uint8_t stat1,stat2;
  uint8_t cmd_fail;

  //print_it=0;

  if(print_it) {
    Serial.print("go cmd -");
    Serial.println(prt);
  }

    waitOnBusy(0);
    SPI.beginTransaction(_spiSettings);
    digitalWrite(_ss, LOW);

  for (int x=0; x<cnt; x++)
  {
    response = SPI.transfer(cmd[2+x]);
    if (print_it) Serial.println(response);
    cmd[2+x] = response;
    if(!x)
      stat1=response;
    // decode stat1, stat2
    if(x==1)
      if (print_it) cmd_fail = decode_stat(stat1,response);
  }
  if(debug_print_enabled) {
    Serial.print(prt);
    Serial.print("- ");
    decode_stat(prt,stat1,response,1);
  }
  decode_stat(prt,stat1,response,print_it);

  // command out complete
  digitalWrite(_ss, HIGH);

  // if response, wait for busy low, then drop _cs again
  if(cnt_in>0) {
    waitOnBusy(0);
    digitalWrite(_ss, LOW);

    for (int x=0; x<cnt_in; x++)
    {
      response = SPI.transfer(0);
      if (print_it) {Serial.print(response); Serial.print(" "); }
      cmd[2+cnt+x] = response;
    }
    digitalWrite(_ss, HIGH);
  }
  
  SPI.endTransaction();
    if (print_it) Serial.println(" done\n----");

  readOpState = (cmd[3]>>1) & 7;

// Debug - states
#if 0
  switch ((cmd[3]>>1) & 7)
  {
    case 0:
      //Serial.print("  Sleep");
      break;
    case 1:
      //Serial.print("  STD RC");
      break;
    case 2:
      //Serial.print("  STD Xtc");
      break;
    case 3:
      //Serial.print("  FS");
      break;
    case 4:
      //Serial.print("  RX");
      break;
    case 5:
      //Serial.print("  TX");
      break;
    case 6:
      //Serial.print("  RDO");
      break;
  }
#endif

  return 0;
}

uint16_t ISR_VECT lr11xx::A32Transfer(uint16_t opcode, uint16_t address, uint16_t value)
{
    waitOnBusy();

    uint8_t response;
    uint16_t response_msb;
    uint16_t response_lsb;

    digitalWrite(_ss, LOW);

    Serial.println("go");
    SPI.beginTransaction(_spiSettings);
    Serial.println(SPI.transfer((opcode & 0xFF00) >> 8));
    Serial.println(SPI.transfer(opcode & 0x00FF));
    //SPI.transfer((address & 0xFF00) >> 8);
    //SPI.transfer(address & 0x00FF);
    //if (opcode == OP_READ_REGISTER_6X) {
    //    SPI.transfer(0x00);
    //}
    ////SPI.transfer((value & 0xFF00) >> 8);
    //SPI.transfer(value & 0x00FF);
    //response_msb = SPI.transfer(value);
    //response_lsb = SPI.transfer(value);
    response = SPI.transfer(0);
    Serial.println(response);
    if (response & 0x4) {
    Serial.println(SPI.transfer(0));
    Serial.println(SPI.transfer(0));
    Serial.println(SPI.transfer(0));
    Serial.println(SPI.transfer(0));
    }
    SPI.endTransaction();
    Serial.println("end");

    digitalWrite(_ss, HIGH);

    return (response_msb << 8) | response_lsb;
}

void lr11xx::rxAntEnable()
{
  if (_rxen != -1) {
    digitalWrite(_rxen, HIGH);
  }
}

void lr11xx::loraMode() {

  //setpackettype
  uint8_t cmd[10] = {3,0,0x2,0xe,2};

//dododo
  cmdTransfer("packettype lora", cmd);

#if 0
  Serial.print("loramode st ");
  Serial.print(cmd[2]);
  Serial.print(" ");
  Serial.print(cmd[3]);
  Serial.print(" ");
  Serial.println(cmd[4]);
#endif

  // test mode
  //uint8_t buf2[10] = {2,2, 0x02, 0x02 };

  //cmdTransfer("packettype", buf2);
  //Serial.print("LM stat1 ");
  //decode_stat(buf2[2], buf2[3]);
#if 0
  Serial.print(buf2[2]);
  Serial.print(" stat2 ");
  Serial.print(buf2[3]);
  Serial.print(" stat rt ");
  Serial.print(buf2[4]);
  Serial.print(" lora2? ");
  Serial.println(buf2[5]);
#endif

  //getErrors2();


  #if 0
    // enable lora mode on the SX1262 chip
    uint8_t mode = MODE_LONG_RANGE_MODE_6X;
    executeOpcode(OP_PACKET_TYPE_6X, &mode, 1);
  #endif
}

void lr11xx::waitOnBusy(uint8_t doYield) {
    unsigned long time = millis();
    unsigned long time2 = time;
    long count = 0;
    if (_busy != -1) {
        while (digitalRead(_busy) == HIGH)
        {
          count++;
          time2 = millis(); 
            if (millis() >= (time + 300)) {
              Serial.println("waitonbusy timeout!!");
                break;
            }
            // do nothing
            if(doYield) yield();
        }
        //Serial.print("busy was = ");
        //Serial.println(time2 - time);
        //Serial.println(count);
    }
}

void lr11xx::executeOpcode(uint8_t opcode, uint8_t *buffer, uint8_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);

    for (int i = 0; i < size; i++)
    {
        SPI.transfer(buffer[i]);
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

void lr11xx::executeOpcodeRead(uint8_t opcode, uint8_t *buffer, uint8_t size)
{
    waitOnBusy();

    digitalWrite(_ss, LOW);

    SPI.beginTransaction(_spiSettings);
    SPI.transfer(opcode);
    SPI.transfer(0x00);

    for (int i = 0; i < size; i++)
    {
        buffer[i] = SPI.transfer(0x00);
    }

    SPI.endTransaction();

    digitalWrite(_ss, HIGH);
}

// lr1110
void lr11xx::writeBuffer(const uint8_t* buffer, size_t size)
{

  uint8_t cmd[4] = {2,0,0x1, 0x9 };
  uint8_t response, stat1;


  waitOnBusy(0);
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);

  for (int x=0; x<2; x++)
  {
    response = SPI.transfer(cmd[2+x]);
    if(!x)
      stat1 = response;
  }
  // write data (TX)
  for (int x=0; x<size; x++)
  {
    response = SPI.transfer(buffer[x]);
    if(x<6 || x>size-3) {
    }
  }

  SPI.endTransaction();
  digitalWrite(_ss, HIGH);
}

// lr1110
void lr11xx::readBuffer(uint8_t* buffer, size_t size)
{
  uint8_t cmd[7] = {4,0,0x1, 0xa, _fifo_rx_addr_ptr, size};
  uint8_t cnt = cmd[0];
  uint8_t response, stat1, stat2, stat3;

  waitOnBusy(0);
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);

  for (int x=0; x<cnt; x++)
  {
    response = SPI.transfer(cmd[2+x]);
  }
  digitalWrite(_ss, HIGH);
  SPI.endTransaction();

  waitOnBusy();
  digitalWrite(_ss, LOW);
  SPI.beginTransaction(_spiSettings);
    
  response = SPI.transfer(0);
  for (int x=0; x<size; x++)
  {
    response = SPI.transfer(0);
    buffer[x] = response;
  }
    SPI.endTransaction();
    digitalWrite(_ss, HIGH);
    _local_rx_buffer = size;

}

// lr1110
void lr11xx::setModulationParams(uint8_t sf, uint8_t bw, uint8_t cr, int ldro) {

if(modulationDirty==0) return;
modulationDirty = 0;

#if 0
  uint8_t buf[8];


  buf[0] = sf;
  buf[1] = bw;
  buf[2] = cr; 
  // low data rate toggle
  buf[3] = ldro;
  // unused params in LoRa mode
  buf[4] = 0x00; 
  buf[5] = 0x00;
  buf[6] = 0x00;
  buf[7] = 0x00;
#endif

  uint8_t cmd2[10] = {6,0, 0x02, 0x0f, sf /*SF*/, bw /*BW*/, cr /*CR*/, ldro /*LDR*/};
  #if 0
  Serial.print("sf ");
  Serial.print(cmd2[4]);
  Serial.print(" bw ");
  Serial.print(cmd2[5]);
  Serial.print(" cr ");
  Serial.print(cmd2[6]);
  Serial.print(" ldro ");
  Serial.println(cmd2[7]);
  #endif

  cmdTransfer("modparams", cmd2);
}

void lr11xx::getErrors(void)
{
  // geterrors
  uint8_t cmd3[10] = {2,3, 1,0xd};
  cmdTransfer("err cmd", cmd3);

  uint8_t cmd2[5] = {2,0, 1,0xe};
  cmdTransfer("err clr", cmd2);

}

// lr1110 done
void lr11xx::setPacketParams(long preamble, uint8_t headermode, uint8_t length, uint8_t crc) {

  if(packetParamsDirty==0) return;
  packetParamsDirty=0;

  // packet params
  uint8_t buf[10] = {8,0, 0x02, 0x10 };

  buf[4] = uint8_t((preamble & 0xFF00) >> 8);
  buf[5] = uint8_t((preamble & 0x00FF));
  buf[6] = headermode;
  buf[7] = length;
  buf[8] = crc;
  // standard IQ setting (no inversion)
  buf[9] = 0x00; 

#if 0
  Serial.print("Packet params  - preamble ");
  Serial.print(preamble);
  Serial.print(" headermode ");
  Serial.print(headermode);
  Serial.print(" len ");
  Serial.print(length);
  Serial.print(" crc  ");
  Serial.println(crc);
#endif

  cmdTransfer("packet params", buf);
}

void lr11xx::reset(void) {

  if (_reset != -1) {
    pinMode(_reset, OUTPUT);

    // perform reset
    digitalWrite(_reset, LOW);
    delay(2);
    digitalWrite(_reset, HIGH);
    delay(2);

    waitOnBusy(0);

#if 0
    // Reboot / Restart
    uint8_t cmd3[9] = {3,0,0x1,0x18,0x0};

    cmdTransfer("->getStat reboot", cmd3, 1);
#endif

    /// Clear reset status
    // GetStatus, last 4 are irq status on return
    uint8_t cmd[9] = {6,0,0x1,0x0, 0x0,0x0,0x0,0x0};

    cmdTransfer("->getStat clr rst", cmd, 1);

    uint8_t cmd2[9] = {6,0,0x1,0x0, 0x0,0x0,0x0,0x0};

    cmdTransfer("->getStat reset2", cmd2, 1);

    }

    // setup pins
    // Not needed for current firmware,
    // but supports T1000-E sensor power
    #if defined(BOARD_SENSECAP_TRACKER_T1000E)
      //pinMode(pin_3v3_en_sensor, OUTPUT);
      //digitalWrite(pin_3v3_en_sensor, HIGH);
    #endif

}

// lr1110
void lr11xx::calibrate(uint8_t cal) {

  // all
  //uint8_t buf[7] = {3,0, 0x01, 0xf, 0b111111 };
  //uint8_t buf[7] = {3,0, 0x01, 0xf, 0b1010 };

  //uint8_t buf[7] = {3,0, 0x01, 0xf, 0b100111 };
  //uint8_t buf[7] = {3,0, 0x01, 0xf, 0xff };
  uint8_t buf[7] = {3,0, 0x01, 0xf, cal };

  cmdTransfer("calib", buf);

}

// lr1110
void lr11xx::calibrate_image(long frequency) {
  uint8_t image_freq[2] = {0};
  uint8_t freq_error = 0;

  
  // lr1110
  if (frequency >= 430E6 && frequency <= 440E6) {
    image_freq[0] = 0x6B;
    image_freq[1] = 0x6E;
  }
  else if (frequency >= 470E6 && frequency <= 510E6) {
    image_freq[0] = 0x75;
    image_freq[1] = 0x81;
  }
  else if (frequency >= 779E6 && frequency <= 787E6) {
    image_freq[0] = 0xC1;
    image_freq[1] = 0xC5;
  }
  else if (frequency >= 863E6 && frequency <= 870E6) {
    image_freq[0] = 0xD7;
    image_freq[1] = 0xDB;
  }
  else if (frequency >= 902E6 && frequency <= 928E6) {
    image_freq[0] = 0xE1;
    image_freq[1] = 0xE9;
  } else {
    freq_error = 1;
  }

  if(!freq_error) {
    uint8_t buf[7] = {4,0, 0x01, 0x11, image_freq[0], image_freq[1] };

    cmdTransfer("calib img", buf);

    Serial.print("calib img - freq ");
    Serial.print(frequency);
    Serial.print(" ");
    Serial.print(image_freq[0]);
    Serial.print(" ");
    Serial.println(image_freq[1]);
  } else {
    Serial.println("calib img - FREQ ERROR  EEEEE");

  }
}

void lr11xx::getErrors2(void)
{

    Serial.println("get errors2");
    uint8_t cmd2[8] = {5,0, 0x01, 0x0d, 0x0, 0x0,0x0};
    cmdTransfer("getErr", cmd2);
    Serial.print("Fail errors ");
    Serial.print(cmd2[5]);
    Serial.print(" (");
    Serial.print(cmd2[5] & 0x1);
    Serial.print("), ");
    Serial.println(cmd2[6]);

}



int lr11xx::begin(long frequency)
{

Serial.println("\n Lora begin lr1110\n - - RESET - -  XXXXXXXXXXXXXXXXXXXXXXX");
  
  if (_busy != -1) {
      pinMode(_busy, INPUT);
  }

  reset();

#if 0
  if (_busy != -1) {
      pinMode(_busy, INPUT);
  }
#endif

/// reset clears chip, redo preInit?
// No, resets bluetooth?
#if 0
    if (!preInit()) {
      return false;
    }
#else 
  if (!_preinit_done) {
    Serial.println("lr1110 preinit ");
    if (!preInit()) {
      return false;
    }
  }
#endif

  if (_rxen != -1) {
      pinMode(_rxen, OUTPUT);
  }

  //calibrate(0b1101);
  //calibrate(0b111111);

  doSetup();

  //Serial.println("lr1110 en tcxo ");
  //enableTCXO();

  // todo 
  //loraMode();
  //standby();

// todo
  // Set sync word
  //setSyncWord(SYNC_WORD_6X);

// No for LR1110?
//  #if DIO2_AS_RF_SWITCH
//    // enable dio2 rf switch
//    uint8_t byte = 0x01;
//    executeOpcode(OP_DIO2_RF_CTRL_6X, &byte, 1);
//  #endif

  Serial.print(" .rxAnt. ");
  rxAntEnable();

  Serial.print(" .freq. ");
  setFrequency(frequency);

  getErrors2();

  /// todo - startup calibrates at 915MHz
  /// if freq is changed, need to CalibImage
  /// if temp change, need to calibrate
///  Serial.println("lr1110 calibrate img");
///  calibrate_image(frequency);
  Serial.println("lr1110 calibrate");
  //calibrate(0b100010);
  calibrate(0b111111);

// todo - testing after calibrate
  calibrate_image(frequency);

  getErrors2();

  //setTxPower(2);
  ////setTxPower(6);

  Serial.print(" .CRC. ");
  enableCrc();

  // todo
  // set LNA boost
  //writeRegister(REG_LNA_6X, 0x96);

  // todo
  // set base addresses
  //uint8_t basebuf[2] = {0};
  //executeOpcode(OP_BUFFER_BASE_ADDR_6X, basebuf, 2);

  //setpackettype
  uint8_t cmd[10] = {3,0,0x2,0xe,2};

  cmdTransfer("packettype lora", cmd);


  setModulationParams(_sf, _bw, _cr, _ldro);
  setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

Serial.println(".... done ");

  return 1;
}

void lr11xx::end()
{
  // put in sleep mode
  sleep();

  // stop SPI
  SPI.end();

  _preinit_done = false;
}

// lr1110
// for TX
int lr11xx::beginPacket(int implicitHeader)
{
  standby();

  loraMode();

  if (implicitHeader) {
    implicitHeaderMode();
  } else {
    explicitHeaderMode();
  }

  _payloadLength = 0;
  _packetIndex = 0;
  packetParamsDirty=1;
  _fifo_tx_addr_ptr = 0;
  // set in endPacket()
  //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

      modulationDirty=1;
      setModulationParams(_sf, _bw, _cr, _ldro);
      // length not known yet...
      //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

  return 1;
}

uint8_t _wait_tx_cmpl;

// lr1110
// for TX
int lr11xx::endPacket()
{

    _payloadLength = _packetIndex;
    packetParamsDirty=1;

// both in beginPacket
//      setModulationParams(_sf, _bw, _cr, _ldro);
      setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);

    writeBuffer(_packet, _payloadLength);

    // put in single TX mode
    // SetTX
    uint8_t cmd2[8] = {5,0,0x2,0xa,0x0,0x0,0x0};
    TXCompl_flag=0;

    cmdTransfer("->tx", cmd2);
    _wait_tx_cmpl = 1;


      // Rely on INT call setting this (wait)
      while (!TXCompl_flag) {
        yield();
      }
      _db_intcnt--;
      checkTXstat();
      TXCompl_flag=0;

  return 1;
}



void lr11xx::checkTXstat()
{
  uint8_t dat[3], done=0;
  //dodo
  while(!done) {
    dioStat(0,&(dat[0]),&(dat[1]),&(dat[2]));
    if(dat[2] & 0b100) {
      // found TXDone
      done=1;
      uint8_t clr[9] = {6,0,0x1,0x14,0x0,0x0,0x0,0b100};
      cmdTransfer("->clear irqs", clr, 0);
    }
    yield();

  }
}


// lr1110 
// RNode main firmware expects the following status bits
	// Status flags
//	const uint8_t SIG_DETECT = 0x01;
//	const uint8_t SIG_SYNCED = 0x02;
//	const uint8_t RX_ONGOING = 0x04;
uint8_t lr11xx::modemStatus() {

    uint8_t byte = 0x00;
#if 0  //debug rx
    // GetStatus, last 4 are irq status on return
    uint8_t cmd[9] = {6,0,0x1,0x0, 0x0,0x0,0x0,0x0};
    uint8_t toClr = 0x00;
    int debug = 0;

    cmdTransfer("->Modem-IrqStat", cmd, 0);
    // Preamble detected
    if(cmd[7] & 0b10000) {
      byte = byte | 0x01 | 0x04;
      toClr = 0b10000;
      Serial.print("Prembl ");
      // debug rx
      debug=1;
    }
    // Header Valid
    if(cmd[7] & 0b100000) {
      byte = byte | 0x02 | 0x04;
      toClr |= 0b100000;
      Serial.print("HdrVl ");
      debug=1;
    }

    // clear active IRQs
    //uint8_t cmd2[9] = {6,0,0x1,0x14, 0x0,0x0,0x0,0b110000};
    if(toClr) {
      uint8_t cmd2[9] = {6,0,0x1,0x14, 0x0,0x0,0x0,toClr};
      cmdTransfer("->Clr Act IRQs", cmd2);
      Serial.print("Clr ");
      debug=1;
    } else {
      //Serial.println("Clr Irq -NA");
    }

    if(debug)
      Serial.println(".");

    #if 0
      if(cmd[7]) {
      Serial.print("modemstat cmd-7 ");
      Serial.println(cmd[7]);
      }
      if(byte>0) {
      Serial.print("modemstat ");
      Serial.println(byte);
      }
    #endif

#endif
    return byte; 

#if 0
    Serial.println("XXXX\n XXXXX modemStatus\n -----------------------");
    // imitate the register status from the sx1276 / 78
    uint8_t buf[2] = {0};

    executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
    uint8_t clearbuf[2] = {0};
    uint8_t byte = 0x00;

    if ((buf[1] & IRQ_PREAMBLE_DET_MASK_6X) != 0) {
      byte = byte | 0x01 | 0x04;
      // clear register after reading
      clearbuf[1] = IRQ_PREAMBLE_DET_MASK_6X;
    }

    if ((buf[1] & IRQ_HEADER_DET_MASK_6X) != 0) {
      byte = byte | 0x02 | 0x04;
    }

    executeOpcode(OP_CLEAR_IRQ_STATUS_6X, clearbuf, 2);

    return byte; 
#endif
}

unsigned long preamble_detected_at = 0;
extern long lora_preamble_time_ms;
extern long lora_header_time_ms;
bool false_preamble_detected = false;

bool lr11xx::dcd() {
  uint8_t clr[9] = {6,0,0x1,0x14,0, 0, 0, 0};
  uint8_t stat1 = 0;
  dioStat(0,0,0, &stat1);

  //uint8_t buf[2] = {0}; executeOpcodeRead(OP_GET_IRQ_STATUS_6X, buf, 2);
  uint32_t now = millis();

  bool header_detected = false;
  bool carrier_detected = false;

//  if ((buf[1] & IRQ_HEADER_DET_MASK_6X) != 0) { header_detected = true; carrier_detected = true; }
  if ((stat1 & 0b100000) != 0) { header_detected = true; carrier_detected = true; }
  else { header_detected = false; }

//  if ((buf[1] & IRQ_PREAMBLE_DET_MASK_6X) != 0) {
  if ((stat1 & 0b10000) != 0) {
    carrier_detected = true;
    if (preamble_detected_at == 0) { preamble_detected_at = now; }
    if (now - preamble_detected_at > lora_preamble_time_ms + lora_header_time_ms) {
      preamble_detected_at = 0;
      if (!header_detected) { false_preamble_detected = true; }
      //uint8_t clearbuf[2] = {0};
      //clearbuf[1] = IRQ_PREAMBLE_DET_MASK_6X;
      //executeOpcode(OP_CLEAR_IRQ_STATUS_6X, clearbuf, 2);

      clr[7] = 0b10000;
      cmdTransfer("->clrIrq", clr);
    }
  }

  // TODO: Maybe there's a way of unlatching the RSSI
  // status without re-activating receive mode?
  // TODO - needed for LR1110?  It looks like Semtech
  // defines base RSSI as only valid for the last packet
  // so maybe that is correct.  THere is another RSSI 
  // instantaneous for when you don't want last packet.
  //if (false_preamble_detected) { sx126x_modem.receive(); false_preamble_detected = false; }
  return carrier_detected;
}


// lr1110
uint8_t lr11xx::currentRssiRaw() {
    // GetRssiInst
    uint8_t cmd[7] = {2,2,0x2,0x5, 0x0,0x0};

    cmdTransfer("->Modem-GetRssi", cmd, 0);

#if 0  //debug rx
    if(cmd[5] || cmd[4]) {
      Serial.print(cmd[2]);
      Serial.print(" ");
      Serial.print(cmd[3]);
      Serial.print(" ");
      Serial.print(cmd[4]);
      Serial.print(" ");
      Serial.println(cmd[5]);
    }
#endif
    return cmd[5];
}

// lr1110
int ISR_VECT lr11xx::currentRssi() {

    uint8_t byte;
    byte = currentRssiRaw();
    int rssi = -(int(byte)) / 2;
    return rssi;

#if 0
    uint8_t byte = 0;
    executeOpcodeRead(OP_CURRENT_RSSI_6X, &byte, 1);
    int rssi = -(int(byte)) / 2;
    return rssi;
#endif
}

// unused
uint8_t lr11xx::packetRssiRaw() {
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
    return buf[2];
}

// used
int ISR_VECT lr11xx::packetRssi() {

    uint8_t cmd[7] = {2,4,0x2,0x4};

    cmdTransfer("->Modem-GetPacketStat", cmd, 0);
    // use RssiPkt
    int pkt_rssi = -cmd[5] / 2;
    Serial.print("RssiPkt ");
    Serial.println(pkt_rssi);
    return pkt_rssi;


    #if 0
    // may need more calculations here
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
    int pkt_rssi = -buf[0] / 2;
    return pkt_rssi;
    #endif
}

// lr1110
uint8_t ISR_VECT lr11xx::packetSnrRaw() {
    uint8_t cmd[7] = {2,4,0x2,0x4};

    cmdTransfer("->Modem-GetPacketStat", cmd, 0);
    int8_t snr = (((int8_t)cmd[6]) + 2) >> 2;
    return snr;
}

// unused
float ISR_VECT lr11xx::packetSnr() {
    uint8_t buf[3] = {0};
    executeOpcodeRead(OP_PACKET_STATUS_6X, buf, 3);
    return float(buf[1]) * 0.25;
}

// unused
long lr11xx::packetFrequencyError()
{
    // todo: implement this, no idea how to check it on the sx1262
    const float fError = 0.0;
    return static_cast<long>(fError);
}

size_t lr11xx::write(uint8_t byte)
{
  return write(&byte, sizeof(byte));
}

// TX buffer write
size_t lr11xx::write(const uint8_t *buffer, size_t size)
{
    if ((_payloadLength + size) > MAX_PKT_LENGTH) {
        size = MAX_PKT_LENGTH - _payloadLength;
    }


    for (int x=0; x<size; x++) {
      _packet[_packetIndex++] = buffer[x];
    }
    return size;
}

// lr1110
int ISR_VECT lr11xx::available() {
  uint8_t size, rxbufstart;

  return available(&size, &rxbufstart);
}

// lr1110
int ISR_VECT lr11xx::available(uint8_t *size, uint8_t *rxbufstart)
{
    uint8_t sz_read = 0;
    // Rx buffer status
    uint8_t cmd[8] = {2,3,0x2,0x3,0x0,0x0,0x0};

    cmdTransfer("->rx buf stat", cmd);
    // returns stat1, PayloadLengthRX, RxStartBufferPointer

    // CMD_DAT - length is returned in Stat2
    if ((cmd[2] & 0b110) == 0b110 ) {
      if(size) *size = cmd[3];
      sz_read = cmd[3];

    // CMD_OK - length should be in packetlength field
    } else if (cmd[2] & 0b100 ) {
      if(size) *size = cmd[5];
      sz_read = cmd[5];

    }
  
    if(rxbufstart) *rxbufstart = cmd[6];
    #if 0
      // rx buf size debug
      if (rxbufstart && *rxbufstart) {
        Serial.print("avail rxbufst>> ");
        Serial.println(*rxbufstart);
      }
    #endif
 
#if 0
    Serial.print("rx buf ");
    Serial.print(cmd[2]);
    Serial.print(" ");
    Serial.print(cmd[3]);
    Serial.print(" ");
    Serial.print(cmd[4]);
    Serial.print(" rx avail sz: ");
    Serial.print(cmd[5]);
    Serial.print(" buf strt: ");
    Serial.print(cmd[6]);
    Serial.print(" rtn ");
    Serial.println(sz_read - _packetIndexRX);
    //Serial.print(" stat ");
    //Serial.println(cmd[5]);
#endif

    if(sz_read)
      return sz_read - _packetIndexRX;
    else
      return 0;

}

//  lr1110
int ISR_VECT lr11xx::read()
{
  uint8_t data_left, size, rxbufstart;

  if(_local_rx_buffer == 0) { 
    // TODO? check this in available
    data_left = available(&size, &rxbufstart);
    if (!data_left) {
      return -1;
    }
  }

  // if received new packet
  if (_packetIndexRX == 0) {
      _fifo_rx_addr_ptr = rxbufstart;

      readBuffer(_packetRX, size);
  }

  #if 0  // sx1262
  // if received new packet
  if (_packetIndex == 0) {
      uint8_t rxbuf[2] = {0};
      executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, rxbuf, 2);
      int size = rxbuf[0];
      _fifo_rx_addr_ptr = rxbuf[1];

      readBuffer(_packet, size);
  }
  #endif

  uint8_t byte = _packetRX[_packetIndexRX];
  _packetIndexRX++;
  // did we read last byte?
  if(_packetIndexRX >= _local_rx_buffer) {
    _local_rx_buffer = 0;
  }
  return byte;
}

// lr1110 - not impl - unused
int lr11xx::peek()
{
  Serial.println("XXXX - lr1110 peek called - not implemented - XXXX");
  #if 0
  if (!available()) {
    return -1;
  }

  // if received new packet
  if (_packetIndexRX == 0) {
      uint8_t rxbuf[2] = {0};
      executeOpcodeRead(OP_RX_BUFFER_STATUS_6X, rxbuf, 2);
      int size = rxbuf[0];
      _fifo_rx_addr_ptr = rxbuf[1];

      readBuffer(_packetRX, size);
  }

  uint8_t b = _packetRX[_packetIndexRX];
  return b;
  #endif

  return 0;
}

// lr1110 unused
void lr11xx::flush()
{
}

// lr1110
void lr11xx::onReceive(void(*callback)(int))
{
  _onReceive = callback;

  if (callback) {


    // Clear what we enable next
    uint8_t cmd2[9] = {6,0,0x1,0x14,0, 0, 0, 0b1100};

    cmdTransfer("->clrIrq", cmd2);


    // enable RXDone, PreambleDetected, HeaderValid(Lora), 
    // headerCRCErr 0b111 1000
    // use SetDioIrqParams on lr1110
    // add TXDone, timeout
    // 0xb 1100 0000 - 0000 0100 - 1111 1100

    // DIO9 enabled for RXDone, DIO11 no enables
    // irqenable, irq enable

    // Rx buffer status
    // byte mapped wrong!
    //uint8_t cmd[13] = {10,0,0x1,0x13,0x0,0x0,0x0,0x78, 0x0,0x0,0x0,0x0};
    // all
    //uint8_t cmd[13] = {10,0,0x1,0x13,0x0,0xc0,0x4,0xfc, 0x0,0x0,0x0,0x0};

    // remove 0x 1100 1100 (preamble and syncword irq), upper bytes unchanged
    //uint8_t cmd[13] = {10,0,0x1,0x13,0x0,0xc0,0x4,0xcc, 0x0,0x0,0x0,0x0};

    // simple version, rxdone
//    uint8_t cmd[13] = {10,0,0x1,0x13,0x0,0x00,0x0,0x08, 0x0,0x0,0x0,0x0};
    // simple version, rxdone, txdone
    uint8_t cmd[13] = {10,0,0x1,0x13,0x0,0x00,0x0,0b1100, 0x0,0x0,0x0,0x0};

    cmdTransfer("->dioIrqParam", cmd);

    // clr irqs active when we programmed




    #if 0
    pinMode(_dio0, INPUT);

    // set preamble and header detection irqs, plus dio0 mask
    uint8_t buf[8];

    // set irq masks, enable all
    buf[0] = 0xFF; 
    buf[1] = 0xFF;

    // set dio0 masks
    buf[2] = 0x00;
    buf[3] = IRQ_RX_DONE_MASK_6X; 

    // set dio1 masks
    buf[4] = 0x00; 
    buf[5] = 0x00;

    // set dio2 masks
    buf[6] = 0x00; 
    buf[7] = 0x00;

    executeOpcode(OP_SET_IRQ_FLAGS_6X, buf, 8);
    #endif

#ifdef SPI_HAS_NOTUSINGINTERRUPT
    SPI.usingInterrupt(digitalPinToInterrupt(_dio0));
#endif
    attachInterrupt(digitalPinToInterrupt(_dio0), lr11xx::onDio0Rise, RISING);
  } else {
    detachInterrupt(digitalPinToInterrupt(_dio0));
#ifdef SPI_HAS_NOTUSINGINTERRUPT
    SPI.notUsingInterrupt(digitalPinToInterrupt(_dio0));
#endif
  }
}

// lr1110 
void lr11xx::receive(int size)
{
    loraMode();

    // test
    //modulationDirty=1;
    setModulationParams(_sf, _bw, _cr, _ldro);


    if (size > 0) {
        implicitHeaderMode();

        // tell radio payload length
        _payloadLength = size;
        packetParamsDirty=1;
        setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    } else {
        explicitHeaderMode();
        _payloadLength = size;
        packetParamsDirty=1;
        setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
    }

    if (_rxen != -1) {
        rxAntEnable();
    }

    // continuous mode
    // Rx mode, no timeout  (setRX)
    // stay until command/continuous mode
    uint8_t cmd[8] = {5,0,0x2,0x9,0xff,0xff,0xff};

    cmdTransfer("->rx", cmd, 0);
    debug_print_enabled=0;
}

// lr1110 
void lr11xx::standby()
{
  // TODO

  // SetStandby Xosc mode
  uint8_t cmd[6] = {3,0,0x1,0x1c,0x1};
  // standby RC osc
  //uint8_t cmd[6] = {3,0,0x1,0x1c,0x0};

  //cmdTransfer("->stby xosc", cmd);
  cmdTransfer("->stby RC", cmd);
}


// lr1110 
void lr11xx::sleep()
{
  // sleep mode
  // no retention, no wakeup
  //uint8_t cmd[10] = {7,0,0x1,0x1b,0x0, 0,0,0,0};
  // retention, no wakeup
  uint8_t cmd[10] = {7,0,0x1,0x1b,0x1, 0,0,0,0};

  //cmdTransfer("->sleep", cmd);
  // TODO
}

void lr11xx::enableTCXO() {
  Serial.println("lr1110 enableTCXO() called, but internally enabled on current boards");
  #if 0
  #if HAS_TCXO
    #if BOARD_MODEL == BOARD_RAK4630 || BOARD_MODEL == BOARD_HELTEC_LORA32_V3 || BOARD_MODEL == BOARD_HELTEC_CAPSULE_V3 || BOARD_HELTEC_WIRELESS_PAPER_1_1
      uint8_t buf[4] = {MODE_TCXO_3_3V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_TBEAM
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #elif BOARD_MODEL == BOARD_RNODE_NG_22
      uint8_t buf[4] = {MODE_TCXO_1_8V_6X, 0x00, 0x00, 0xFF};
    #endif
    executeOpcode(OP_DIO3_TCXO_CTRL_6X, buf, 4);
  #endif
  #endif
}

// Once enabled, LR1110 needs a complete reset to disable TCXO
void lr11xx::disableTCXO() { }

// lr1110 only
void lr11xx::setTxPower(int level, int outputPin) {
  if (level > 22) { level = 22; }
  else if (level < -17) { level = -17; }

  _txp = level;

  // PA Config
  uint8_t PaSel, RegPASupply, PaDutyCycle, PaHPSel = 0;

  // HP PA
  if(level>15) {
    PaSel = RegPASupply = 1;
    if(level==22) {
      PaDutyCycle = 4;
      PaHPSel = 7;
    } else if(level>=20) {
      PaDutyCycle = 2;
      PaHPSel = 7;
    } else if(level>=17) {
      // optional 1,5 instead of 4,3
      PaDutyCycle = 4;
      PaHPSel = 3;
    } else if(level>=14) {
      PaDutyCycle = 2;
      PaHPSel = 2;
    }

  // LP PA
  } else {
    PaSel = RegPASupply = 0;
    if(level==15) {
      PaDutyCycle = 7;
      PaHPSel = 0;
    } else if(level>=14) {
      PaDutyCycle = 4;
      PaHPSel = 0;
    } else {
      PaDutyCycle = 0;
      PaHPSel = 0;
    }
  }

  uint8_t cmd3[9] = {6,0,0x02, 0x15, PaSel, RegPASupply,
    PaDutyCycle, PaHPSel};

  cmdTransfer("->pa cfg", cmd3);

  uint8_t rampTime = 0x02;  // 48uS
  uint8_t cmd4[10] = {4,0,0x02, 0x11, level,rampTime};

  cmdTransfer("->tx pow", cmd4);

 
  // currently no low power mode for LR1110 implemented, assuming PA boost
  // TODO
  // RXBoosted - enabled, ~2mA more consumption in RX, better sensitivity
  // enable
  uint8_t cmd5[6] = {3,0,0x02, 0x27, 1};
  // disable
  //uint8_t cmd5[6] = {3,0,0x02, 0x27, 0};
  cmdTransfer("->rx boost", cmd5);

}

uint8_t lr11xx::getTxPower() {
    return _txp;
}

// lr1110 done
void lr11xx::setFrequency(long freq) {
  _frequency = freq;

  Serial.print( "freq - ");
  Serial.println(freq);
  // set freq cmd
  // SetRfFrequency
  uint8_t cmd4[10] = {6,0,0x02,0x0b, 
    (freq >> 24) & 0xFF, 
    (freq >> 16) & 0xFF, 
    (freq >> 8) & 0xFF, 
    freq & 0xFF};

  if( freq < 400000000 || freq > 930000000 )
    Serial.println("freq out of range, not set");
  else
    cmdTransfer("->freq", cmd4);

// todo - notes say fails 'with TCXO fitted' ?
//  calibrate_image(_frequency);

}

uint32_t lr11xx::getFrequency() {
    // we can't read the frequency on the sx1262 / 80
    uint32_t frequency = _frequency;

    return frequency;
}

void lr11xx::setSpreadingFactor(int sf)
{
  if (sf < 5) {
      sf = 5;
  } else if (sf > 12) {
    sf = 12;
  }

  _sf = sf;

  handleLowDataRate();
  modulationDirty=1;
  //setModulationParams(sf, _bw, _cr, _ldro);
}

long lr11xx::getSignalBandwidth()
{
    int bw = _bw;
    switch (bw) {
        case 0x00: return 7.8E3;
        case 0x01: return 15.6E3;
        case 0x02: return 31.25E3;
        case 0x03: return 62.5E3;
        case 0x04: return 125E3;
        case 0x05: return 250E3;
        case 0x06: return 500E3;
        case 0x08: return 10.4E3;
        case 0x09: return 20.8E3;
        case 0x0A: return 41.7E3;
    }
  return 0;
}

void lr11xx::handleLowDataRate(){
  if ( long( (1<<_sf) / (getSignalBandwidth()/1000)) > 16) {
    _ldro = 0x01;
  } else {
    _ldro = 0x00;
  }
}

void lr11xx::optimizeModemSensitivity(){
    // todo: check if there's anything we can do here
}

// lr1110
void lr11xx::setSignalBandwidth(long sbw)
{
  #if 0
  if (sbw <= 7.8E3) {
      _bw = 0x00;
  } else if (sbw <= 10.4E3) {
      _bw = 0x08;
  } else if (sbw <= 15.6E3) {
      _bw = 0x01;
  } else if (sbw <= 20.8E3) {
      _bw = 0x09;
  } else if (sbw <= 31.25E3) {
      _bw = 0x02;
  } else if (sbw <= 41.7E3) {
      _bw = 0x0A;
  } else if (sbw <= 62.5E3) {
      _bw = 0x03;
  } else if (sbw <= 125E3) {
      _bw = 0x04;
  } else if (sbw <= 250E3) {
      _bw = 0x05;
  } else /*if (sbw <= 250E3)*/ {
      _bw = 0x06;
  }
  #endif

  if (sbw <= 62.5E3) {
      _bw = 0x03;
  } else if (sbw <= 125E3) {
      _bw = 0x04;
  } else if (sbw <= 250E3) {
      _bw = 0x05;
  } else /*if (sbw <= 250E3)*/ {
      _bw = 0x06;
  }


  handleLowDataRate();
  modulationDirty=1;
  //setModulationParams(_sf, _bw, _cr, _ldro);

  optimizeModemSensitivity();
}

// lr1110 ok
void lr11xx::setCodingRate4(int denominator)
{
  if (denominator < 5) {
    denominator = 5;
  } else if (denominator > 8) {
    denominator = 8;
  }

#if 1
  int cr = denominator - 4;
#else
  // lr1110 has two  interleavers, test with long interleaver
  int cr = denominator;
#endif

  _cr = cr;

  modulationDirty=1;
  //setModulationParams(_sf, _bw, cr, _ldro);
}

void lr11xx::setPreambleLength(long length)
{
  _preambleLength = length;
  packetParamsDirty=1;
  //setPacketParams(length, _implicitHeaderMode, _payloadLength, _crcMode);
}

void lr11xx::setSyncWord(uint16_t sw)
{
  // lr1110 not used, so far
  // TODO: Fix
    // writeRegister(REG_SYNC_WORD_MSB_6X, (sw & 0xFF00) >> 8);
    // writeRegister(REG_SYNC_WORD_LSB_6X, sw & 0x00FF);
    //writeRegister(REG_SYNC_WORD_MSB_6X, 0x14);
    //writeRegister(REG_SYNC_WORD_LSB_6X, 0x24);
}

void lr11xx::enableCrc()
{
    _crcMode = 1;
    packetParamsDirty=1;
    //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void lr11xx::disableCrc()
{
    _crcMode = 0;
    packetParamsDirty=1;
    //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

// done lr1110
byte lr11xx::random()
{
  // generate byte of random with built in 
  // Random Number Gen (RNG)
  NRF_RNG->EVENTS_VALRDY = 0;
  NRF_RNG->TASKS_START = 1;
  while (NRF_RNG->EVENTS_VALRDY == 0)
  {
    // wait for start
  }
  uint8_t rand1 = NRF_RNG->VALUE;
  NRF_RNG->TASKS_STOP = 1;

  return rand1;
}

void lr11xx::setPins(int ss, int reset, int dio0, int busy)
{
  _ss = ss;
  _reset = reset;
  _dio0 = dio0;
  _busy = busy;
}

void lr11xx::setSPIFrequency(uint32_t frequency)
{
  _spiSettings = SPISettings(frequency, MSBFIRST, SPI_MODE0);
}

void lr11xx::dumpRegisters(Stream& out)
{
  for (int i = 0; i < 128; i++) {
    out.print("0x");
    out.print(i, HEX);
    out.print(": 0x");
    out.println(readRegister(i), HEX);
  }
}

void lr11xx::explicitHeaderMode()
{
  _implicitHeaderMode = 0;
  packetParamsDirty=1;
  //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void lr11xx::implicitHeaderMode()
{
  _implicitHeaderMode = 1;
  packetParamsDirty=1;
  //setPacketParams(_preambleLength, _implicitHeaderMode, _payloadLength, _crcMode);
}

void lr11xx::dioStatInternal(uint8_t *stat1, uint8_t *int2, uint8_t *int3, uint8_t *int4)
{

// NOTE - modified to use ClearIrq with all 0 clears
// to read IRQ status, after issues with GetStatus

// getStatus
//uint8_t cmd[9] = {2,4,0x1,0x0, 0x0,0x0,0x0,0x0};
uint8_t clr[9] = {6,0,0x1,0x14,0x0,0x0,0x0,0x0};
int doClr = 0;

//cmdTransfer("->getStatus irq", cmd);
cmdTransfer("->ClearIrq", clr);

if(stat1) *stat1 = clr[2];
if(int2) *int2 = clr[5];
if(int3) *int3 = clr[6];
if(int4) *int4 = clr[7];
}


void lr11xx::dioStat(uint8_t *stat1, uint8_t *int2,  uint8_t *int3, uint8_t *int4)
{
  uint8_t l_stat1, l_int2, l_int3, l_int4;
  dioStatInternal(&l_stat1, &l_int2, &l_int3, &l_int4);

  // Test for CMD_DAT, then IRQ stat was not sent
  if((l_stat1 & 0b0110) == 6)
  {
    dioStatInternal(&l_stat1, &l_int2, &l_int3, &l_int4);
  }

  if(stat1) *stat1 = l_stat1;
  if(int2) *int2 = l_int2;
  if(int3) *int3 = l_int3;
  if(int4) *int4 = l_int4;

}

uint8_t ISR_VECT lr11xx::dumpRx(void)
{
        uint8_t cmd2[8] = {2,3,0x2,0x3};

        cmdTransfer("->getRxBuf", cmd2);
        int packetLength = cmd2[5];

        Serial.print("stat1.1 ");
        Serial.print(cmd2[2]);
        Serial.print(" stat2 ");
        Serial.print(cmd2[3]);
        Serial.print(" stat1.2 ");
        Serial.print(cmd2[4]);
        Serial.print(" pack len= ");
        Serial.print(packetLength);
        Serial.print(" start= ");
        Serial.println(cmd2[6]);

        uint8_t stat2 = cmd2[5];
        return stat2;
}


void lr11xx::handleIntStatus()
{
    uint8_t cmd[9] = {2,4,0x1,0x0, 0x0,0x0,0x0,0x0};
    uint8_t clr[9] = {6,0,0x1,0x14,5,11,8,0x0};

    int doClr = 0;

  dioStat(&cmd[2], &cmd[5], &cmd[6], &cmd[7]);

  // if RXdone
  if (cmd[7] & 0b1000) {
    clr[7] |= 0b1000;
    doClr = 1;

    // if no CRC header error
    if ((cmd[7] & 0b1000000) == 0) {

      // critical - set foreground rx processing
      _db_rxcnt++;
      // received a packet
      _packetIndexRX = 0;

    } else {
      // clear CRC header err
      clr[7] |= 0b1000000;
      doClr = 1;

      Serial.println("  xxxxxxxx   RXDone - CRC Header Err      EEEEEEEEEEEEEEE");
    }
  }

  if(cmd[7] && 0b10000 )
  {
      clr[7] |= 0b10000;
      doClr = 1;
//        Serial.println("Preamble");
  }
  if(cmd[7] && 0b100000 )
  {
      clr[7] |= 0b100000;
      doClr = 1;
//        Serial.println("Header");
  }

  // ClearIrq
  if (doClr) 
  {
    cmdTransfer("->clear irqs", clr, 0);
  }

}


// lr1110 - todo 2 item - ignores error, packet length
void ISR_VECT lr11xx::handleDio0Rise()
{

  _db_intcnt++;
  if(_wait_tx_cmpl) {
    TXCompl_flag=1;
    _wait_tx_cmpl=0;
  }

// debug rx
#if 0
        // LoRa GetStats
        uint8_t cmd9[15] = {2,9,0x2,0x1};
        cmdTransfer("->getStats", cmd9);

        Serial.print("Lora stats, pkts ");
        Serial.print( (cmd9[5] << 8) | cmd[6] );
        Serial.print(" err pkts, crc ");
        Serial.print( (cmd9[7] << 8) | cmd[8] );
        Serial.print(" hdr ");
        Serial.print( (cmd9[9] << 8) | cmd[10] );
        Serial.print(" fls sync ");
        Serial.print( (cmd9[11] << 8) | cmd[12] );
        Serial.print(" st ");
        Serial.print( cmd9[2] );
        Serial.print(" ");
        Serial.print( cmd9[3] );
        Serial.print(" ");
        Serial.print( cmd9[4] );
#endif

}

void ISR_VECT lr11xx::onDio0Rise()
{
    lr11xx_modem.handleDio0Rise();
}


// Debug for LR1110 State
void lr11xx::checkOpState()
{

  if( lOpState==4 && readOpState==1 ) // RX & STD RC
  {
    Serial.println("rx to std");

  } 
  else if( lOpState==1 && readOpState==4 )
  {
    Serial.println("std to rx");
  }
  else if( lOpState==5 && readOpState==4 )
  {
    Serial.println("tx to rx");
  }
  else if( lOpState==1 && readOpState==5 )
  {
    Serial.println("std to tx");
  }
  else if( readOpState==3 )
  {
    Serial.println("in fs");
  }
  lOpState = readOpState;

}


lr11xx lr11xx_modem;

unsigned long ltime = 0;

// Foreground loop for rx/tx handling
void lr11xx::foreground(void)
{
  unsigned long time = millis();

  // Debug state
  //checkOpState();

  if (_db_intcnt) {
    handleIntStatus();
    _db_intcnt = 0;
  }

  if (_db_rxcnt) {
    _db_rxcnt=0;

    //unsigned long time = millis();

    uint8_t packetLength = 0;
    available(&packetLength, NULL);
    // receive_callback in main firmware, 
    // calls in to our read() for bytes
    if (_onReceive && packetLength) {
      _onReceive(packetLength);
    }

  }

  // Debug, enable for continuous status prints
  #if 0
    if(time-ltime > 5*1000) {
      ltime = time;

      Serial.print("Tm ");
      Serial.print(time/1000);
      print_state();
      // geterrors
      uint8_t cmd3[10] = {2,3, 1,0xd};
      cmdTransfer("errors?", cmd3, 0);

      // Active Int
      if(cmd3[2] & 0x1) {
        dioStat(NULL, NULL, NULL, NULL);
      }
    }
  #endif

};

void lr11xx::print_state()
{
  if( readOpState==1 ) {
    Serial.println(" std");
  } 
  else if ( readOpState==2 ) {
    Serial.println(" std xosc");
  }
  else if ( readOpState==3 ) {
    Serial.println(" FS");
  }
  else if ( readOpState==4 ) {
    Serial.println(" RX");
  }
  else if ( readOpState==5 ) {
    Serial.println(" TX");
  }
}


#endif