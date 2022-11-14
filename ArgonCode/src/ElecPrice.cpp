/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/mathi/Desktop/IOT/ElecPrice/ArgonCode/src/ElecPrice.ino"
#include "string.h"
#include "application.h"
#include "../lib/MQTT/src/MQTT.h"
#include "cost_calc.h"
#include "state_variables.h"
#include "mDNSResolver.h"
#include "BLE_include.h"
#include <fcntl.h>
#include "price_handler.h"


//#define STATEDEBUG 1
void setup();
void loop();
void timer_callback();
void init_memory();
void rotate_prices();
void BLEOnConnectcallback(const BlePeerDevice& peer, void* context);
void transmit_prices(int start_stop[12][2], int size);
void check_time(void);
#line 13 "c:/Users/mathi/Desktop/IOT/ElecPrice/ArgonCode/src/ElecPrice.ino"
#define USEMQTT

#define KW_SENSOR_PIN D8
#define WATT_CONVERSION_CONSTANT 3600000
#define HOST "192.168.0.103"
#define PORT 1883
#define PULL_TIME_1 13
#define PULL_TIME_2 0

#define MAX_TRANSMIT_BUFF 128
#define SLEEP_DURATION 30000

Timer timer(60000, timer_callback);

statemachine state;

int oneShotGuard = -1;
int oneShotGuard2 = -1;
double cost[MAX_RANGE];
int * wh_yesterday;
int * wh_today;

 int fd_today;

bool NewBLEConnection =  false;
int last_connect = 0;
int calc_power;


//int calc_low(int **low_price_intervals);

void timer_callback(void);
void init_memory(void);
void get_data(int day);
void handle_sensor(void);
void check_mqtt(void);
void init_GPIO(void);
void transmit_prices(int start_stop[12][2], int cnt);
void handle_sensor(void);
void myPriceHandler(const char *event, const char *data);

#ifdef USEMQTT
// Callback function for MQTT transmission
void callback(char *topic, byte *payload, unsigned int length);
// Create MQTT client
MQTT client("192.168.110.6", PORT, 512, 30, callback);
#endif
// Create sleep instance
SystemSleepConfiguration config;

UDP udp;
mDNSResolver::Resolver resolver(udp);

SYSTEM_THREAD(ENABLED);

void setup()
{
    // Particle.connect();
    init_GPIO();

    // setup BLE
    ble_setup();

    // Initialize memory
    init_memory();

    state = STARTUP;
#ifdef STATEDEBUG
    digitalWrite(state, LOW);
#endif
    waitUntil(Particle.connected);
#ifdef STATEDEBUG
    digitalWrite(state, HIGH);
#endif
    state = GET_DATA;
#ifdef STATEDEBUG
    digitalWrite(state, HIGH);
#endif

    Time.zone(1);
    Time.beginDST();
    
    pinMode(KW_SENSOR_PIN, INPUT_PULLDOWN);                // Setup pinmode for LDR pin
    attachInterrupt(KW_SENSOR_PIN, handle_sensor, RISING); // Attach interrup that will be called when rising
#ifdef USEMQTT
    // Resolve MQTT broker IP address
    IPAddress IP = resolver.search("homeassistant.local");
    client.setBroker(IP.toString(), PORT);
#endif

    // Subscribe to the integration response event
    Particle.subscribe("prices", myHandler, MY_DEVICES);

    // Publish state variable to Particle cloud
    Particle.variable("State", state);

#ifdef USEMQTT
    // connect to the mqtt broker(unique id by Time.now())
    Serial.printf("Return value: %d", client.connect("client_" + String(Time.now()), "mqtt", "mqtt"));

    // publish/subscribe
    if (client.isConnected())
    {
        // Debugging publish
        client.publish("power/get", "hello world");
        // Subscribe to 2 topics
        // client.subscribe("power/get");
        client.subscribe("power/prices");
    }
#endif
    // Setup low power mode
    // config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(KW_SENSOR_PIN, RISING).network(NETWORK_INTERFACE_ALL);
}

void loop()
{
    static int start_stop[12][2] = {0};
    static int cnt = 0;

#ifdef USEMQTT
    check_mqtt();
#endif

    // Is it time to update the prices or has it been requested?
    if (state == GET_DATA)
    {
#ifdef STATEDEBUG
        digitalWrite(state, LOW);
#endif
        state = AWAITING_DATA;
#ifdef STATEDEBUG
        digitalWrite(state, HIGH);
#endif
        get_data(Time.day());
    }

    // Has the prices for today arrived?
    if (state == CALCULATE)
    {
        cnt = calc_low(start_stop, cost_today, cost_hour, range);
        Serial.printf("Current HH:MM: %02d:%02d\n", Time.hour() + 2, Time.minute());
        state = TRANSMIT_PRICE;
    }

    if (state == TRANSMIT_PRICE)
    {
        transmit_prices(start_stop, cnt);
    }

    if (state == TRANSMIT_SENSOR) // Did we receive a request for updated values
    {
        Serial.printf("Received power/get\n");
        #ifdef USEMQTT
        char values[16];
        sprintf(values, "%d", calc_power);
        client.publish("power", values);
        #endif
        char buffer[255];
        sprintf(buffer, "{\"watt\":%d}", calc_power);
        WattCharacteristic.setValue(buffer);

#ifdef STATEDEBUG
        digitalWrite(state, LOW);
#endif
        state = SLEEP_STATE;
#ifdef STATEDEBUG
        digitalWrite(state, HIGH);
#endif
    }

    if (state == ROTATE)
    {
        rotate_prices();
        state = SLEEP_STATE;
    }

    if(NewBLEConnection & ((millis()-last_connect)>1400)){
        //send everything relavant on new connect
        //needs a bit og delay to ensure device is ready
        char buffer[255];
        sprintf(buffer, "{\"watt\":%d}", calc_power);
        WattCharacteristic.setValue(buffer);
        DkkYesterdayCharacteristic.setValue("{\"pricesyesterday\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,24]}");
        DkkTodayCharacteristic.setValue("{\"pricestoday\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,24]}");  // string mKr/kwhr
        DkkTomorrowCharacteristic.setValue("{\"pricestomorrow\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24]}"); // string mKr/kwhr
        WhrYesterdayCharacteristic.setValue("{\"WHr_yesterday\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24]}");
        WhrTodayCharacteristic.setValue("{\"WHr_today\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24]}"); // Whr used in the corrisponding hour
        
        NewBLEConnection = false;
        Serial.printf("ble_connected");
    }

}
void timer_callback()
{
    check_time();
}
void init_memory()
{
    // Allocate for the prices
    cost_yesterday = (double *) malloc(MAX_RANGE * sizeof(double));
    cost_today  = (double *) malloc(MAX_RANGE * sizeof(double));
    cost_tomorrow = (double *) malloc(MAX_RANGE * sizeof(double));
    wh_today = (int *) malloc(MAX_RANGE * sizeof(int));
    wh_yesterday = (int *) malloc(MAX_RANGE * sizeof(int));
    // Set all values to 0
    memset(cost_yesterday, 0, MAX_RANGE * sizeof(double));
    memset(cost_today, 0, MAX_RANGE * sizeof(double));
    memset(cost_tomorrow, 0, MAX_RANGE * sizeof(double));
    memset(wh_today, 0, MAX_RANGE * sizeof(int));
    memset(wh_yesterday, 0, MAX_RANGE * sizeof(int));

    // Hent indhold af filer, så vi har gemte data i tilfælde af reboot
   fd_today = open("/sd/prices_today.txt", O_RDWR | O_CREAT);
}
void rotate_prices()
{
    // Rotate prices so that we can use the same array for all days
    double *temp = cost_yesterday;
    cost_yesterday = cost_today;
    cost_today = cost_tomorrow;
    cost_tomorrow = temp;

    for (int i = 0; i < MAX_RANGE; i++)
    {
        cost_tomorrow[i] = 0;
    }

    // Opdater filer med nye priser
}

void BLEOnConnectcallback(const BlePeerDevice& peer, void* context){
    NewBLEConnection = true;
    last_connect = millis();
}

void handle_sensor(void)
{
    static unsigned long last_read = 0;
    // statemachine prev_state = state;
    unsigned long delta;
    unsigned long current_reading = millis();

    if ((delta = current_reading - last_read) > 100)
    {
        Serial.printf("In interrupt\n");
#ifdef STATEDEBUG
        digitalWrite(state, LOW);
#endif
        state = SENSOR_READ;
#ifdef STATEDEBUG
        digitalWrite(state, HIGH);
#endif
        calc_power = WATT_CONVERSION_CONSTANT / delta;
        last_read = current_reading;
#ifdef STATEDEBUG
        digitalWrite(state, LOW);
#endif
        state = TRANSMIT_SENSOR;
// state = prev_state;
#ifdef STATEDEBUG
        digitalWrite(state, HIGH);
#endif
    }
}

void init_GPIO(void)
{
    pinMode(SENSOR_READ, OUTPUT);
    pinMode(GET_DATA, OUTPUT);
    pinMode(CALCULATE, OUTPUT);
    pinMode(TRANSMIT_PRICE, OUTPUT);
    pinMode(TRANSMIT_SENSOR, OUTPUT);
    pinMode(SLEEP_STATE, OUTPUT);
    pinMode(AWAITING_DATA, OUTPUT);
    pinMode(STARTUP, OUTPUT);
}

void callback(char *topic, byte *payload, unsigned int length)
{
#ifdef STATEDEBUG
    digitalWrite(state, LOW);
#endif
    state = GET_DATA;
#ifdef STATEDEBUG
    digitalWrite(state, HIGH);
#endif
}


/** @brief Reconnects MQTT client if disconnected
 */
void check_mqtt(void)
{
    if (client.isConnected())
    {
        client.loop();
    }
    else
    {
        Serial.printf("Client disconnected\n");
        client.connect("sparkclient_" + String(Time.now()), "mqtt", "mqtt");
        if (client.isConnected())
        {
            Serial.printf("Client reconnected\n");
        }
    }
}

void transmit_prices(int start_stop[12][2], int size)
{
    Serial.printf("In work\n");
    // Do some meaningfull work with the collected data
    String data = "Cheap(ish) hours of the day: ";
    for (int z = 0; z < size; z++)
    {
        data += String::format("%02d to %02d, ", start_stop[z][0], start_stop[z][1]);
    }
    // Publish the cheap hours to cloud
    Particle.publish("Low price hours", data, PRIVATE);
    // Publish cheap hour to MQTT
    client.publish("prices", data);
    client.loop();
#ifdef STATEDEBUG
    digitalWrite(state, LOW);
#endif
    state = SLEEP_STATE;
#ifdef STATEDEBUG
    digitalWrite(state, HIGH);
#endif
}
void check_time(void)
{
    int currentHour = Time.hour();
    if ((currentHour == PULL_TIME_1) && currentHour != oneShotGuard)
    {
        oneShotGuard = currentHour;
#ifdef STATEDEBUG
        digitalWrite(state, LOW);
#endif
        state = GET_DATA;
#ifdef STATEDEBUG
        digitalWrite(state, HIGH);
#endif
    }
    if ((currentHour == PULL_TIME_2) && currentHour != oneShotGuard2)
    {
        oneShotGuard2 = currentHour;

        state = ROTATE;
    }
}