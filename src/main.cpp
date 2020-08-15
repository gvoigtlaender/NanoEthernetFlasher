#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>

#define VERSION "0.3.0.0"

#define DHCP_IN A0
#define USE_UDP 1
#define USE_TCP 1

#if defined ARDUINO_AVR_NANO
#define FLASH_STRING(str) F(str)
#else
#define FLASH_STRING(str) (str)
#endif

#if USE_W5500 == 1
#if defined ESP8266
#include <Ethernet2.h>
#else
#include <Ethernet.h>
#endif
#if USE_UDP == 1
#include <EthernetUdp.h>
EthernetUDP Udp;
char packetBuffer[2]; // buffer to hold incoming packet,
char ReplyBuffer[1];  // a string to send back
#endif
#endif

#if _LOG_DEBUG == 1
  #define _DLOG(s)    Serial.print(s)
  #define _DLOGLN(s)  Serial.println(s)
  #define _WLOG(s)    Serial.print(s)
  #define _WLOGLN(s)  Serial.println(s)
#else
  #define _DLOG(s)
  #define _DLOGLN(s)
  #define _WLOG(s)    Serial.print(s)
  #define _WLOGLN(s)  Serial.println(s)
#endif

#if USE_ENC28J60 == 1
#include <UIPEthernet.h>
#if USE_UDP == 1
EthernetUDP Udp;
char packetBuffer[2]; // buffer to hold incoming packet,
char ReplyBuffer[1];  // a string to send back
#endif
#endif

byte mac[] = {
    0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};

unsigned long nMS = 0;
class CTimer
{
public:
  CTimer()
      : m_nEndTime(0)
  {
  }
  void Start(unsigned long nDelayS)
  {
    m_nEndTime = nMS + nDelayS * 1000;
#if _LOG_DEBUG == 1
    if (ms_bLog)
    {
      // _DLOG(m_sName);
      _DLOG(FLASH_STRING("->Start("));
      _DLOG(nDelayS);
      _DLOG(FLASH_STRING("s) => "));
      _DLOG(GetTimeToRun() / 1000.0);
      _DLOG(FLASH_STRING("s) => "));
      _DLOG(GetTimeToRunString());
      _DLOG(FLASH_STRING("\n"));
    }
#endif
  }
  void StartMs(unsigned long nDelayMS)
  {
    m_nEndTime = nMS + nDelayMS;
#if _LOG_DEBUG == 1
    if (ms_bLog)
    {
      // _DLOG(m_sName);
      _DLOG(FLASH_STRING("->Start("));
      _DLOG(nDelayMS);
      _DLOG(FLASH_STRING("ms) => "));
      _DLOG(GetTimeToRun() / 1000.0);
      _DLOG(FLASH_STRING("s) => "));
      //_DLOG(GetTimeToRunString());
      _DLOG(FLASH_STRING("\n"));
    }
#endif
  }
  bool IsExpired()
  {
    if (m_nEndTime <= nMS)
    {
#if _LOG_DEBUG == 1
      if (ms_bLog)
      {
        // DLOG(m_sName);
        _DLOG(FLASH_STRING("->IsExpired() = true\n"));
      }
#endif
      return true;
    }
    return false;
  }
  unsigned long GetTimeToRun()
  {
    return (m_nEndTime - nMS);
  }
  const String GetTimeToRunString() { return "---"; }
  // String m_sName;
  unsigned long m_nEndTime;
  static bool ms_bLog;
  // char m_szTTR[32];
};
/*static*/ bool CTimer::ms_bLog = true;

class CFlashChannel
{
public:
  CFlashChannel()
      : m_nChannel(0), m_nPin(0), m_Timer(), m_bActive(false), m_sName("")
  {
  }

  void Setup(int nChannel, int nPin)
  {
    char szTmp[5];
    sprintf(szTmp, "LED%d", nChannel);
    m_sName = szTmp;

    m_nChannel = nChannel;
    m_nPin = nPin;
    pinMode(nPin, OUTPUT);
  }

  void Trigger(unsigned long nDelayMS = 5)
  {
    digitalWrite(m_nPin, HIGH);
    m_Timer.StartMs(nDelayMS);
    m_bActive = true;
#if _LOG_DEBUG == 1
    _DLOG(m_sName);
    _DLOG(FLASH_STRING("->Trigger("));
    _DLOG(nDelayMS);
    _DLOG(FLASH_STRING(")\n"));
#endif
  }

  void Control()
  {
    if (!m_bActive)
      return;

    if (m_Timer.IsExpired())
      Reset();
  }

  void set(bool bOn)
  {
    digitalWrite(m_nPin, bOn);
  }

protected:
  int m_nChannel;
  int m_nPin;
  CTimer m_Timer;
  bool m_bActive;
  String m_sName;

  void Reset()
  {
    digitalWrite(m_nPin, LOW);
    m_bActive = false;
#if _LOG_DEBUG == 1
    _DLOG(m_sName);
    _DLOG(FLASH_STRING("->Reset("));
    _DLOG(FLASH_STRING(")\n"));
#endif
  }
};

#if defined ESP8266 
 #define CHANNELS 4
#else
 #define CHANNELS 8
#endif
CFlashChannel aLEDs[CHANNELS];

int nPort = 8888;

class CServer
{
public:
  CServer(int nPort, const char *szName)
      : m_nPort(nPort), m_sName(szName), m_nChannel(0), m_nDelay(0)
  {
  }

  virtual void Begin() = 0;

  void Control()
  {
    int nSize = HasData();
    if (nSize == 0)
      return;
#if _LOG_DEBUG == 1
    _WLOG(m_sName);
    _WLOG(FLASH_STRING("::Recv "));
    _WLOG(nSize);
    _WLOGLN(FLASH_STRING(" bytes"));
#endif
    if (nSize < 2)
      return;
    String sRsp = "";
    while (nSize >= 2)
    {

      ReadData();
      m_nChannel = (uint8_t)cMsg[0];
      m_nDelay = (uint8_t)cMsg[1];
      sRsp += (char)m_nChannel;

      if (m_nChannel < CHANNELS)
        aLEDs[m_nChannel].Trigger(m_nDelay);
      nSize -= 2;
    }

    _WLOG(m_sName);
    _WLOG(FLASH_STRING("::Send "));
    _WLOG(sRsp.length());
    _WLOG(FLASH_STRING(" bytes, success="));
    bool bSuccess = WriteResponse(sRsp);
    _WLOGLN(bSuccess);
  }

  virtual int HasData() = 0;
  virtual bool ReadData() = 0;
  virtual bool WriteResponse(String &sRsp) = 0;

protected:
  int m_nPort;
  String m_sName;
  uint8_t m_nChannel;
  uint8_t m_nDelay;
  char cMsg[2];
};

#if USE_TCP == 1
class CServerTcp : public CServer
{
public:
  CServerTcp(int nPort)
      : CServer(nPort, "CServerTcp"), m_pServer(NULL), client()
  {
  }

  void Begin() override
  {
    _DLOG(FLASH_STRING("Starting TCP server on port "));
    _DLOG(m_nPort);
#if USE_ENC28J60 == 1
    server.begin(m_nPort);
#endif
#if USE_W5500 == 1 
    m_pServer = new EthernetServer(m_nPort);
    m_pServer->begin();
#endif
    _DLOGLN(FLASH_STRING(" ... done"));
  }
  int HasData() override
  {
    client = m_pServer->available();
    if (client)
      return client.available();
    return 0;
  }
  bool ReadData() override
  {
    client.read((uint8_t*)cMsg, 2);
    return true;
  }
  bool WriteResponse(String &sRsp) override
  {
    return (m_pServer->write(sRsp.c_str(), sRsp.length()) == sRsp.length() );
  }

protected:
  EthernetServer* m_pServer;
  EthernetClient client;
};
CServerTcp ServerTcp(nPort);
#endif

#if USE_UDP == 1
class CServerUdp : public CServer
{
public:
  CServerUdp(int nPort)
      : CServer(nPort, "CServerUdp"), Udp()
  {
  }

  void Begin() override
  {
    _DLOG(FLASH_STRING("Starting UDP on port "));
    _DLOG(m_nPort);
    Udp.begin(m_nPort);
    _DLOGLN(FLASH_STRING(" ... done"));
  }

  int HasData() override
  {
    return Udp.parsePacket();
  }

  bool ReadData() override
  {
    Udp.read(cMsg, 2);
    return true;
  }

  bool WriteResponse(String &sRsp) override
  {
    Udp.flush();

    int success;
    do
    {
      success = Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    } while (!success);

    success = Udp.print(sRsp);
    success = Udp.endPacket();
    return success == 1;
  }

protected:
  EthernetUDP Udp;
  char packetBuffer[2]; // buffer to hold incoming packet,
  char ReplyBuffer[1];  // a string to send back
};
CServerUdp ServerUdp(nPort + 1);
#endif

#include <EEPROM.h>
/*
char szIP[16] = "";
void EEPROM_ReadString(char *szString, size_t nSize, size_t nOffset)
{
  memset(szString, 0, nSize);
  unsigned int n;
  for (n = 0; n < nSize; n++)
  {
    szString[n] = (char)EEPROM.read(nOffset + n);
    if (szString[n] == 0x00)
      break;
  }
}

void EEPROM_WriteString(char *szString, size_t nSize, size_t nOffset)
{
  unsigned int n;
  for (n = 0; n < nSize; n++)
  {
    EEPROM.update(nOffset + n, szString[n]);
    if (szString[n] == 0x00)
      break;
  }
}
*/

IPAddress oIP(0,0,0,0);
bool EEPROM_ReadString(IPAddress& rIP, size_t nOffset)
{
  rIP = INADDR_NONE;
  unsigned int n;
  for (n = 0; n < 4; n++)
  {
    rIP[n] = (char)EEPROM.read(nOffset + n);
    if (n == 0 && rIP[n] == 0x00)
      return false;
  }
  return true;
}

bool EEPROM_WriteString(IPAddress& rIP, size_t nOffset)
{
  unsigned int n;
  for (n = 0; n < 4; n++)
  {
#if defined ARDUINO_AVR_NANO
    EEPROM.update(nOffset + n, rIP[n]);
#else
    EEPROM.write(nOffset + n, rIP[n]);
    EEPROM.commit();
#endif
    if (n == 0 && rIP[n] == 0x00)
      return false;
  }
  return true;
}

void setup()
{
  Serial.begin(57600);
  while (!Serial)
  {
    ;
  }
  _WLOG(FLASH_STRING("NanoEthernetFlasher, FW: "));
  _WLOGLN(VERSION);

  delay(100);

  _DLOG(FLASH_STRING("Initialize LED Channels"));
  for (int n = 0; n < CHANNELS; n++)
  {
    aLEDs[n].Setup(n + 1, 2 + n);
  }
  _DLOGLN(FLASH_STRING(" ... done"));

  _DLOG(FLASH_STRING("Sweep LED Channels"));
  for (int i = 0; i < CHANNELS; i++)
  {
    for (int n = 0; n < CHANNELS; n++)
    {
      aLEDs[n].set(i == n);
    }
    delay(20);
  }
  for (int i = 0; i < CHANNELS; i++)
  {
    for (int n = 0; n < CHANNELS; n++)
    {
      aLEDs[n].set((CHANNELS - i - 1) == n);
    }
    delay(20);
  }
  for (int n = 0; n < CHANNELS; n++)
  {
    aLEDs[n].set(false);
  }
  _DLOGLN(FLASH_STRING(" ... done"));

  bool bDhcp = false;

  IPAddress ip;
  IPAddress gw;
  IPAddress ds;
  IPAddress sn;

  if ( !EEPROM_ReadString(oIP, 0) ) {
    _WLOGLN(FLASH_STRING("No IP address set in EEPROM, using dhcp"));
    bDhcp = true;
  }
  else {
    _WLOG(FLASH_STRING("EEPROM IP: "));
    oIP.printTo(Serial);
    _WLOGLN();
    if ( oIP[3] != 0 ) {
      ip = oIP;
      gw = ip; gw[3] = 1;
      ds = gw;
      sn.fromString("255.255.0.0");
      bDhcp = false;
    } else
    {
      _WLOGLN(FLASH_STRING("Decoding IP from EEPROM failed, using DHCP"));
      bDhcp = true;
    }
    
  }


  Ethernet.init(10); // Most Arduino shields

#if USE_W5500 == 1
#if defined ARDUINO_AVR_NANO
  switch (Ethernet.hardwareStatus())
  {
  case EthernetNoHardware:
    _WLOGLN("EthernetNoHardware");
    break;
  case EthernetW5100:
    _WLOGLN("EthernetW5100");
    break;
  case EthernetW5200:
    _WLOGLN("EthernetW5200");
    break;
  case EthernetW5500:
    _WLOGLN("EthernetW5500");
    break;
  default:
    _WLOGLN("No Hardware found");
    break;
  }
#endif
  // start the Ethernet connection:
  if (!bDhcp)
  {
    _WLOGLN("Using static IP");
    Ethernet.begin(mac, ip, gw, sn);
  }
  else
  {
    _WLOGLN("Initialize Ethernet with DHCP:");
    if (Ethernet.begin(mac) == 0)
    {
      _WLOGLN("Failed to configure Ethernet using DHCP");
#if defined ARDUINO_AVR_NANO
      if (Ethernet.hardwareStatus() == EthernetNoHardware)
      {
        _WLOGLN("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      }
      else if (Ethernet.linkStatus() == LinkOFF)
      {
        _WLOGLN("Ethernet cable is not connected.");
      }
#endif
      // no point in carrying on, so do nothing forevermore:
      while (true)
      {
        delay(1);
      }
    }
  }
#endif

#if USE_ENC28J60 == 1
  if (!bDhcp)
  {
    WLOGLN(FLASH_STRING("Using static IP"));
    Ethernet.begin(mac, ip, gw, sn);
  }
  else
  {
    WLOGLN(FLASH_STRING("Initialize Ethernet with DHCP:"));
    if (Ethernet.begin(mac) == 0)
    {
      WLOGLN(FLASH_STRING("Failed to configure Ethernet using DHCP"));
#if defined ARDUINO_AVR_NANO
       if (Ethernet.hardwareStatus() == EthernetNoHardware) {
         LOGLN("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
       } else if (Ethernet.linkStatus() == LinkOFF) {
         LOGLN("Ethernet cable is not connected.");
       }
#endif
      // no point in carrying on, so do nothing forevermore:
      while (true)
      {
        delay(1);
      }
    }
  }
#endif

#if defined ARDUINO_AVR_NANO
  switch (Ethernet.linkStatus())
  {
  case LinkON:
    _WLOGLN("LinkON");
    break;
  case LinkOFF:
    _WLOGLN("LinkOFF");
    break;
  case Unknown:
  default:
    _WLOGLN("No Hardware found");
    break;
  }
#endif
  // print your local IP address:
  _WLOG(FLASH_STRING("My IP address: "));
  _WLOGLN(Ethernet.localIP());

  // start listening for clients
#if USE_TCP == 1
  ServerTcp.Begin();
#endif

#if USE_UDP == 1
  ServerUdp.Begin();
#endif
}

void(* resetFunc) (void) = 0;

String sComRcv;
void loop() {
  // put your main code here, to run repeatedly:
  nMS = millis();

#if USE_TCP == 1
  ServerTcp.Control();
#endif

#if USE_UDP == 1
  ServerUdp.Control();
#endif
  for (int n = 0; n < CHANNELS; n++) {
    aLEDs[n].Control();
  }

  while ( Serial.available() > 0 ) {
    char c = (char)Serial.read();
    if ( c != '\r' && c != '\n' ) {
      sComRcv += c;
    } else {
      _WLOGLN(sComRcv);
      delay(20);
      if  ( sComRcv == "ip?" ) {
        _DLOG("Get IP: ");
        if ( oIP[3] == 0x00 ) {
          _DLOGLN("DHCP");
        } else {
          #if _LOG_DEBUG == 1
          oIP.printTo(Serial);
          #endif
        }
        delay(20);
      } else if ( sComRcv.startsWith("ip=dhcp") ) {
        //memset(szIP, 0, 16);
        //EEPROM_WriteString(szIP, 16, 0);
        oIP = INADDR_NONE;
        EEPROM_WriteString(oIP, 0);
        _WLOGLN("Reset device now");
        delay(20);
        resetFunc();
      } else if ( sComRcv.startsWith("ip=") ) {
        String sIp = sComRcv.substring(3);
        _DLOG("IP=");
        if ( oIP[3] == 0x00 ) {
          _DLOGLN("DHCP");
        } else {
          // _DLOGLN(sIp);
          oIP.printTo(Serial);
        }
        delay(20);

        IPAddress test;
        if ( test.fromString(sIp) ) {
          _WLOG("Set IP: ");
          _DLOGLN(sIp);
          delay(20);
          // strcpy(szIP, sIp.c_str());
          // EEPROM_WriteString(szIP, 16, 0);
          oIP.fromString(sIp);
          EEPROM_WriteString(oIP, 0);
          _WLOGLN("Reset device now");
          delay(20);
          resetFunc();
        } else {
          
        }
      }
      sComRcv = "";
      Serial.flush();
    }
  }

}
