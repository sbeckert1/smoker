#include <iostream>
#include <RF24/RF24.h>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <thread>
#include <string>
#include <sstream>
#include <mosquitto.h>

using namespace std;

#define PIN_CE 17
#define PIN_CSN 8

using Clock = chrono::system_clock;
using TimePoint = chrono::time_point<Clock>;
TimePoint lastSampleTime = TimePoint();

uint8_t payloadSize = 21;
int interruptPin = 27;
RF24 radio(PIN_CE, PIN_CSN);

struct Sample 
{
  uint16_t temp1;
  uint16_t alarmMax1;
  uint16_t alarmMin1;
  uint16_t temp2;
  uint16_t alarmMax2;
  uint16_t alarmMin2;
  uint8_t alarm1On;
  uint8_t probe1;
  uint8_t alarm2On;
  uint8_t probe2;
  uint8_t tempIsFahrenheit;
  uint8_t something;
  uint8_t something2;
  uint8_t something3;
  uint8_t something4;
};

struct InterpretedSample
{
  TimePoint timestamp;
  double temp1_F;
  bool hasTemp1;
  double temp2_F;
  bool hasTemp2;

  InterpretedSample()
    : timestamp(TimePoint())
    , temp1_F(0)
    , hasTemp1(false)
    , temp2_F(0)
    , hasTemp2(false)
  {}
};

class MQTTSender
{
public:
  struct mosquitto *mosq;

  MQTTSender()
    : mosq(NULL)
  {}

  ~MQTTSender()
  {
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
  }

  int Init()
  {
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);
    if(!mosq)
    {
      std::cerr << "Failed mosquitto_new" << std::endl;
      return 1;
    }

    int rc = mosquitto_connect(mosq, "localhost", 1883, 60);
    if(rc>0)
    {
      std::cerr << "Failed to connect:" << rc << std::endl;
      return 1;
    }

    return 0;
  }

  static string FormatMessageJSON(const InterpretedSample& interpretedSample)
  {
    stringstream ss;
    
    time_t now = time(0);
    tm *ltm = localtime(&now);

    ss << "{\"date\":\"" << std::put_time(ltm, "%F") << "\"";
    ss << ", {\"timestamp\":\"" << std::put_time(ltm, "%s") << "\"";
    
    if(interpretedSample.hasTemp1)
    {
      ss << ", \"air\":";
      ss << interpretedSample.temp1_F;
    }
    
    if(interpretedSample.hasTemp2)
    {
      ss << ", \"meat\":";
      ss << dec << interpretedSample.temp2_F;
    }
  
    return ss.str();  
  }

  static string FormatMessage(const InterpretedSample& interpretedSample)
  {
    stringstream ss;
    
    if(interpretedSample.hasTemp1)
      ss << dec << interpretedSample.temp1_F;
    
    ss << ",";

    if(interpretedSample.hasTemp2)
      ss << dec << interpretedSample.temp2_F;
  
    return ss.str();  
  }

  void Publish(const InterpretedSample& interpretedSample)
  {
    //string msg = FormatMessage(interpretedSample);
    string msg = FormatMessageJSON(interpretedSample);

    int rc = mosquitto_publish(
      mosq,
      NULL,
      "smoker/temperature",
      msg.length(),
      msg.c_str(),
      0,
      false);
  
    if(rc>0)
    {
      std::cerr << "Failed to send:" << rc << std::endl;
    }
  }

};

MQTTSender Sender;

double convertToFahrenheit(double tempCel)
{
  return (tempCel * 9.0/5.0) + 32;
}

InterpretedSample parse(char* buffer, int len)
{
  Sample* sample = reinterpret_cast<Sample*>(buffer);

  InterpretedSample interpretedSample;
  interpretedSample.timestamp = Clock::now();
  interpretedSample.hasTemp1 = sample->probe1 ? false : true;
  interpretedSample.hasTemp2 = sample->probe2 ? false : true;

  if(interpretedSample.hasTemp1)
  {
    double temp = (double) sample->temp1 / 10.0;
    interpretedSample.temp1_F = sample->tempIsFahrenheit ? temp : convertToFahrenheit(temp);
  }

  if(interpretedSample.hasTemp2)
  {
    double temp = (double) sample->temp2 / 10.0;
    interpretedSample.temp2_F = sample->tempIsFahrenheit ? temp : convertToFahrenheit(temp);
  }

  return interpretedSample;
}

void intHandler()
{
  while(radio.available())
  {
    char payload[payloadSize];

    radio.read(&payload, payloadSize);

    InterpretedSample interpretedSample = parse(payload, payloadSize);

    const auto timeSinceLastSample = chrono::duration_cast<chrono::seconds>(interpretedSample.timestamp - lastSampleTime);
    if(timeSinceLastSample.count() > 10)
    {
      cout << dec << chrono::duration_cast<chrono::seconds>(interpretedSample.timestamp.time_since_epoch()).count();
      
      cout << ",";
      if(interpretedSample.hasTemp1)
        cout << dec << interpretedSample.temp1_F;
     
      cout << ",";
      if(interpretedSample.hasTemp2)
        cout << dec << interpretedSample.temp2_F;
      
      cout << endl;

      //for (uint8_t i = 0; i < payloadSize; i++)
      //{
        //cout << setw(2) << setfill('0') << hex << (int) payload[i] << " ";
      //}
      //cout << endl;
    
      lastSampleTime = interpretedSample.timestamp;

      Sender.Publish(interpretedSample);
    }
    
  }
}

int main()
{
  radio.begin();
  
  radio.setChannel(70);
  radio.setPayloadSize(payloadSize);
  radio.setCRCLength(RF24_CRC_16);
  radio.setAddressWidth(5);
  radio.setDataRate(RF24_250KBPS);
  radio.openReadingPipe(0, 0x03a40a11ebLL);
  radio.setAutoAck(false);

  radio.maskIRQ(true, true, false);
  attachInterrupt(interruptPin, INT_EDGE_FALLING, intHandler);

  if(Sender.Init())
    return 1;

  radio.startListening(); 

  while (true)
  {
    this_thread::sleep_for(5s);
  }

  return 0;
}
