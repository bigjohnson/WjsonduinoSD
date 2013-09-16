/*			          *
 *  Arduino Power Ethernet Server *
 *	    11 May 2011           *
 *         Basic Arduino          *
 *     power ethernet server      *
 *   a command line interface     *
 *        by Alberto Panu         *
 *      alberto[at]panu.it        *
 *       based on work of         *
 *         Steve Lentz            *
 *    stlentz[at]gmail[dot]com    *

    copyright Alberto Panu 2012
    
    Ardupower http://www.panu.it/wjsonduino
    
    Your comments and suggestion are welcomed to alberto[at]panu.it
    
    Released under GPL licence V3 see http://www.gnu.org/licenses/gpl.html and file gpl.txt
*/

// Standart libaryes from arduino 1.0.1
#include <pins_arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

// Need extra Flash library from http://arduiniana.org/libraries/flash/
#include <Flash.h>

// Need extra MyTinyWebServer library from http://www.panu.it/wjsonduino/MyTinyWebServer.zip
#include <MyTinyWebServer.h>
// Or if you wont you can use the original library from https://github.com/ovidiucp/MyTinyWebServer
//#include <TinyWebServer.h>

/****************VALUES YOU CHANGE*************/
// pin 4 is the SPI select pin for the SDcard
#define SD_CS 4

// pin 10 is the SPI select pin for the Ethernet
#define ETHER_CS 10

// Enable serial print debug message
// #define DEBUG 1

// The 4 pin is used by SD_CS and for future development is jumped
static byte ioports[] = { 2, 3, 5, 6, 7, 8, 9 };
#define ioportsnum 7
byte ioportsStatus[ioportsnum];
byte actualread = 0;
unsigned int analogValues[6];
byte actualanalog = 0;

// Don't forget to modify the IP to an available one on your home network
byte ip[] = { 192, 168, 0, 5 };
byte gateway[] = { 192, 168, 0, 1 };
byte subnet[] = { 255, 255, 255, 0 };
/*********************************************/
// Substitute with the eth shiel mac address
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

boolean index_handler(MyTinyWebServer& web_server);
boolean json_handler(MyTinyWebServer& web_server);

boolean has_filesystem = true;
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

MyTinyWebServer::PathHandler handlers[] = {
  {"/", MyTinyWebServer::GET, &index_handler },
  {"/json" "*", MyTinyWebServer::GET, &json_handler },
  {NULL},
};

boolean json_handler(MyTinyWebServer& web_server) {
  web_server.send_error_code(200);
  web_server.end_headers();
  
  web_server << F("processData( { ");
    for (byte i=0; i < ioportsnum; i++) {
      web_server << F("\"IO");
      web_server << (int) ioports[i];
      web_server << F("\" : \"");
      //if (digitalRead(ioports[i])) {
        if (ioportsStatus[i]) {
        web_server << F("H");
      } else {
        web_server << F("L");
      }
      web_server << F("\", ");
    }
    for (byte i=0; i < 6; i++){
      web_server << F("\"AD");
      web_server << (int) i;
      web_server << F("\" : \"");
//      web_server << analogRead(i);
       web_server << analogValues[i];
      web_server << F("\"");
      if ( i < 5) {
        web_server << F(", ");
      }
    }
    web_server << F("} );\n"); 
    #ifdef DEBUG
      Serial << F("Sent json\n");
    #endif
  return true;
}

boolean index_handler(MyTinyWebServer& web_server) {
  send_file_name(web_server, "INDEX.HTM");
  return true;
}

boolean has_ip_address = false;
MyTinyWebServer web = MyTinyWebServer(handlers, NULL);

const char* ip_to_str(const uint8_t* ipAddr)
{
  static char buf[16];
  sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return buf;
}

void file_uploader_handler(MyTinyWebServer& web_server,
			   TinyWebPutHandler::PutAction action,
			   char* buffer, int size) {
  static uint32_t start_time;
  static uint32_t total_size;

  switch (action) {
  case TinyWebPutHandler::START:
    start_time = millis();
    total_size = 0;
    if (!file.isOpen()) {
      // File is not opened, create it. First obtain the desired name
      // from the request path.
      char* fname = web_server.get_file_from_path(web_server.get_path());
      if (fname) {
	Serial << F("Creating ") << fname << "\n";
	file.open(&root, fname, O_CREAT | O_WRITE | O_TRUNC);
	free(fname);
      }
    }
    break;

  case TinyWebPutHandler::WRITE:
    if (file.isOpen()) {
      file.write(buffer, size);
      total_size += size;
    }
    break;

  case TinyWebPutHandler::END:
    file.sync();
    Serial << F("Wrote ") << file.fileSize() << F(" bytes in ")
	   << millis() - start_time << F(" millis (received ")
           << total_size << F(" bytes)\n");
    file.close();
  }
}

void setup() {
  
  #ifdef DEBUG
    Serial.begin(9600);
    Serial << F("Free RAM: ") << FreeRam() << "\n";
  #endif
  
  pinMode(SS_PIN, OUTPUT);	// set the SS pin as an output
                                // (necessary to keep the board as
                                // master and not SPI slave)
  digitalWrite(SS_PIN, HIGH);	// and ensure SS is high

  // Ensure we are in a consistent state after power-up or a reset
  // button These pins are standard for the Arduino w5100 Rev 3
  // ethernet board They may need to be re-jigged for different boards
  pinMode(ETHER_CS, OUTPUT);	// Set the CS pin as an output
  digitalWrite(ETHER_CS, HIGH);	// Turn off the W5100 chip! (wait for
                                // configuration)
  pinMode(SD_CS, OUTPUT);	// Set the SDcard CS pin as an output
  digitalWrite(SD_CS, HIGH);	// Turn off the SD card! (wait for
                                // configuration)

  for (byte i=0; i < ioportsnum; i++) {
    pinMode(ioports[i], INPUT_PULLUP);
  }
  
    // initialize the SD card.
  #ifdef DEBUG  
    Serial << F("Setting up SD card...\n");
  #endif
  // Pass over the speed and Chip select for the SD card
  if (!card.init(SPI_FULL_SPEED, SD_CS)) {
    #ifdef DEBUG
      Serial << F("card failed\n");
    #endif
    has_filesystem = false;
  }
  // initialize a FAT volume.
  if (!volume.init(&card)) {
    #ifdef DEBUG
      Serial << F("vol.init failed!\n");
    #endif
    has_filesystem = false;
  }
  if (!root.openRoot(&volume)) {
    #ifdef DEBUG
      Serial << F("openRoot failed\n");
    #endif
    has_filesystem = false;
  }

  if (has_filesystem) {
    // Assign our function to `upload_handler_fn'.
    TinyWebPutHandler::put_handler_fn = file_uploader_handler;
  }
    
  // Initialize the Ethernet.
  #ifdef DEBUG
    Serial << F("Setting up the Ethernet card...\n");
  #endif
  
  Ethernet.begin(mac, ip);

  // Start the web server.
  #ifdef DEBUG
    Serial << F("Web server starting...\n");
  #endif  
  web.begin();
  #ifdef DEBUG
    Serial << F("Ready to accept HTTP requests.\n\n");
  #endif
}

void loop() {
  ioportsStatus[actualread] = digitalRead(ioports[actualread]);
  actualread++;
  if (actualread >= ioportsnum) {
    actualread = 0;
  }
  
  unsigned int midread = 0;
  for (byte i=0; i <= 10; i++) {
    midread = midread + analogRead(actualanalog);
  }
  analogValues[actualanalog] = midread / 10;
  actualanalog++;
  if (actualanalog > 5) {
    actualanalog = 0;
  }
  web.process();
}

void send_file_name(MyTinyWebServer& web_server, const char* filename) {
  if (!filename) {
    web_server.send_error_code(404);
    web_server << F("Could not parse URL");
  } else {
    MyTinyWebServer::MimeType mime_type
      = MyTinyWebServer::get_mime_type_from_filename(filename);
    web_server.send_error_code(200);
    web_server.send_content_type(mime_type);
    web_server.end_headers();
    if (file.open(&root, filename, O_READ)) {
      #ifdef DEBUG
        Serial << F("Read file "); Serial.println(filename);
      #endif
      web_server.send_file(file);
      file.close();
    } else {
      web_server << F("Could not find file: ") << filename << "\n";
    }
  }
}
