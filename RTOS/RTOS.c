#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <SPI.h>

//Pin Definitions
const uint8_t BUTTON_PIN = PA13;  // Active LOW button
const uint8_t RELAY_PIN  = PA5;   // Relay output

// Declare Variables
const unsigned int numTasks = 4;
const unsigned long period = 1;//1ms
const unsigned long periodSample = 1;//1ms->1Khz
const unsigned long periodPrint = 500;//500ms with 2 states->1Hz
const unsigned long periodAVG = 10;//10ms with 10 cycles ->10Hz 100ms
const unsigned long periodRelay = 1;//100ms with 2 states ->5Hz

float power_mW[3] = {0.0f, 0.0f, 0.0f};//Sensor Data
float busVoltage_V[3] = {0.0f, 0.0f, 0.0f};//Sensor Data
float current_mA[3]   = {0.0f, 0.0f, 0.0f};//Sensor Data
float powers_mW[3][9]= {//Averaging array
    {0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0}
};
float power_avg[3] = {0.0f, 0.0f, 0.0f};//array for averages
unsigned char avg_count= 0;//Counter for averaging
float powers=0;//Temp for averaging

//Button/Relay
bool relayState = false;        // false = OFF (LOW), true = ON (HIGH)
unsigned char counter  = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // ms

//Tasks
typedef struct task
{
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*Function) (int);
} task;

task tasks[4];

enum Sample_States {Read};
int Sample (int state);

enum Print_States {print, Wait};
int Print (int state);

enum AVG_States {array, calculate};
int AVG (int state);

enum Relay_States {off, on};
int Relay (int state);

// INA219 instances with different I2C addresses
Adafruit_INA219 ina1(0x40);  // A0=GND, A1=GND
Adafruit_INA219 ina2(0x41);  // A0=VCC, A1=GND
Adafruit_INA219 ina3(0x44);  // A0=GND, A1=VCC

void TimerISR ()
{
    unsigned char i;
    for (i = 0; i < numTasks; i++)
    {
        if (tasks[i].elapsedTime >= tasks[i].period)
        {
            tasks[i].state = tasks[i].Function(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += period;
    }
}

void setup(){
    Serial.begin(115200);
    // small delay so the USB serial comes up
    delay(2000);
    Wire.begin();  // uses default I2C1 pins (PB8=SCL, PB9=SDA on Nucleo-F446RE)
    // Initialize all three INA219 devices
    if (!ina1.begin()) {
        Serial.println(F("Failed to find INA219 #1 (0x40). Check wiring."));
    } else {
        // Choose a calibration that matches your expected range
        ina1.setCalibration_32V_2A();
    }
    if (!ina2.begin()) {
        Serial.println(F("Failed to find INA219 #2 (0x41). Check wiring."));
    } else {
        ina2.setCalibration_32V_2A();
    }
    if (!ina3.begin()) {
        Serial.println(F("Failed to find INA219 #3 (0x44). Check wiring."));
    } else {
        ina3.setCalibration_32V_2A();
    }
    Serial.println(F("INA219 multi-channel monitor ready."));

     // GPIO for button and relay
    pinMode(BUTTON_PIN, INPUT_PULLUP); // active LOW, use internal pull-up
    pinMode(RELAY_PIN, OUTPUT);

    // Start with relay OFF
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
}

int main ()
{
    tasks[0].state = Read;
    tasks[0].period = periodSample;
    tasks[0].elapsedTime = tasks[0].period;
    tasks[0].Function = &Sample;
    tasks[1].state = print;
    tasks[1].period = periodPrint;
    tasks[1].elapsedTime = tasks[1].period;
    tasks[1].Function = &Print;
    tasks[2].state = array;
    tasks[2].period = periodAVG;
    tasks[2].elapsedTime = tasks[2].period;
    tasks[2].Function = &AVG;
    tasks[3].state = off;
    tasks[3].period = periodRelay;
    tasks[3].elapsedTime = tasks[3].period;
    tasks[3].Function = &Relay;

    TimerSet (period);
    TimerOn ();

    while (1);
    return 0;
}
// Task implementations
int Sample (int state)
{
    switch (state)
    {
        case (Read):
            // INA219 #1
            busVoltage_V[0] = ina1.getBusVoltage_V();
            current_mA[0]   = ina1.getCurrent_mA();
            power_mW[0]     = ina1.getPower_mW();
            // INA219 #2
            busVoltage_V[1] = ina2.getBusVoltage_V();
            current_mA[1]   = ina2.getCurrent_mA();
            power_mW[1]     = ina2.getPower_mW();
            // INA219 #3
            busVoltage_V[2] = ina3.getBusVoltage_V();
            current_mA[2]   = ina3.getCurrent_mA();
            power_mW[2]     = ina3.getPower_mW();
            state=Read;
            break;
    }
    return state;
}

int Print (int state)
{
    switch (state)
    {
        case (print):
            Serial.println(F("===== INA219 Power Readings ====="));
            for (int i = 0; i < 3; ++i){
                Serial.print(F("Channel "));
                Serial.print(i + 1);
                Serial.print(F(": "));
                Serial.print(power_mW[i], 2);
                Serial.print(F(" mW"));
                // Optional extra info:
                Serial.print(F("  (Vbus="));
                Serial.print(busVoltage_V[i], 3);
                Serial.print(F(" V, I="));
                Serial.print(current_mA[i], 3);
                Serial.println(F(" mA)"));
            }
            Serial.println();
            state=Wait;
            break;  
        case(Wait):
            state=print;
            break;
    }
    return state;
}

int AVG(int state){
    switch(state){
        case(array):
            powers_mW[0][avg_count]=power_mW[0];
            powers_mW[1][avg_count]=power_mW[1];
            powers_mW[2][avg_count]=power_mW[2];
            avg_count=avg_count+1;
            if(avg_count>8) state=calculate; 
            break;
        case(calculate):
            for(int i=0; i<3; ++i){
                for(int j=0; j<9; ++j){
                    powers=powers+powers_mW[i][j];
                }
                powers=powers+power_mW[i];
                power_avg[i]=powers/10;
                powers=0;
            }
            avg_count = 1;
            powers_mW[0][0]=power_mW[0];
            powers_mW[1][0]=power_mW[1];
            powers_mW[2][0]=power_mW[2];
            state=array;
            break;
    }
    return state;
}

int Relay(int state){
    int reading = digitalRead(BUTTON_PIN);
    switch(state){
        case(off):
            digitalWrite(RELAY_PIN, LOW);
            if(reading==0) counter++;
            else counter=0;

            if(counter>=50){
                counter=0;
                state=on;
            }
            break;
        case(on):
            digitalWrite(RELAY_PIN, HIGH);
            if(reading!=0) counter++;
            else counter=0;

            if(counter>=50){
                counter=0;
                state=off;
            }
            break;
    }
    return state;
}