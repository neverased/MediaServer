#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#include "SdFat.h"
#include "FreeStack.h"
#include <LiquidCrystal.h>
LiquidCrystal lcd(2, 3, 4, 5, 6, 7); //Informacja o podlaczeniu nowego wyswietlacza

/************ ETHERNET STUFF ************/
byte mac[] = { 0x02, 0xAC, 0xCB, 0xCC, 0xDD, 0x04 };
byte ip[] = { 192, 168, 10, 3};
EthernetServer server(80);

const char legalChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890/.-_~";
unsigned int requestNumber = 0;

unsigned long connectTime[MAX_SOCK_NUM];

/************ SDCARD STUFF ************/
SdFat sd;
SdFile file;
SdFile dirFile;
File myFile;

int clientCount = 0;
int k;
uint8_t tBuf[100];

// Number of files found.
uint16_t n = 0;
uint16_t x = 0;

const uint16_t nMax = 50;

// Position of file's directory entry.
uint16_t dirIndex[nMax];

char myChar;

const char Style[] PROGMEM =
"<html>"
"<head>"
"<style>"
"body {"
"  margin: 0;"
"  font-family: -apple-system, BlinkMacSystemFont, Roboto, Arial, sans-serif;"
"  font-size: 1em;"
"  font-weight: 400;"
"  line-height: 1.5em;"
" color: #111;"
"  text-align: left;"
"  background-color: #fff;"
"}"
""
"ul {"
"  width: 700px;"
"}"

"li:before {"
"  display: flex;"
"  margin-right: 0.55rem;"
"  margin-top: 5px;"
"  content: url(data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIyNCIgaGVpZ2h0PSIyNCIgdmlld0JveD0iMCAwIDI0IDI0IiBmaWxsPSJub25lIiBzdHJva2U9ImN1cnJlbnRDb2xvciIgc3Ryb2tlLXdpZHRoPSIyIiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiIGNsYXNzPSJmZWF0aGVyIGZlYXRoZXItZGlzYyI+PGNpcmNsZSBjeD0iMTIiIGN5PSIxMiIgcj0iMTAiPjwvY2lyY2xlPjxjaXJjbGUgY3g9IjEyIiBjeT0iMTIiIHI9IjMiPjwvY2lyY2xlPjwvc3ZnPg==);"
"}"
"li {"
"  align-items: center;"
"  display: flex;"
"  justify-content: flex-start;"
"  padding: 0.55rem;"
""
"  margin-bottom: -1px;"
"  background-color: #fff;"
"  border: 1px solid rgba(0, 0, 0, 0.125);"
"  overflow: hidden;"
"}"
"li:hover {"
"  color: #111;"
"  text-decoration: none;"
"  background-color: #f012be;"
"}"
""
"li:first-child {"
"  border-top-left-radius: 0.55rem;"
"  border-top-right-radius: 0.55rem;"
"}"
""
"li:last-child {"
"  margin-bottom: 0;"
"  border-bottom-right-radius: 0.55rem;"
"  border-bottom-left-radius: 0.55rem;"
"}"
""
"li a {"
"  text-decoration: none;"
"  color: #111;"
"}"
"</style>"
"</head>"
"<body>"
;

//------------------------------------------------------------------------------
void setup() {
Serial.begin(9600);

lcd.begin(16, 2); //Deklaracja typu
lcd.setCursor(0, 0); //Ustawienie kursora
lcd.print("IP Address:"); //Wyswietlenie tekstu
lcd.setCursor(0, 1); //Ustawienie kursora

  while (!Serial) {}
  delay(1000);

  if (!sd.begin(4)) {
    sd.initErrorHalt();
  }
  Serial.print(F("FreeStack: "));
  Serial.println(FreeStack());
  Serial.println();

 // List files in root directory.
  if (!dirFile.open("/", O_READ)) {
    sd.errorHalt("open root failed");
  }
  while (n < nMax && file.openNext(&dirFile, O_READ)) {

    // ukryj podfoldery i ukryte pliki
    if (!file.isSubDir() && !file.isHidden()) {

      // Save dirIndex of file in directory.
      dirIndex[n] = file.dirIndex();

      // Print the file number and name.
      Serial.print(n++);
      Serial.write(' ');
      file.printName(&Serial);
      Serial.println();
    }
    file.close();
  }

  Ethernet.begin(mac, ip);
  Serial.print(Ethernet.localIP());
  server.begin();

  lcd.print(Ethernet.localIP()); //Wyswietlenie ip po DHCP
}

void ListFiles(EthernetClient client) {


  dirFile.rewind ();
  client.println("<ul>");

  while (x < nMax && file.openNext(&dirFile, O_READ)) {


    if (!file.isSubDir() && !file.isHidden()) {

      // Save dirIndex of file in directory.
      dirIndex[n] = file.dirIndex();

      // Print the file number and name.
      client.print("<li><a href=\"");
      file.printName(&client);
      client.print("\">");
    }

      // print file name with possible blank fill
      file.printName(&client);
      client.print("</a>");

    file.close();

     client.println("</li>");
  }
  client.println("</ul>");
}


//------------------------------------------------------------------------------
#define BUFSIZE 100

void loop() {

  char clientline[BUFSIZE];
  int index = 0;

EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line

    boolean current_line_is_blank = true;

    // reset the input buffer
    index = 0;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        // If it isn't a new line, add the character to the buffer
        if (c != '\n' && c != '\r') {
          clientline[index] = c;
          index++;
          // are we too big for the buffer? start tossing out data
          if (index >= BUFSIZE)
            index = BUFSIZE -1;

          // continue to read more data!
          continue;
        }

        // got a \n or \r new line, which means the string is done
        clientline[index] = 0;

        // Print it out for debugging
        Serial.println(clientline);

        // Look for substring such as a request to get the root file
        if (strstr(clientline, "GET / ") != 0) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println();
          for (k = 0; k < strlen_P(Style); k++)
              {
                  myChar = pgm_read_byte_near(Style + k);
                  Serial.print(myChar);
                  client.print(myChar);

              }

          // print all the files, use a helper to keep it clean
          client.println("<h2>Pliki:</h2>");
          ListFiles(client);
          client.println("</body>");
          client.println("</html>");
        } else if (strstr(clientline, "GET /") != 0) {

          char *filename;

          filename = clientline + 5; // po "GET /" (5 znaków)
          // szukaj " HTTP/1.1"
          // zamien peirwszy znak substringa na 0
          (strstr(clientline, " HTTP"))[0] = 0;

          // print the file we want
          Serial.println(filename);

          if (! file.open(&dirFile, filename, O_READ)) {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: audio/mpeg3");
            client.println();
            client.println("<h2>File Not Found!</h2>");
            break;
          }

          Serial.println("Opened!");

           while(file.available()) {
                    clientCount = file.read(tBuf,sizeof(tBuf));
                    client.write(tBuf,clientCount);
                  }

    // close the file:
          file.close();

        } else {
          // everything else is a 404
          client.println("HTTP/1.1 404 Not Found");
          client.println("Content-Type: text/html");
          client.println();
          client.println("<h2>File Not Found!</h2>");
        }
        break;
      }
    }
    // czas dla przegladarki na odebranie pliku
    delay(1);
    client.stop();
  }


  Serial.flush();
  delay(100);
}
