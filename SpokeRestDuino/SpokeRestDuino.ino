/*
 RESTduino
 
 A REST-style interface to the Arduino via the 
 Wiznet Ethernet shield.
 
 Based on David A. Mellis's "Web Server" ethernet
 shield example sketch.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * pins 1,2,4, 6 also unavailable
 * Pins 3,5,7,8,9 available for digital input & output
 * Pins A1-A5 available for analog input
 
 created 04/12/2011
 by Jason J. Gullickson
 
 added 10/16/2011
 by Edward M. Goldberg - Optional Debug flag
 
 modified 3/2/2013
 by Will Ochandarena - added functions to track/return digital pin status
 
 Workflow -

 /                                  - Return list of available pins in JSON format.
                                      {
                                        "3": {
                                          "digital":true,
                                          "output":true,
                                          "on":false
                                        }
                                        "5": {
                                          "digital":true,
                                          "output":true,
                                          "on":false
                                        }
                                        "A0": {
                                          "digital":false,
                                          "output":false,
                                          "value":1022
                                        }                                      
                                      }
                                      
  /pin                              - Return status of pin in JSON format -
                                        "digital":true,
                                        "output":true,
                                        "on":false
                                     PIN in format [A0-A5,0-9]
                                      
  /pin/value                       - Set pin to value if available & digital. 
                                      HIGH|LOW sets value if output.  IN|OUT changes mode.
                                      No error checking currently done on value of pin, just atoi.
                                    
 
 */

#define DEBUG false
#include <SPI.h>
#include <Ethernet.h>
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};  //CHANGEME!
byte ip[] = {192,168,1,250};                        //CHANGEME!

typedef struct {
  boolean available;  // To filter out pins reserved for Ethernet, others
  boolean digital;  // Should be obvious from the pin name
  boolean output;  // For digital pins, 1 if output, 0 if input
  boolean on;  // For digital output pins, use this to track 1 = On, 0 = Off
} pin_t;

pin_t pin_list[16];  // To store status of all pins, array index is pin_num.
                     // Analog pins A1-A5 stored in 11-15

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(8080);

void initialize_digital_output_pin (int pin_num) {
  pinMode(pin_num, OUTPUT);
  digitalWrite(pin_num, LOW);
  pin_list[pin_num].available = true;
  pin_list[pin_num].digital = true;
  pin_list[pin_num].output = true;
  pin_list[pin_num].on = false;
}

void initialize_digital_input_pin (int pin_num) {
  pinMode(pin_num, INPUT);
  digitalWrite(pin_num, HIGH);
  pin_list[pin_num].available = true;
  pin_list[pin_num].digital = true;
  pin_list[pin_num].output = false;
  pin_list[pin_num].on = false;
}

void initialize_analog_input_pin (int pin_num) {
  pin_list[pin_num].available = true;
  pin_list[pin_num].digital = false;
  pin_list[pin_num].output = false;
  pin_list[pin_num].on = false;
}

void print_http_ok (EthernetClient client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println();
}

void print_http_error (EthernetClient client) {
  client.println(F("HTTP/1.1 404 Not Found"));
  client.println(F("Content-Type: text/html"));
  client.println();
}

void assemble_pin_json(EthernetClient client, int pin_num) {
    client.print(F("\"digital\":"));
    if (pin_list[pin_num].digital) {
      client.print(F("true"));
      client.print(",");
      client.print(F("\"output\":"));
      if (pin_list[pin_num].output) {
        client.print(F("true"));
        client.print(",");
        client.print(F("\"on\":"));
        if (pin_list[pin_num].on) {
          client.print(F("true"));
        }
        else {
          client.print(F("false"));
        }
      }
      else {
        client.print(F("false"));
        client.print(",");
        client.print(F("\"on\":"));
        if (digitalRead(pin_num)) {
          client.print(F("true"));
        }
        else {
          client.print(F("false"));
        }
      }
    }
    else {
      client.print(F("false"));
      client.print(",");
      client.print(F("\"output\":"));
      client.print(F("false"));
      client.print(",");
      client.print(F("\"value\":"));
      client.print(analogRead(pin_num-10));
    }
}


void setup()
{
#if DEBUG
  //  turn on serial (for debugging)
  Serial.begin(9600);
#endif
  // start the Ethernet connection:
  Ethernet.begin(mac, ip);
  server.begin();
  // Initialize available pins
  
  for (int i = 0; i < 16; i++) {
    pin_list[i].available = false;
  }
  
  initialize_digital_output_pin(3);
  initialize_digital_output_pin(5);
  initialize_digital_output_pin(7);
  initialize_digital_output_pin(8);
  initialize_digital_output_pin(9);
  
  initialize_analog_input_pin(10);
  initialize_analog_input_pin(11);
  initialize_analog_input_pin(12);
  initialize_analog_input_pin(13);
  initialize_analog_input_pin(14);
  initialize_analog_input_pin(15);
  
}

//  url buffer size
#define BUFSIZE 12
// Toggle case sensitivity
#define CASESENSE true

void loop()
{
  char clientline[BUFSIZE];
  int index = 0;
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    //  reset input buffer
    index = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //  fill url the buffer
        if(c != '\n' && c != '\r' && index < BUFSIZE){ // Reads until either an eol character is reached or the buffer is full
          clientline[index++] = c;
          continue;
        }  
#if DEBUG
        Serial.print(F("client available bytes before flush: ")); Serial.println(client.available());
        Serial.print(F("request = ")); Serial.println(clientline);
#endif
        // Flush any remaining bytes from the client buffer
        client.flush();
#if DEBUG
        // Should be 0
        Serial.print(F("client available bytes after flush: ")); Serial.println(client.available());
#endif
        //  convert clientline into a proper
        //  string for further processing
        String url_string = String(clientline);
        //  extract the operation
        String op = url_string.substring(0,url_string.indexOf(' '));
        //  we're only interested in the first part...
        url_string = url_string.substring(url_string.indexOf('/'), url_string.indexOf(' ', url_string.indexOf('/')));
        //  put what's left of the URL back in client line
#if CASESENSE
        url_string.toUpperCase();
#endif
        url_string.toCharArray(clientline, BUFSIZE);
        //  get the first two parameters
        char *pin = strtok(clientline,"/");
        char *value = strtok(NULL,"/");
        //  this is where we actually *do something*!
        char out_value[10] = "MU";
        //String json_out = String();
        if(pin != NULL){
          if(value != NULL){
#if DEBUG
            //  set the pin value
            Serial.print(F("setting pin "));
#endif
            //  select the pin
            int selected_pin = atoi (pin);
#if DEBUG
            Serial.print(selected_pin);
#endif
            if (pin_list[selected_pin].available && pin_list[selected_pin].digital) {
              if((strncmp(value, "HIGH", 4) == 0)  && pin_list[selected_pin].output){
#if DEBUG
                Serial.println(" HIGH");
#endif
                digitalWrite(selected_pin, HIGH);
                pin_list[selected_pin].on = true;         
              }
              else if((strncmp(value, "LOW", 3) == 0) && pin_list[selected_pin].output){
#if DEBUG
                Serial.println(" LOW");
#endif
                digitalWrite(selected_pin, LOW);    
                pin_list[selected_pin].on = false; 
              }
              else if(strncmp(value, "IN", 2) == 0){
#if DEBUG
                Serial.println(F(" INPUT"));
#endif
                initialize_digital_input_pin(selected_pin); 
              }
              else if(strncmp(value, "OUT", 3) == 0){
#if DEBUG
                Serial.println(F(" OUTPUT"));
#endif
                initialize_digital_output_pin(selected_pin); 
              }
              //  return status
              print_http_ok(client);
            }
            else {
              //  error
#if DEBUG
              Serial.println(F("erroring"));
#endif
              print_http_error(client);
            }
          } 
          else {
            //  determine analog or digital
            if(pin[0] == 'a' || pin[0] == 'A'){
              //  analog
              int selected_pin = pin[1] - '0';
#if DEBUG
              Serial.println(selected_pin);
              Serial.println("analog");
#endif
              sprintf(out_value,"%d",analogRead(selected_pin));
#if DEBUG
              Serial.println(out_value);
#endif
              print_http_ok(client);
              client.print("{");
              assemble_pin_json(client,selected_pin+10);
              client.println("}");
            } 
            else if(pin[0] != NULL) {
              //  digital
              int selected_pin = pin[0] - '0';
#if DEBUG
              Serial.print(selected_pin);
              Serial.println(" digital");
#endif
            //  assemble the json output
            print_http_ok(client);
            client.print("{");
            assemble_pin_json(client,selected_pin);
            client.println("}");
            }
            else {
              //  error
#if DEBUG
              Serial.println(F("erroring"));
#endif
              print_http_error(client);
            }
          }
        } 
        else {
          //Read All Digital   
#if DEBUG
          //  read the pin value
          Serial.println(F("listing pins"));
#endif       
          print_http_ok(client);
          bool firstLine = true;
          client.print("{");
          for (int i = 0; i < 16; i++){
            if (pin_list[i].available) {
              if(!firstLine) {
                client.print(",");
              }
              firstLine = false;
                  client.print("\"");
              if (i < 10) {
                client.print(i);
              }
              else {
                client.print("A");
                client.print(i-10);
              }
              client.print("\"");
              client.print(":{");
              assemble_pin_json(client,i);
              client.print("}");
            }
          }
          client.println("}");
        }
        break;
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    //client.stop();
    client.stop();
      while(client.status() != 0){
      delay(5);
    }
  }
}
