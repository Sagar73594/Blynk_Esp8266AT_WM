/****************************************************************************************************************************
 * BlynkSimpleShieldEsp8266_SAMD.h
 * For SAMD boards using ESP8266 WiFi Shields
 *
 * Blynk_Esp8266AT_WM is a library for the Mega, Teensy and SAMD boards (https://github.com/khoih-prog/Blynk_Esp8266AT_WM)
 * to enable easy configuration/reconfiguration and autoconnect/autoreconnect of WiFi/Blynk
 * 
 * Forked from Blynk library v0.6.1 https://github.com/blynkkk/blynk-library/releases
 * Built by Khoi Hoang https://github.com/khoih-prog/Blynk_WM
 * Licensed under MIT license
 * Version: 1.0.2
 *
 * Original Blynk Library author:
 * @file       BlynkSimpleShieldEsp8266.h
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2015 Volodymyr Shymanskyy
 * @date       Jun 2015
 * @brief
 *
 * Version Modified By   Date        Comments
 * ------- -----------  ----------   -----------
 *  1.0.0   K Hoang      16/02/2020  Initial coding
 *  1.0.1   K Hoang      17/02/2019  Add checksum, fix bug
 *  1.0.2   K Hoang      22/02/2019  Add support to SAMD boards
 *****************************************************************************************************************************/

#ifndef BlynkSimpleShieldEsp8266_SAMD_h
#define BlynkSimpleShieldEsp8266_SAMD_h

#if ( defined(ESP8266) || defined(ESP32) || defined(ARDUINO_AVR_MEGA2560) || defined(ARDUINO_AVR_MEGA) )
#error This code is not intended to run on the ESP8266, ESP32 nor AVR platform! Please check your Tools->Board setting.
#endif

#if    ( defined(ARDUINO_SAM_DUE) || defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_SAMD_MKR1000) || defined(ARDUINO_SAMD_MKRWIFI1010) \
      || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_SAMD_MKRFox1200) || defined(ARDUINO_SAMD_MKRWAN1300) || defined(ARDUINO_SAMD_MKRWAN1310) \
      || defined(ARDUINO_SAMD_MKRGSM1400) || defined(ARDUINO_SAMD_MKRNB1500) || defined(ARDUINO_SAMD_MKRVIDOR4000) || defined(__SAMD21G18A__) \
      || defined(ARDUINO_SAMD_CIRCUITPLAYGROUND_EXPRESS) || defined(__SAM3X8E__) || defined(__CPU_ARC__) )      
  #if defined(ESP8266_AT_USE_SAMD)
    #undef BLYNK_ESP8266_AT_USE_SAMD
  #endif
  #define BLYNK_ESP8266_AT_USE_SAMD      true
#endif

#ifndef BLYNK_INFO_CONNECTION
#define BLYNK_INFO_CONNECTION  "ESP8266"
#endif

#ifndef BLYNK_ESP8266_MUX
#define BLYNK_ESP8266_MUX  1
#endif

#define BLYNK_SEND_ATOMIC
#define BLYNK_SEND_CHUNK 40

#include <BlynkApiArduino.h>
#include <Blynk/BlynkProtocol.h>
#include <utility/BlynkFifo.h>
#include <ESP8266_Lib.h>

#define SIMPLE_SHIELD_ESP8266_DEBUG       1

class BlynkTransportShieldEsp8266
{
    static void onData(uint8_t mux_id, uint32_t len, void* ptr) {
        ((BlynkTransportShieldEsp8266*)ptr)->onData(mux_id, len);
    }

    void onData(uint8_t mux_id, uint32_t len) {
        if (mux_id != BLYNK_ESP8266_MUX) {
            return;
        }
        
        //KH
        #if (SIMPLE_SHIELD_ESP8266_DEBUG > 1)
          BLYNK_LOG4("Got: ", len, ", Free: ", buffer.free());
        #endif
        //
        
        if ( (uint32_t) buffer.free() < len) 
        {
          //KH
          #if (SIMPLE_SHIELD_ESP8266_DEBUG > 0)
            BLYNK_LOG4("Got: ", len, ", Free: ", buffer.free());
          #endif
          BLYNK_LOG1(BLYNK_F("Buffer overflow"));
          return;
        }
        while (len) {
            if (client->getUart()->available()) {
                uint8_t b = client->getUart()->read();
                //KH
                // len got from +IPD data
                buffer.put(b);
                //
                len--;
            }
        }
        //KH
        #if (SIMPLE_SHIELD_ESP8266_DEBUG > 1)
          BLYNK_LOG2(BLYNK_F("onData Buffer len"), len );
        #endif
        //
    }

public:
    BlynkTransportShieldEsp8266()
        : client(NULL)
        , status(false)
        , domain(NULL)
        , port(0)
    {}

    void setEsp8266(ESP8266* esp8266) {
        client = esp8266;
        client->setOnData(onData, this);
    }

    //TODO: IPAddress

    void begin(const char* d,  uint16_t p) {
        domain = d;
        port = p;
    }

    bool connect() {
        if (!domain || !port)
            return false;
        status = client->createTCP(BLYNK_ESP8266_MUX, domain, port);
        return status;
    }

    void disconnect() {
        status = false;
        buffer.clear();
        client->releaseTCP(BLYNK_ESP8266_MUX);
    }

    size_t read(void* buf, size_t len) 
    {
        millis_time_t start = BlynkMillis();
        //KH
        //buffer.size() is number of bytes currently still in FIFO buffer
        //Check to see if all data are read or not
        
        #if (SIMPLE_SHIELD_ESP8266_DEBUG > 1)
          BLYNK_LOG4("read: Data len: ", len, " Buffer: ", buffer.size());
        #endif
        
        while ((buffer.size() < len) && (BlynkMillis() - start < 1500)) 
        {
            // Actually call ESP8266 run/rx_empty (read and locate +IPD, know data len, 
            // then call onData() to get len bytes of data to buffer => BlynkProtocol::ProcessInput()
            client->run();
        }
        //KH Mod
        #if 0
        // Check to see if got sequence [14|0D|0A]+IPD,<id>,<len>:<data> => discard until end of <data>
        
        /*static*/ uint8_t tempBuff[128];
        if (len < sizeof(tempBuff))
        {
          buffer.get(tempBuff, len);
          //tempBuff[len] = 0;
        }
        
        char testString1[5] = { 0x0D, 0x0A, '+', 'I', 0};
        char testString2[6] = { '+', 'I', 'P', 'D', ',', 0 };
        char testString3[3] = { 'P', 'D', 0 };
        char testString4[3] = { 0x0D, 0x0A, 0 };
        
        /*static*/ uint8_t localBuff[128];
        /*static*/ unsigned int localLength;
        /*static*/ unsigned int copyLength;
        
        /*+IPD,<id>,<len>:<data>*/
        if ( strstr((char*) tempBuff, testString1) /*|| strstr((char*) tempBuff, testString2) || strstr((char*) tempBuff, testString3)*/ )
        {
          //Got +I, next => PD,<id>,<len>:<data>
          //                012  3 4  5  6  7
          BLYNK_LOG2("read: Bad sequence a ", (char*) tempBuff);
          // Read and discard more
          if (buffer.size() < sizeof(tempBuff))
          {
            buffer.get(tempBuff, buffer.size());
            
            if ( (tempBuff[0] == 'P') && (tempBuff[1] == 'D') && (tempBuff[2] == ',') /*&& (tempBuff[3] == '1') && (tempBuff[4] == ',')*/ )
            {
              BLYNK_LOG2("read: Bad sequence2 ", (char*) tempBuff);
              localLength = tempBuff[5];
              copyLength = localLength < sizeof(localBuff)? localLength : sizeof(localBuff);
              memcpy(localBuff, tempBuff + 7, copyLength );
              memcpy(tempBuff, localBuff, copyLength);
              BLYNK_LOG2("read: Bad sequence3 ", (char*) tempBuff);
              return copyLength;
            }
          }
          
          return 0;
        }
        else if ( strstr((char*) tempBuff, testString2) /*|| strstr((char*) tempBuff, testString3)*/ )
        {
          //Got +IPD, next => <id>,<len>:<data>
          //                    0 1  2  3  4
          BLYNK_LOG2("read: Bad sequence b ", (char*) tempBuff);
          // Read and discard more
          if (buffer.size() < sizeof(tempBuff))
          {
            buffer.get(tempBuff, buffer.size());
            
            if ( (tempBuff[1] == ',') && (tempBuff[3] == ':') )
            {
              BLYNK_LOG2("read: Bad sequence4 ", (char*) tempBuff);
              localLength = tempBuff[2];
              copyLength = localLength < sizeof(localBuff)? localLength : sizeof(localBuff);
              memcpy(localBuff, tempBuff + 4, copyLength );
              memcpy(tempBuff, localBuff, copyLength);
              BLYNK_LOG2("read: Bad sequence5 ", (char*) tempBuff);
              return copyLength;
            }
          }
          
          return 0;
        }     
        else if ( strstr((char*) tempBuff, testString4) /*|| strstr((char*) tempBuff, testString3)*/ )
        {
          //Got X X X 0x0D, 0x0A, next +IPD, just discard this 5 bytes
          //    0 1 2  3     4
          BLYNK_LOG2("read: Bad sequence c ", (char*) tempBuff);
          // Read and discard more
          if (buffer.size() < sizeof(tempBuff))
          {
            buffer.get(tempBuff, buffer.size());
            
            //if ( (tempBuff[1] == ',') && (tempBuff[3] == ':') )
            {
              //BLYNK_LOG2("read: Bad sequence4 ", (char*) tempBuff);
              //localLength = tempBuff[2];
              //copyLength = localLength < sizeof(localBuff)? localLength : sizeof(localBuff);
              memcpy(localBuff, tempBuff + 5, sizeof(tempBuff) - 5 );
              memcpy(tempBuff, localBuff, sizeof(tempBuff) - 5);
              BLYNK_LOG2("read: Bad sequence5 ", (char*) tempBuff);
              return 0;
            }
          }
          
          return 0;
        }                
        else
        {
          memcpy(buf, tempBuff, len);
          return len;
        }
        
        #else
        //All data got in FIFO buffer, copy to destination buf and return len
        return buffer.get((uint8_t*)buf, len);
        #endif
    }
    
    size_t write(const void* buf, size_t len) {
        if (client->send(BLYNK_ESP8266_MUX, (const uint8_t*)buf, len)) {
            return len;
        }
        return 0;
    }

    bool connected() { return status; }

    int available() 
    {
        client->run();
        #if (SIMPLE_SHIELD_ESP8266_DEBUG > 2)
          BLYNK_LOG2("Still: ", buffer.size());
        #endif
        return buffer.size();
    }

private:
    ESP8266* client;
    bool status;

    //KH   
    #if (BLYNK_ESP8266_AT_USE_SAMD)
      // For NANO_33_IOT, MKRFox1200, MKRWAN1300, MKRWAN1310, MKRGSM1400, MKRNB1500, MKRVIDOR4000.
      BlynkFifo<uint8_t, 4096> buffer;
      #warning Board SAMD uses 4k FIFO buffer        
    #else
      // For other boards
      //BlynkFifo<uint8_t,256> buffer;
      // For MeGa 2560 or 1280
      BlynkFifo<uint8_t,512> buffer;  
      #warning Not SAMD board => uses 512bytes FIFO buffer    
    #endif
    
    const char* domain;
    uint16_t    port;
};

class BlynkWifi
    : public BlynkProtocol<BlynkTransportShieldEsp8266>
{
    typedef BlynkProtocol<BlynkTransportShieldEsp8266> Base;
public:
    BlynkWifi(BlynkTransportShieldEsp8266& transp)
        : Base(transp)
        , wifi(NULL)
    {}

    bool connectWiFi(const char* ssid, const char* pass)
    {
        //BlynkDelay(500);
        BLYNK_LOG2(BLYNK_F("Connecting to "), ssid);
        #if 1
        if (!wifi->restart()) {
            BLYNK_LOG1(BLYNK_F("Failed to restart"));
            return false;
        }
        #endif
        if (!wifi->kick()) 
        {
             BLYNK_LOG1(BLYNK_F("ESP is not responding"));
             //TODO: BLYNK_LOG_TROUBLE(BLYNK_F("esp8266-not-responding"));
             return false;
        }
        if (!wifi->setEcho(0)) 
        {
            BLYNK_LOG1(BLYNK_F("Failed to disable Echo"));
            return false;
        }
        
        String ver = wifi->ESP8266::getVersion();
        BLYNK_LOG1(ver);
                   
        // KH
        BlynkDelay(500);
        
        if (!wifi->enableMUX()) 
        {
            BLYNK_LOG1(BLYNK_F("Failed to enable MUX"));
        }
        
        if (!wifi->setOprToStation()) 
        {
            BLYNK_LOG1(BLYNK_F("Failed to set STA mode"));
            return false;
        }
        
        if (wifi->joinAP(ssid, pass)) 
        {
            getLocalIP();
        } 
        else 
        {
            BLYNK_LOG1(BLYNK_F("Failed to connect WiFi"));
            return false;
        }
        
        BLYNK_LOG1(BLYNK_F("Connected to WiFi"));
        return true;
    }

    void config(ESP8266&    esp8266,
                const char* auth,
                const char* domain = BLYNK_DEFAULT_DOMAIN,
                uint16_t    port   = BLYNK_DEFAULT_PORT)
    {
        Base::begin(auth);
        wifi = &esp8266;
        this->conn.setEsp8266(wifi);
        this->conn.begin(domain, port);
    }

    void begin(const char* auth,
               ESP8266&    esp8266,
               const char* ssid,
               const char* pass,
               const char* domain = BLYNK_DEFAULT_DOMAIN,
               uint16_t    port   = BLYNK_DEFAULT_PORT)
    {
        config(esp8266, auth, domain, port);
        connectWiFi(ssid, pass);
        while(this->connect() != true) {}
    }

    String getLocalIP(void)
    {   
      // Check to use getStationIp()
      #if 1
      ipAddress = wifi->getStationIp();
      #else
      ipAddress = wifi->getLocalIP();
      ipAddress.replace("+CIFSR:STAIP,\"", "");
      ipAddress.replace("\"", "");     
      #endif
      
      BLYNK_LOG2(BLYNK_F("IP = "), ipAddress);
      return ipAddress;
    }
    
private:
    ESP8266* wifi;
    String ipAddress = "0.0.0.0";
};

static BlynkTransportShieldEsp8266 _blynkTransport;
BlynkWifi Blynk(_blynkTransport);

#include <BlynkWidgets.h>

#endif