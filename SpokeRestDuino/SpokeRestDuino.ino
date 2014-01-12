/*
 SpokeRestDuino
 
 An almost-RESTful API for an Arduino Uno Ethernet board.
 Methods to toggle pins between input & output, set & read values.
 If relays are connected, pins can be put into "garage" mode to control garage doors.
 
 Circuit:
 * Ethernet attached to pins 10, 11, 12, 13
 * Pins 1,2,4,6 unavailable
 * Pins 3,5,7,8,9 available for digital input & output
 * Pins A1-A5 available for analog input

 APIs Available -

 /                                 - Return list of available pins in JSON format.
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
                                      
  /<pin>                           - Return status of pin in JSON format -
                                        "digital":true,
                                        "output":true,
                                        "on":false
                                     <pin> in format [A0-A5,0-9]
                                      
  /<pin>/<value>                   - Set pin to value if available & digital. 
                                      HIGH|LOW sets value if output.  IN|OUT changes mode.
                                      No error checking currently done on value of pin, just atoi.
                                    
  /GAR                             - List garages in JSON format
                                      {
                                        "G0": {
                                          "open":false,
                                          "on":true
                                        }
                                      }

  /<garage>                          - Return status of garage in JSON format -
                                        "open":false,
                                        "on":false
                                     <garage> in format [G0-G4]                            

  /<garage>/<value>                  - Perform action on garage. 
                                      TOG opens or closes the garage.  ON/OFF toggles the power.
                                      
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

#define NUM_PINS 16
pin_t pin_list[NUM_PINS];  // To store status of all pins, array index is pin_num.
                     // Analog pins A1-A5 stored in 11-15

typedef struct {
  byte sensor_pin;  // Pin going to magnetic sensor, wired with pull-down resistor, digital on when closed, digital off when open
  byte driver_pin;  // Pin going to relay controlling door up/down, wired N.O
  byte power_pin; // Pin going to powerswitch tail 2 for hard off of whole system
  boolean open;
  boolean on;
} garage_t;

#define NUM_GARAGES 1
garage_t garage_list[NUM_GARAGES];  // More garages can be added if needed

typedef enum {
  RETURN_ALL_PINS,
  RETURN_ALL_GARAGES,
  RETURN_DIGITAL_PIN,
  RETURN_ANALOG_PIN,
  RETURN_GARAGE,
  SET_DIGITAL_PIN,
  SET_GARAGE,
  NONE
} action_t;

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(8080);

void initialize_garage (byte garage_num, byte driver_pin, byte sensor_pin, byte power_pin) {
  garage_list[garage_num].driver_pin = driver_pin;
  garage_list[garage_num].sensor_pin = sensor_pin;
  garage_list[garage_num].power_pin = power_pin;
  pinMode(garage_list[garage_num].sensor_pin,INPUT);
  pinMode(garage_list[garage_num].driver_pin,OUTPUT);
  pinMode(garage_list[garage_num].power_pin,OUTPUT);
  digitalWrite(garage_list[garage_num].driver_pin,LOW);
  digitalWrite(garage_list[garage_num].power_pin,LOW);
  pin_list[driver_pin].available = false;
  pin_list[sensor_pin].available = false;
  pin_list[power_pin].available = false;
}

void initialize_digital_output_pin (byte pin_num) {
  pinMode(pin_num, OUTPUT);
  digitalWrite(pin_num, LOW);
  pin_list[pin_num].available = true;
  pin_list[pin_num].digital = true;
  pin_list[pin_num].output = true;
  pin_list[pin_num].on = false;
}

void initialize_digital_input_pin (byte pin_num) {
  pinMode(pin_num, INPUT);
  digitalWrite(pin_num, HIGH);
  pin_list[pin_num].available = true;
  pin_list[pin_num].digital = true;
  pin_list[pin_num].output = false;
  pin_list[pin_num].on = false;
}

void initialize_analog_input_pin (byte pin_num) {
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

void assemble_garage_json(EthernetClient client, byte garage_num) {
  boolean open = digitalRead(garage_list[garage_num].sensor_pin);
  client.print(F("\"open\":"));
  if (open) {
    client.print(F("true"));
  }
  else {
    client.print(F("false"));
  }
  client.print(",");
  client.print(F("\"on\":"));
  if (garage_list[garage_num].on) {
    client.print(F("true"));
  }
  else {
    client.print(F("false"));
  }
}

void assemble_pin_json(EthernetClient client, byte pin_num) {
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

void garage_toggle(byte driver_pin) {
  digitalWrite(driver_pin,HIGH);
  delay(300);
  digitalWrite(driver_pin,LOW);
}

void setup()
{
#if DEBUG
  //  turn on serial (for debugging)
  Serial.begin(9600);
#endif
  Ethernet.begin(mac, ip);
  server.begin();
  // Initialize available pins
  
  for (byte i = 0; i < NUM_PINS; i++) {
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
  
  //initialize garages here, if more entries added make sure to increase global garage_list array
  initialize_garage(0,7,9,5);  //garage_num=0,driver_pin=7, sensor_pin=9, power_pin=5
}

#define BUFSIZE 12

void loop()
{
  char clientline[BUFSIZE];
  int index = 0;
  EthernetClient client = server.available();
  if (client) {
    index = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if(c != '\n' && c != '\r' && index < BUFSIZE){ 
          clientline[index++] = c;
          continue;
        }  
        client.flush();
        String url_string = String(clientline);
        //  extract the operation
        String op = url_string.substring(0,url_string.indexOf(' '));
        url_string = url_string.substring(url_string.indexOf('/'), url_string.indexOf(' ', url_string.indexOf('/')));
        url_string.toUpperCase();
        url_string.toCharArray(clientline, BUFSIZE);
        char *pin = strtok(clientline,"/");
        char *value = strtok(NULL,"/");
        char out_value[10] = "MU";
        
        action_t requested_action;
        
        if ((pin == NULL) && (value == NULL)) {
          requested_action = RETURN_ALL_PINS;
        }
        else if ((strncmp(pin, "GAR", 3) == 0) && (value == NULL)) {
          requested_action = RETURN_ALL_GARAGES;
        }
        else if ((pin != NULL) && (value == NULL)) {
          if (pin[0] == 'A') {
            requested_action = RETURN_ANALOG_PIN;
          }
          else if (pin[0] == 'G') {
            requested_action = RETURN_GARAGE;
          }
          else {
            requested_action = RETURN_DIGITAL_PIN;
          }
        }
        else if ((pin != NULL) && (value != NULL)) {
          if (pin[0] == 'A') {
            requested_action = NONE;
          }
          else if (pin[0] == 'G') {
            requested_action = SET_GARAGE;
          }
          else {
            requested_action = SET_DIGITAL_PIN;
          }
        }
        else {
          requested_action = NONE;
        }   
        
        switch (requested_action) {
          case RETURN_ALL_PINS:
          {
            print_http_ok(client);
            boolean firstLine = true;
            client.print("{");
            for (byte i = 0; i < NUM_PINS; i++){
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
            break;
          }
          case RETURN_ALL_GARAGES:
          {
            print_http_ok(client);
            boolean firstLine = true;
            client.print("{");
            for (byte i = 0; i < NUM_GARAGES; i++){
              if(!firstLine) {
                client.print(",");
              }
              firstLine = false;
                  client.print("\"");
              client.print("G");
              client.print(i);
              client.print("\"");
              client.print(":{");
              assemble_garage_json(client,i);
              client.print("}");
            }
            client.println("}");
            break;
          }
            
          case SET_DIGITAL_PIN:
          {
            byte selected_pin = atoi (pin);
            if (pin_list[selected_pin].available && pin_list[selected_pin].digital) {
              if((strncmp(value, "HIGH", 4) == 0)  && pin_list[selected_pin].output) {
                digitalWrite(selected_pin, HIGH);
                pin_list[selected_pin].on = true;   
                print_http_ok(client);      
              }
              else if((strncmp(value, "LOW", 3) == 0)  && pin_list[selected_pin].output) {
                digitalWrite(selected_pin, LOW);
                pin_list[selected_pin].on = false;  
                print_http_ok(client);       
              }
              else if(strncmp(value, "IN", 2) == 0) {
                initialize_digital_input_pin(selected_pin);
                print_http_ok(client); 
              }
              else if(strncmp(value, "OUT", 3) == 0) {
                initialize_digital_output_pin(selected_pin);
                print_http_ok(client);
              }
              else {
                print_http_error(client);
              }
            }
            break;
          }
          
          case RETURN_ANALOG_PIN:
          {
            byte selected_pin = pin[1] - '0';
            sprintf(out_value,"%d",analogRead(selected_pin));
            print_http_ok(client);
            client.print("{");
            assemble_pin_json(client,selected_pin+10);
            client.println("}");
            break;
          }
            
          case RETURN_GARAGE:
          {
            byte selected_garage = pin[1] - '0';
            print_http_ok(client);
            client.print("{");
            assemble_garage_json(client,selected_garage);
            client.println("}");
            break;
          }

          case RETURN_DIGITAL_PIN:
          {
            byte selected_pin = pin[0] - '0';
            print_http_ok(client);
            client.print("{");
            assemble_pin_json(client,selected_pin);
            client.println("}");
            break;
          }
            
          case SET_GARAGE:
          {
            byte selected_garage = pin[1] - '0';
            
            if(strncmp(value, "TOG", 3) == 0) {
              garage_toggle(selected_garage);
              print_http_ok(client);         
            }
            else if(strncmp(value, "OFF", 3) == 0) {
              digitalWrite(garage_list[selected_garage].power_pin,LOW);  
              garage_list[selected_garage].on = false;  
              print_http_ok(client);   
            }
            else if(strncmp(value, "ON", 2) == 0) {
              digitalWrite(garage_list[selected_garage].power_pin,HIGH);  
              garage_list[selected_garage].on = true;   
              print_http_ok(client);
            }
            else {
              print_http_error(client);
            }
            break;
          }

          default:
          {
            print_http_error(client);
            break;
          }
        }
      break;
      }
    }
    // give the web browser time to receive the data
    delay(1);
    client.stop();
    while(client.status() != 0){
      delay(5);
    }
  }
}
