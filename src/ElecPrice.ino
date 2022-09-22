#include "string.h"
#include "application.h"
#include "../lib/MQTT/src/MQTT.h"

#define DELTA_OFFSET 0.3
#define KW_SENSOR_PIN D6
#define WATT_CONVERSION_CONSTANT 3600000
#define HOST "192.168.0.103"
#define PORT 1883
#define SECONDS_TO_DAY 86400
#define PULL_TIME_1 23*3600 + 10*60
#define PULL_TIME_2 11*3600 + 10*60
#define MAX_RANGE 48

int oneShotGuard = -1;
double cost[MAX_RANGE];
int cost_hour[MAX_RANGE];
int date;
int range = MAX_RANGE;         // Max received count. Updated if received count is smaller
char temp[5*513];       // Create an array that can hold the entire transmission
byte rec_cnt;           // Counter to keep track of recieved transmissions
bool populate = false;  // Entire transmission received flag
bool work = false;      // Received price data
bool printer = false;
bool update_prices = false;

int low_range_hour[24];

int cnt;
int start_stop[12][2] = {0};

unsigned long last_read;
int calc_power;

// Send updated meassurement flag
bool transmit_value = false;

void calc_low(void);
void get_data(int day);
void handle_sensor(void);
void reconnect(void);

// Callback function for MQTT transmission
void callback(char* topic, byte* payload, unsigned int length);
// Create MQTT client
MQTT client("192.168.0.103", PORT, 512, 30, callback);
// Create sleep instance
SystemSleepConfiguration config;

void setup()
{
    last_read = millis(); //Give it an initial value
    pinMode(KW_SENSOR_PIN, INPUT_PULLDOWN);                 //Setup pinmode for LDR pin
    attachInterrupt(KW_SENSOR_PIN,handle_sensor,RISING);    //Attach interrup that will be called when rising
    
    // Subscribe to the integration response event
    Particle.subscribe("prices", myHandler, MY_DEVICES);
    Particle.subscribe("get_prices", myPriceHandler, MY_DEVICES);

    // Request data on power prices for the next 48 hours
    get_data(Time.day());


    // connect to the server(unique id by Time.now())
    Serial.printf("Return value: %d",client.connect("sparkclient_" + String(Time.now()),"mqtt","mqtt"));

    // publish/subscribe
    if (client.isConnected()) 
    {
        // Debugging publish
        client.publish("power/get","hello world");
        // Subscribe to 2 topics
        //client.subscribe("power/get");
        client.subscribe("power/prices");
    }

    // Setup low power mode
    config.mode(SystemSleepMode::ULTRA_LOW_POWER).gpio(KW_SENSOR_PIN, RISING).network(NETWORK_INTERFACE_ALL);
}

void callback(char* topic, byte* payload, unsigned int length) 
{
    /*
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    */

    work = true;
}

void handle_sensor(void)
{
    unsigned long delta;
    unsigned long current_reading = millis();
    
    if ((delta = current_reading-last_read) > 100)
    {
        calc_power = WATT_CONVERSION_CONSTANT / delta;
        last_read = current_reading;
        printer = true; // Just a debuging flag
        transmit_value = true;
    }
}

void myHandler(const char *event, const char *data)
{
    populate = false;
    rec_cnt++;

    /* When transmissions are greater than 512 bytes, it will be split into 512
     * byte parts. The final transmission part should therefore be less than 512.
     * Save transmission size into variable so we can act on it
    */
    int transmission_size = strlen(data);
    
    // "eventname/<transmission part no>"
    char event_str[12];
    strcpy(event_str,event);

    // Token used for strtok()
    char *token = NULL;
    // Extract the numbered part of eventname and use it for indexing "temp"
    strcat(&temp[atoi(strtok(event_str,"prices/"))*512],data);
    // If transmission size is less than 512 = last transmission received
    if (transmission_size < 512)
    {
        populate = true;
    }

    if (populate)
    {
        // Display what has been received
        Serial.printf("%s\n",temp);
        
        // Tokenize the string. i.e. split the string so we can get to the data
        token = strtok(temp, ",!");
        for (int i = 0; i < range; i++)
        {
            // Save hour and cost in differen containers
            sscanf(token, "%*d-%*d-%dT%d:%*d:%*d", &date, &cost_hour[i]);
            token = strtok(NULL, ",!");
            cost[i] = atof(token) / 1000;
            
            if((token = strtok(NULL, ",!")) == NULL) // Received data count is less than 48.
            {
                range = i;  // Update range, such that the rest of program flow is aware of size
                break;      // Break the while loop
            }
        }
    }
}

void myPriceHandler(const char *event, const char *data)
{
    update_prices = true;
}

void loop()
{
    if (client.isConnected())
    {
        client.loop();
    }
    else 
    {
        Serial.printf("Client disconnected\n");
        reconnect();
    }

    // Check what time it is
    int currentSecond = Time.local() % SECONDS_TO_DAY;
    if ((currentSecond == PULL_TIME_1 || currentSecond == PULL_TIME_2) && currentSecond != oneShotGuard)
    {
        oneShotGuard = currentSecond;
        update_prices = true;
    }

    // Is it time to update the prices or has it been requested?
    if (update_prices == true)
    {
        get_data(Time.day());
        update_prices = false;
    }
    
    if (printer) // Debugging flag set in interrupt handler
    {
        Serial.printf("Light: %d\n",calc_power);
        printer = false;
    }
    // Has the prices for today arrived?
    if (populate)
    {
        calc_low();
        Serial.printf("Current HH:MM: %02d:%02d\n", Time.hour() + 2, Time.minute());
    }

    if (work)
    {
        Serial.printf("In work\n");
        // Do some meaningfull work with the collected data
        String data = "Cheap(ish) hours of the day: ";
        for (int z = 0; z < cnt; z++)
        {
            data += String::format("%02d to %02d, ",start_stop[z][0],start_stop[z][1]);
        }
        // Publish the cheap hours to cloud
        Particle.publish("Low price hours", data, PRIVATE);
        // Publish cheap hour to MQTT
        client.publish("prices", data);
        work = false;
    }

    if (transmit_value) // Did we receive a request for updated values
    {
        Serial.printf("Received power/get\n");
        char values[16];
        sprintf(values,"%d", calc_power);
        client.publish("power",values);
        transmit_value = false;
    }
    // Wait 1 second
    delay(1000);
    //System.sleep(config);
}

void reconnect(void)
{
    client.connect("sparkclient_" + String(Time.now()),"mqtt","mqtt");
}

/** @brief The purpose of the function is to identify the hours at which the highest and lowest cost are.
 *  Furthermore neighbouring low cost hour are identified and saved in an array for easy presentation
*/
void calc_low(void)
{
    int idx = 0;

    double delta;
    double small_offset;
    double last_big = 0;
    double last_small = 100; // Assign any absurdly high value

    for (int i = 0; i < range; i++)
    {
        // Find the highest price in range
        if (cost[i] > last_big)
        {
            last_big = cost[i];
        }
        // Find the lowest price in range
        if (cost[i] < last_small)
        {
            last_small = cost[i];
        }
    }
    // Calculate delta
    delta = last_big - last_small;

    // Define low price area
    small_offset = last_small + delta * DELTA_OFFSET;
    
    // Find hours of day at which price is within the defined low price point
    for (int i = 0; i <= range; i++)
    {
        
        if (cost[i] < small_offset)
        {
            low_range_hour[idx] = cost_hour[i];
            
            idx++;
        }
    }

    // Calculations have been done - clear flag
    populate = false;
    // Display the results
    Serial.printf("Highest price of the day: %f\n", last_big);
    Serial.printf("Lowest price of the day: %f\n", last_small);
    Serial.printf("Hours of the day where electricity is within accepted range:\n");
    
    int i = 0;
    if (idx > 0)
    {
        while (i <= idx)
        {
            start_stop[cnt][0] = low_range_hour[i];

            while (low_range_hour[i] == low_range_hour[i + 1] - 1) // Hour only increased by 1. I.e. coherant
            {
                i++;
            }
            
            start_stop[cnt][1] = low_range_hour[i]+1;
            
            cnt++;
            i++;
        }
        cnt--;
    }
    for (int z = 0; z < cnt; z++)
    {
        Serial.printf("%02d to %02d\n",start_stop[z][0],start_stop[z][1]);
    }

    work = true;
}

/** @brief Puplishes a formatted command string to Particle cloud that fires off a webhook
 *  @param day
 */
void get_data(int day)
{
    rec_cnt = 0;
    range = 48;
    cnt = 0;
    temp[0] = 0;
    String data = String::format("{ \"year\": \"%d\", \"month\":\"%02d\", \"day\": \"%02d\", \"day_two\": \"%02d\", \"hour\": \"%02d\" }", Time.year(), Time.month(), day, day + 2, Time.hour());
    
    // Trigger the integration
    Particle.publish("elpriser", data, PRIVATE);
}