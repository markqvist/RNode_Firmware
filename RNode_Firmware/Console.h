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

#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>

#include "SD.h"
#include "SPI.h"
  
#if HAS_SD
  SPIClass *spi = NULL;
#endif


#if CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/rtc.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/rtc.h"
#else 
#error Target CONFIG_IDF_TARGET is not supported
#endif

WebServer server(80);

void console_dbg(String msg) {
    Serial.print("[Webserver] ");
    Serial.println(msg);
}

bool exists(String path){
  bool yes = false;
  File file = SPIFFS.open(path, "r");
  if(!file.isDirectory()){
    yes = true;
  }
  file.close();
  return yes;
}

String console_get_content_type(String filename) {
  if (server.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  } else if (filename.endsWith(".whl")) {
    return "application/octet-stream";
  }
  return "text/plain";
}

bool console_serve_file(String path) {
  console_dbg("Request for: "+path);
  if (path.endsWith("/")) {
    path += "index.html";
  }

  if (path == "/r/manual/index.html") {
    path = "/m.html";
  }
  if (path == "/r/manual/Reticulum Manual.pdf") {
    path = "/h.html";
  }


  String content_type = console_get_content_type(path);
  String pathWithGz = path + ".gz";
  if (exists(pathWithGz) || exists(path)) {
    if (exists(pathWithGz)) {
      path += ".gz";
    }
    
    File file = SPIFFS.open(path, "r");
    console_dbg("Serving file to client");
    server.streamFile(file, content_type);
    file.close();

    console_dbg("File serving done\n");
    return true;
  } else {
    int spos = pathWithGz.lastIndexOf('/');
    if (spos > 0) {
      String remap_path = "/d";
      remap_path.concat(pathWithGz.substring(spos));
      Serial.println(remap_path);

      if (exists(remap_path)) {
        File file = SPIFFS.open(remap_path, "r");
        console_dbg("Serving remapped file to client");
        server.streamFile(file, content_type);
        console_dbg("Closing file");
        file.close();
        
        console_dbg("File serving done\n");
        return true;
      }
    }
  }

  console_dbg("Error: Could not open file for serving\n");
  return false;
}

void console_register_pages() {
  server.onNotFound([]() {
    if (!console_serve_file(server.uri())) {
      server.send(404, "text/plain", "Not Found");
    }
  });
}

void console_start() {
  Serial.println("");
  console_dbg("Starting Access Point...");
  WiFi.softAP(bt_devname);
  delay(150);
  IPAddress ip(10, 0, 0, 1);
  IPAddress nm(255, 255, 255, 0);
  WiFi.softAPConfig(ip, ip, nm);

  if(!SPIFFS.begin(true)){
    console_dbg("Error: Could not mount SPIFFS");
    return;
  } else {
    console_dbg("SPIFFS Ready");
  }

  #if HAS_SD
    spi = new SPIClass(HSPI);
    spi->begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if(!SD.begin(SD_CS, *spi)){
      console_dbg("No SD card inserted");
    } else {
      uint8_t cardType = SD.cardType();
      if(cardType == CARD_NONE){
        console_dbg("No SD card type");
      } else {
        console_dbg("SD Card Type: ");
        if(cardType == CARD_MMC){
          console_dbg("MMC");
        } else if(cardType == CARD_SD){
          console_dbg("SDSC");
        } else if(cardType == CARD_SDHC){
          console_dbg("SDHC");
        } else {
          console_dbg("UNKNOWN");
        }
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("SD Card Size: %lluMB\n", cardSize);
      }
    }
  #endif

  console_register_pages();
  server.begin();
  led_indicate_console();
}

void console_loop(){
    server.handleClient();
    // Internally, this yields the thread and allows
    // other tasks to run.
    delay(2);
}