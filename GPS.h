// Copyright (C) 2024, Michael Faragher
// Extended from the RNode Firmware. Copyright (C) 2023, Mark Qvist
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

//#include <Boards.h>


char GPS_Buffer[100];
byte GPS_Index;
char GPSGGA[100] = " ";
char GPSGLL[100] = " ";
char GPSVTG[100] = " ";
char GPSRMC[100] = " ";
char GPSTime[6] = {'9','0','0','0','0','0'};
bool isGPS_Automated = false;
bool isGPS_Waiting_Time = false;
bool isGPS_Waiting_Location = false;
bool isGPS_Waiting_NMEA = false;
char GPS_Lat[8] = {'N','O',' ',' ','F','I','X','X'};
char GPS_Lon[9] = {'N','O',' ',' ',' ','F','I','X','X'};

#define GPS_CMD_TIME                0x00
#define GPS_CMD_LOCATION            0x01
#define GPS_CMD_RATE                0x02
#define GPS_CMD_SENTENCE            0x03
#define GPS_CMD_ENABLE_HEARTBEAT    0x10
#define GPS_CMD_DISABLE_HEARTBEAT   0x11
#define GPS_CMD_ENABLE_AUTOMATIC    0x12
#define GPS_CMD_DISABLE_AUTOMATIC   0x13
#define GPS_CMD_HOT                 0x20
#define GPS_CMD_COLD                0x21


byte NMEA_checksum(char message[], byte length){
  byte buffer = 0;
  byte working;
  for(byte i = 0; i < length; i++){
    working = message[int(i)];// Apparently not & 127; //Lower seven bits
    buffer = buffer ^ working; //XORed
  }
  return buffer;
}

void GPS_poling(byte Target, byte Rate){ //Divisor, 0 = off, 1-?, 0-9 in this implementation
  //Targets:
  // 0 GGA
  // 1 GLL
  // 2 GSA
  // 3 GSV
  // 4 RMC
  // 5 VTG
  char cRate[10] = {'0','1','2','3','4','5','6','7','8','9'};
  //char cTarget[6] = {'0','1','2','3','4','5'};
  char cTargetA[6] = {'G','G','G','G','R','V'};
  char cTargetB[6] = {'G','L','S','S','M','T'};
  char cTargetC[6] = {'A','L','A','V','C','G'};
  char message[24] = "PUBX,40,GGA,0,0,0,0,0,0";
  if(Rate>9){Rate=9;}
  if(Rate<0){Rate=0;}
  //itoa(Rate, cRate,10);
  message[14]=cRate[Rate];
  message[8] = cTargetA[int(Target)];
  message[9] = cTargetB[int(Target)];
  message[10] = cTargetC[int(Target)];
  byte checksum = NMEA_checksum(message, 19);
  //char hexlookup[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
  //char checksumA = hexlookup[int(checksum ^ 16)];
  //char checksumB = hexlookup[int((checksum>>4)^16)];

  Serial2.write('$');
  for(int i = 0; i < 19; i++){
    Serial2.write(message[i]);
  }

  Serial2.write('*');
  Serial2.print(checksum,HEX);
  Serial2.write(0x0d);
  Serial2.write(0x0a);

//// Dump outgoing commands to serial console
//  Serial.write('$');
//  for(int i = 0; i < 24; i++){
//    Serial.write(message[i]);
//  }
//
//  Serial.write('*');
//  Serial.print(checksum,HEX);
//  Serial.write(0x0d);
//  Serial.write(0x0a);

}

void GPS_request(byte Target){
  //Targets:
  // 0 GGA
  // 1 GLL
  // 2 GSA
  // 3 GSV
  // 4 RMC
  // 5 VTG
  if(Target>5){return;}
  char cTargetA[6] = {'G','G','G','G','R','V'};
  char cTargetB[6] = {'G','L','S','S','M','T'};
  char cTargetC[6] = {'A','L','A','V','C','G'};
  char message[10] = "EIGPQ,GGA";
  message[6] = cTargetA[int(Target)];
  message[7] = cTargetB[int(Target)];
  message[8] = cTargetC[int(Target)];
  byte checksum = NMEA_checksum(message, 9);

  Serial2.write('$');
  for(int i = 0; i < 9; i++){
    Serial2.write(message[i]);
  }

  Serial2.write('*');
  Serial2.print(checksum,HEX);
  Serial2.write(0x0d);
  Serial2.write(0x0a);

}

void GPS_Query_Time(){
  if(GPSTime[0]!='9'){
    serial_write(FEND);
    serial_write(CMD_GPS_CMD);
    serial_write(GPS_CMD_TIME);
    for(int i = 0; i < 6; i++){
      serial_write(GPSTime[i]);
    }
    serial_write(FEND);
    //debug
    serial_write(char(13));
    serial_write(char(10));
    //eng debug
  }
  else{//if time not buffered
    isGPS_Waiting_Time = true;
  }
}

void GPS_Query_Location(){
    if(GPS_Lon[0]=='0'||GPS_Lon[0]=='1'){
      serial_write(' ');
      serial_write(GPS_Lat[0]);
      serial_write(GPS_Lat[1]);
      serial_write(' ');
      serial_write(GPS_Lat[2]);
      serial_write(GPS_Lat[3]);
      serial_write('.');
      serial_write(GPS_Lat[4]);
      serial_write(GPS_Lat[5]);
      serial_write(GPS_Lat[6]);
      serial_write('\'');
      serial_write(' ');
      serial_write(GPS_Lat[7]);
      serial_write(char(13));
      serial_write(char(10));

      serial_write(GPS_Lon[0]);
      serial_write(GPS_Lon[1]);
      serial_write(GPS_Lon[2]);
      serial_write(' ');
      serial_write(GPS_Lon[3]);
      serial_write(GPS_Lon[4]);
      serial_write('.');
      serial_write(GPS_Lon[5]);
      serial_write(GPS_Lon[6]);
      serial_write(GPS_Lon[7]);
      serial_write('\'');
      serial_write(' ');
      serial_write(GPS_Lon[8]);
      serial_write(char(13));
      serial_write(char(10));
    }
  else{
    isGPS_Waiting_Location = true;
  }
}


void GPS_Parse_Time(){
  if(GPSGGA[8]==','){return;}
  for(int i = 0; i < 6; i++){
    GPSTime[i] = GPSGGA[8+i];
  }
  //debug
  //Serial.print("Pos 8: ");
  //Serial.println(GPSGGA[8]);
  //Serial.println(GPSTime);
}

void GPS_Parse_Location(){
  if(GPSGGA[18]!=','){
    if(GPSGGA[31]=='0'||GPSGGA[31]=='1'){
      GPS_Lat[0] = GPSGGA[18];
      GPS_Lat[1] = GPSGGA[19];
      GPS_Lat[2] = GPSGGA[20];
      GPS_Lat[3] = GPSGGA[21];
      GPS_Lat[4] = GPSGGA[23];
      GPS_Lat[5] = GPSGGA[24];
      GPS_Lat[6] = GPSGGA[25];
      GPS_Lat[7] = GPSGGA[29];

      GPS_Lon[0] = GPSGGA[31];
      GPS_Lon[1] = GPSGGA[32];
      GPS_Lon[2] = GPSGGA[33];
      GPS_Lon[3] = GPSGGA[34];
      GPS_Lon[4] = GPSGGA[35];
      GPS_Lon[5] = GPSGGA[37];
      GPS_Lon[6] = GPSGGA[38];
      GPS_Lon[7] = GPSGGA[39];
      GPS_Lon[8] = GPSGGA[43];
    }
  }

  //debug

//  Serial.print("Locations: ");
//  Serial.print(GPSGGA[18]); //4
//  Serial.print(GPSGGA[19]); //3
//  Serial.print(GPSGGA[20]); //2
//  Serial.print(GPSGGA[21]); //5
//  Serial.print(GPSGGA[22]); //.
//  Serial.print(GPSGGA[23]); //2
//  Serial.print(GPSGGA[24]); //7
//  Serial.print(GPSGGA[25]); //0
//  Serial.print(GPSGGA[26]); //0
//  Serial.print(GPSGGA[27]); //1
//  Serial.println(GPSGGA[29]); //N
//
//  Serial.print(GPSGGA[31]); //0
//  Serial.print(GPSGGA[32]); //8
//  Serial.print(GPSGGA[33]); //8
//  Serial.print(GPSGGA[34]); //1
//  Serial.print(GPSGGA[35]); //2
//  Serial.print(GPSGGA[36]); //.
//  Serial.print(GPSGGA[37]); //2
//  Serial.print(GPSGGA[38]); //5
//  Serial.print(GPSGGA[39]); //0
//  Serial.print(GPSGGA[40]); //2
//  Serial.print(GPSGGA[41]); //2
//  Serial.println(GPSGGA[43]); //W

 

}

void GPS_init(){
    Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    //Serial.println("Serial2 is up");
    GPS_poling(0,0);
    GPS_poling(1,0);
    GPS_poling(2,0);
    GPS_poling(3,0);
    GPS_poling(4,0);
    GPS_poling(5,0);

    //debug
//    isGPS_Automated = true;
//    GPS_poling(0,1);
//    GPS_request(0);
}

void GPS_sweep(){
  char coi[1];
  Serial2.readBytes(coi,1);
  if(coi[0] == char(13)){

    if(GPS_Buffer[4]=='G'&&GPS_Buffer[5]=='G'&&GPS_Buffer[6]=='A'){
      memset(GPSGGA,0xFF,100);
      strncpy(GPSGGA,GPS_Buffer,GPS_Index);
      GPS_Parse_Time();
      GPS_Parse_Location();
      if(isGPS_Waiting_Time){
        isGPS_Waiting_Time = false;
        GPS_Query_Time();
      }
      if(isGPS_Waiting_Location){
        isGPS_Waiting_Location = false;
        GPS_Query_Location();
      }
    }
    if(GPS_Buffer[4]=='G'&&GPS_Buffer[5]=='L'&&GPS_Buffer[6]=='L'){
      memset(GPSGLL,0xFF,100);
      strncpy(GPSGLL,GPS_Buffer,GPS_Index);
    }
    if(GPS_Buffer[4]=='V'&&GPS_Buffer[5]=='T'&&GPS_Buffer[6]=='G'){
      memset(GPSVTG,0xFF,100);
      strncpy(GPSVTG,GPS_Buffer,GPS_Index);
    }
    if(GPS_Buffer[4]=='R'&&GPS_Buffer[5]=='M'&&GPS_Buffer[6]=='C'){
      memset(GPSRMC,0xFF,100);
      strncpy(GPSRMC,GPS_Buffer,GPS_Index);
    }
    GPS_Index = 0;
    memset(GPS_Buffer,0xFF,100);
  }
  else {
    GPS_Buffer[GPS_Index] = coi[0];
    GPS_Index++;
  }

  if(GPS_Index > 98){
    //Serial.println("ERROR");
    //Serial.println("Buffer Overflow: GPS");
    GPS_Index=0;
    memset(GPS_Buffer,0xFF,100);
  }
    
}







void GPS_SetAutomatic(bool state)
{
  isGPS_Automated = state;
}

// Updates all useful buffers at full rate
void GPS_Go_Hot(){
    GPS_poling(0,1);
    GPS_poling(1,1);
    GPS_poling(2,0);
    GPS_poling(3,0);
    GPS_poling(4,1);
    GPS_poling(5,1);
}

// Shuts down automated buffering
void GPS_Go_Cold(){
    GPS_poling(0,0);
    GPS_poling(1,0);
    GPS_poling(2,0);
    GPS_poling(3,0);
    GPS_poling(4,0);
    GPS_poling(5,0);
}

void GPS_Enable_Heartbeat(bool state){
  if(state){
    GPS_poling(0,1);
    GPS_SetAutomatic(true);
  }
  else
  {
    GPS_poling(0,0);
    GPS_SetAutomatic(false);
  }

}

void GPS_send(){
  if(GPSGGA[1]=='$'){
    byte i = 0;
    char coi;
    serial_write(FEND);
    serial_write(CMD_GPS_CMD);
    serial_write(GPS_CMD_SENTENCE);
    while(coi != 0xFF){
      coi = GPSGGA[i];
      if(coi != 0xFF){serial_write(coi);}
      i++;
      if(i>99){break;}
    }
    serial_write(FEND);
//    Serial.println("");
//    GPSGGA[0]=0;
    memset(GPSGGA,0xFF,100);
  }

  if(GPSGLL[1]=='$'){
    byte i = 0;
    char coi;
    serial_write(FEND);
    serial_write(CMD_GPS_CMD);
    serial_write(GPS_CMD_SENTENCE);
    while(coi != 0xFF){
      coi = GPSGLL[i];
      if(coi != 0xFF){serial_write(coi);}
      i++;
      if(i>99){break;}
    }
    serial_write(FEND);
//    GPSGLL[0]=0;
    memset(GPSGLL,0xFF,100);
  }

  if(GPSVTG[1]=='$'){
    byte i = 0;
    char coi;
    serial_write(FEND);
    serial_write(CMD_GPS_CMD);
    serial_write(GPS_CMD_SENTENCE);
    while(coi != 0xFF){
      coi = GPSVTG[i];
      if(coi != 0xFF){serial_write(coi);}
      i++;
      if(i>99){break;}
    } 
    serial_write(FEND);
//    GPSVTG[0]=0;
    memset(GPSVTG,0xFF,100);
  }

  if(GPSRMC[1]=='$'){
    byte i = 0;
    char coi;
    serial_write(FEND);
    serial_write(CMD_GPS_CMD);
    serial_write(GPS_CMD_SENTENCE);
    while(coi != 0xFF){
      coi = GPSRMC[i];
      if(coi != 0xFF){serial_write(coi);}
      i++;
      if(i>99){break;}
    }
    serial_write(FEND);
//    GPSRMC[0]=0;
    memset(GPSRMC,0xFF,100);
  }
}

void kiss_GPS_send(){
   GPS_send();
}

void kiss_GPS_location(){

}

void kiss_GPS_time(){

}

void GPS_Query_NMEA(){
  GPS_send();
}


void GPS_process(){
  GPS_sweep();
  if(isGPS_Automated || isGPS_Waiting_NMEA){
    GPS_send();
  }
}

void GPS_handler(byte CMD){
  if(CMD==GPS_CMD_TIME){
    GPS_Query_Time();
  }
  if(CMD== GPS_CMD_LOCATION){
    GPS_Query_Location();
  }
  if(CMD==GPS_CMD_SENTENCE){
    GPS_Query_NMEA();
  }
  if(CMD==GPS_CMD_ENABLE_HEARTBEAT){
    GPS_Enable_Heartbeat(true);
  }
  if(CMD==GPS_CMD_DISABLE_HEARTBEAT){
    GPS_Enable_Heartbeat(false);
  }
  if(CMD==GPS_CMD_HOT){
    GPS_Go_Hot();
  }
  if(CMD==GPS_CMD_COLD){
    GPS_Go_Cold();
  }
  if(CMD==GPS_CMD_ENABLE_AUTOMATIC){
    GPS_SetAutomatic(true);
  }
  if(CMD==GPS_CMD_DISABLE_AUTOMATIC){
    GPS_SetAutomatic(false);
  }

}