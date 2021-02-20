#include <EEPROM.h>
#include <SoftwareSerial.h>

SoftwareSerial espSerial(10, 11); // RX, TX
int x=0;
int str[2]={0,0};
int times = 0;
bool objectState = false;

#define sensor_sharp A3
#define relay_uv 2
#define led_sterilize 13
#define led_detector 12

//Save to EEPROM
int mode_auto = 1; //1 == OTOMATIS 0 == MANUAL
unsigned long timer_duration = 5;

long previousMillis = 0;     
bool sterilState = false;
bool uvState = false;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
  espSerial.begin(38400);
  delay(10);
  Serial.println("Startup");

  //Load last state EEPROM
  mode_auto = EEPROM.read(1);
  timer_duration = EEPROM.read(2);

  pinMode(relay_uv, OUTPUT);
  pinMode(led_sterilize, OUTPUT);
  pinMode(led_detector, OUTPUT);
  digitalWrite(relay_uv, HIGH);
  digitalWrite(led_sterilize, LOW);
  digitalWrite(led_detector, LOW);
}

void loop() {
  /* --- PENDETEKSIAN BARANG --- */
  //float volts = analogRead(sensor_sharp) * 0.00048828125; // value from sensor sharp * (5/1024)
  //int distance = 13 * pow(volts, -1); // worked out from datasheet graph
  int adc = analogRead(sensor_sharp);
  //Serial.println(adc);

  if (adc >= 200)  //spike filtering
    times++;
  else 
    times = 0;
  
  if (times > 5) 
    objectState = true;
  else
    objectState = false;

  if (objectState) { //if any packet entering box
    //Send new packet to ESP
    espSerial.write("A");
    Serial.println("New packet detected");

    if (mode_auto == 1) { 
        Serial.println("System Otomatis");
        sterilState = true; //mode sterilisasi
        uvState = true;
      }
      else {
        Serial.println("System Manual");
      }

    delay(4000); //avoid bottleneck
  } else {
      // Serial.println("Tidak ada barang");
  }

  /* --- MODE STERILISASI --- */
  if (sterilState == true){ 
    //Serial.println("Sedang sterilisasi");

    if (uvState == true) {
      digitalWrite(relay_uv, LOW);
      digitalWrite(led_sterilize, HIGH);
      Serial.println("Lampu UV aktif");
      Serial.println("Update status cleaning dimulai");
      //Send cleaning state to ESP
      espSerial.write("B");
      uvState = false;
      previousMillis = millis();
    }

    unsigned long currentMillis = millis();
//    Serial.print(currentMillis - previousMillis);
//    Serial.print(" ");
//    Serial.println(timer_duration*1000);
    // if((currentMillis - previousMillis) > (timer_duration*1000)) { //in seconds: MODE TEST!!
   if(currentMillis - previousMillis > timer_duration*60000) { //in minutes
      Serial.println("Lampu UV nonaktif");
      Serial.println("Update status sudah steril");
      digitalWrite(relay_uv, HIGH);
      digitalWrite(led_sterilize, LOW);
      //Send finish state to ESP
      espSerial.write("C");
      sterilState = false;
    }
  }

  /* --- RECEIVE CONFIG --- */
  if (espSerial.available()) {
    delay(10);

    while (espSerial.available() > 0) {
        //Serial.write(espSerial.read()); //just for test
        int byteRead = espSerial.read();
        delay(10);

        if (byteRead=='M'){
            Serial.println("Tombol manual steril ditekan");
            sterilState = true;
            uvState = true;
        }
        else if(isdigit(byteRead))
            str[x] = (str[x]*10)+(byteRead-48);
        else if (byteRead==':'){ //separate data
            //break;
            x++;
        }
        else if (byteRead==';'){
            //1 = config mode_auto
            //2 = config timer_duration
            if (str[0] == 1) {
                mode_auto = str[1];
                Serial.print("Mode auto: ");
                Serial.println(str[1]);
                EEPROM.write(1, str[1]);
            }
            else if (str[0]== 2) {
                timer_duration = str[1];
                Serial.print("Timer duration: ");
                Serial.println(str[1]);
                EEPROM.write(2, str[1]);
            }

            x = 0; //reset string every ; handle bottleneck
            str[0]=0;str[1]=0; //reset cache var
        }
    }
    x = 0; //reset all pointer
    espSerial.flush();
  }

  if (Serial.available()) {
    //Serial.println(Serial.read());
    espSerial.write(Serial.read());
  }

  delay(10);
}
