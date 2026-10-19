#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <map>
class String {
public:
 std::string s;
 String() = default;
 String(const char* v):s(v){}
 String(std::string v):s(v){}
 const char* c_str() const {return s.c_str();}
 size_t length() const {return s.size();}
 bool isEmpty() const {return s.empty();}
 char charAt(size_t n) const {return n<s.size()?s[n]:0;}
 char operator[](size_t n) const {return charAt(n);}
 String& operator+=(char c) {s+=c;return *this;}
 int indexOf(char c) const {auto n=s.find(c);return n==s.npos?-1:int(n);}
 void remove(size_t pos,size_t n=std::string::npos) {s.erase(pos,n);}
 String substring(size_t a,size_t b) const {return s.substr(a,b-a);}
 friend String operator+(const char* a,const String& b) {return std::string(a)+b.s;}
};
static uint32_t clockMs=0;
inline uint32_t millis(){return clockMs;}
inline uint32_t esp_random(){return 765432;}
inline void delay(unsigned n){clockMs+=n;}
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE=65535;
constexpr int BLE_HS_IO_DISPLAY_ONLY=0, BLE_SM_PAIR_KEY_DIST_ENC=1, BLE_SM_PAIR_KEY_DIST_ID=2;
struct ble_gap_conn_desc {uint16_t conn_handle; struct {bool encrypted=false;bool authenticated=false;} sec_state;};
static std::map<uint16_t,ble_gap_conn_desc> peers;
inline int ble_gap_conn_find(uint16_t h,ble_gap_conn_desc* d){if(!peers.count(h))return -1;*d=peers[h];return 0;}
class NimBLEServer;
class NimBLECharacteristic;
class NimBLEServerCallbacks {
public:
 virtual ~NimBLEServerCallbacks()=default;
 virtual uint32_t onPassKeyRequest(){return 0;}
 virtual void onConnect(NimBLEServer*,ble_gap_conn_desc*){}
 virtual void onDisconnect(NimBLEServer*,ble_gap_conn_desc*){}
 virtual void onAuthenticationComplete(ble_gap_conn_desc*){}
};
class NimBLECharacteristicCallbacks {
public: virtual ~NimBLECharacteristicCallbacks()=default;
 virtual void onWrite(NimBLECharacteristic*,ble_gap_conn_desc*){}
};
namespace NIMBLE_PROPERTY {enum {READ=1,NOTIFY=2,READ_ENC=4,READ_AUTHEN=8,WRITE=16,WRITE_NR=32,WRITE_ENC=64,WRITE_AUTHEN=128};}
class NimBLECharacteristic {
public:
 unsigned props;std::string value;unsigned notifications=0;
 NimBLECharacteristicCallbacks* cb=nullptr;
 explicit NimBLECharacteristic(unsigned p):props(p){}
 std::string getValue(){return value;}
 void setValue(uint8_t* d,size_t n){value.assign(reinterpret_cast<char*>(d),n);}
 void notify(){++notifications;}
 void setCallbacks(NimBLECharacteristicCallbacks* c){cb=c;}
};
class BLEAdvertising {public: void addServiceUUID(const char*){} void setScanResponse(bool){} void setMinPreferred(int){} void setMaxPreferred(int){} void start(){} };
class BLEService {public: NimBLECharacteristic* createCharacteristic(const char*,unsigned p){return new NimBLECharacteristic(p);} void start(){} };
class NimBLEServer {public:
 NimBLEServerCallbacks* cb=nullptr;BLEAdvertising adv;std::vector<uint16_t> disconnected;
 uint16_t getPeerMTU(uint16_t){return 23;}
 void setCallbacks(NimBLEServerCallbacks* c){cb=c;}
 BLEService* createService(const char*){return new BLEService;}
 BLEAdvertising* getAdvertising(){return &adv;}
 void startAdvertising(){}
 void disconnect(uint16_t h){disconnected.push_back(h);}
};
using BLEServer=NimBLEServer;using BLECharacteristic=NimBLECharacteristic;
class NimBLEDevice {public:
 static bool bonding,mitm,sc;static unsigned cleared;
 static void init(const char*){} static void deinit(){}
 static NimBLEServer* createServer(){return new NimBLEServer;}
 static BLEAdvertising* getAdvertising(){static BLEAdvertising a;return &a;}
 static void setMTU(uint16_t){}
 static int startSecurity(uint16_t){return 0;}
 static void setSecurityPasskey(unsigned){}
 static void setSecurityAuth(bool b,bool m,bool c){bonding=b;mitm=m;sc=c;}
 static void setSecurityIOCap(int){} static void setSecurityInitKey(int){} static void setSecurityRespKey(int){}
 static void deleteAllBonds(){++cleared;}
};
inline bool NimBLEDevice::bonding=false,NimBLEDevice::mitm=false,NimBLEDevice::sc=false;
inline unsigned NimBLEDevice::cleared=0;
using BLEDevice=NimBLEDevice;
struct Queue {size_t cap,size;std::deque<std::vector<uint8_t>> items;};
using QueueHandle_t=Queue*;
constexpr int pdTRUE=1;
inline QueueHandle_t xQueueCreate(size_t n,size_t sz){return new Queue{n,sz,{}};}
inline int xQueueSend(Queue* q,const void* p,int){if(q->items.size()==q->cap)return 0;auto b=static_cast<const uint8_t*>(p);q->items.emplace_back(b,b+q->size);return 1;}
inline int xQueueReceive(Queue* q,void* p,int){if(q->items.empty())return 0;memcpy(p,q->items.front().data(),q->size);q->items.pop_front();return 1;}
inline void xQueueReset(Queue* q){q->items.clear();}
struct Configuration {struct {String deviceName="test";bool useKISS=false;bool bondResetPending=false;} bluetooth;bool writeFile(){return true;}};
namespace logging {enum class LoggerLevel {LOGGER_LEVEL_INFO,LOGGER_LEVEL_ERROR,LOGGER_LEVEL_DEBUG};struct Logger {template<class... A> void log(A...){} };}
static std::vector<std::string> radioPackets;
namespace LoRa_Utils {inline void sendNewPacket(const String& s){radioPackets.push_back(s.s);} }
namespace KissChar {constexpr uint8_t FEND=0xc0;}
namespace KISS_Utils {inline String decodeKISS(const String& s,bool& ok){ok=true;return s;} inline String encodeKISS(const String& s){return s;} }
static unsigned wakes=0;static std::string displayText;
inline void displayWake(){++wakes;}
static void (*displayHook)() = nullptr;
inline void displayShow(const String&,const String&,const String& text,int=0){displayText=text.s;if(displayHook)displayHook();}
