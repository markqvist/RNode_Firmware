#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>

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

  String content_type = console_get_content_type(path);
  String pathWithGz = path + ".gz";
  if (exists(pathWithGz) || exists(path)) {
    if (exists(pathWithGz)) {
      path += ".gz";
    }
    
    File file = SPIFFS.open(path, "r");
    console_dbg("Serving file to client");
    server.streamFile(file, content_type);
    console_dbg("Closing file");
    file.close();

    console_dbg("File serving done");
    return true;
  }

  console_dbg("Error: Could not open file for serving");
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

// void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
//     Serial.printf("Listing directory: %s\r\n", dirname);

//     File root = fs.open(dirname);
//     if(!root){
//         Serial.println("- failed to open directory");
//         return;
//     }
//     if(!root.isDirectory()){
//         Serial.println(" - not a directory");
//         return;
//     }

//     File file = root.openNextFile();
//     while(file){
//         if(file.isDirectory()){
//             Serial.print("  DIR : ");
//             Serial.println(file.name());
//             if(levels){
//                 listDir(fs, file.path(), levels -1);
//             }
//         } else {
//             Serial.print("  FILE: ");
//             Serial.print(file.name());
//             Serial.print("\tSIZE: ");
//             Serial.println(file.size());
//         }
//         file = root.openNextFile();
//     }
// }

// void readFile(fs::FS &fs, const char * path){
//    Serial.printf("Reading file: %s\r\n", path);

//    File file = fs.open(path);
//    if(!file || file.isDirectory()){
//        Serial.println("− failed to open file for reading");
//        return;
//    }

//    Serial.println("− read from file:");
//    while(file.available()){
//       Serial.write(file.read());
//    }
// }
