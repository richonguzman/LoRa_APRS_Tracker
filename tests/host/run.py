#!/usr/bin/env python3
"""Run host regression tests; compiles production functions, not copied logic."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]

def function(path, signature):
    text = (root / path).read_text()
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

with tempfile.TemporaryDirectory() as temp:
    executable = str(Path(temp) / 'ble-test')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
                    '-Itests/host/stubs', '-Iinclude', 'tests/host/ble_security.cpp',
                    '-o', executable], cwd=root, check=True)
    subprocess.run([executable], check=True)
    code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
static uint32_t now=20000,displayTime=0;
static bool displayState=false,displayEcoMode=true;
static int menuDisplay=0;
struct {struct {uint32_t timeout=4;} display;} Config;
uint32_t millis(){return now;}
#define SH110X_DISPLAYON 1
#define SH110X_DISPLAYOFF 0
struct {bool on=false; void oled_command(int command){on=command;}} display;
void displayToggle(bool);
'''
    code += function('src/display.cpp', 'void displayWake()') + '\n'
    code += function('src/display.cpp', 'void displayToggle(bool toggle)') + '\n'
    code += function('src/utils.cpp', 'void checkDisplayEcoMode()') + '\n'
    code += r'''
int main(){
 displayWake();assert(display.on && displayState && displayTime==now);
 now+=3999;checkDisplayEcoMode();assert(display.on);
 now++;checkDisplayEcoMode();assert(!display.on && !displayState);
 displayWake();now+=3000;displayWake();now+=3000;
 checkDisplayEcoMode();assert(display.on); // activity renews timeout
 now+=1000;checkDisplayEcoMode();assert(!display.on);
 displayToggle(true);assert(displayState); // button/menu path stays in sync
 now+=4000;checkDisplayEcoMode();assert(!display.on);
 now=UINT32_MAX-1000;displayWake();now=2999;checkDisplayEcoMode();
 // Existing 10-second startup guard defers shutdown across millis wrap.
 now=11000;checkDisplayEcoMode();assert(!display.on);
 puts("PASS: OLED wake, timeout, activity renewal, toggle state, millis wrap");
}
'''
    source = Path(temp) / 'display-test.cpp'
    source.write_text(code)
    executable = str(Path(temp) / 'display-test')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(source), '-o', executable], check=True)
    subprocess.run([executable], check=True)

    # Exercise the actual OLED render paths while BLE owns the display.
    code = r'''
#include <cassert>
#include <string>
#include <cstdio>
using String=std::string;
namespace BLE_Utils { bool pairing=false; bool isPairing(){return pairing;} }
#define SH110X_WHITE 1
struct {
 int frames=0; String header; bool first=true;
 void clearDisplay(){first=true;}
 void setTextColor(int){} void setTextSize(int){} void setCursor(int,int){}
 void println(const String& s){if(first){header=s;first=false;}}
 void setContrast(int){} void drawLine(int,int,int,int,int){}
 void display(){frames++;}
} display;
int screenBrightness=80,menuDisplay=0,symbolArraySize=0;
bool symbolAvailable=false,bluetoothConnected=false;
String symbolArray[1];
struct { String symbol; } beacon, *currentBeacon=&beacon;
struct { struct {bool showSymbol=false;} display; } Config;
int delays=0;
void delay(int ms){delays+=ms;}
int now(){return 0;}
void drawSymbol(int,bool){}
'''
    for signature in [
        'static void displayShowShort(',
        'void displayShow(const String& header, const String& line1, const String& line2, int wait)',
        'void displayShowPairing(',
        'void displayShow(const String& header, const String& line1, const String& line2, const String& line3',
    ]:
        code += function('src/display.cpp', signature) + '\n'
    code += r'''
int main(){
 displayShow("MAIN","","","","","",0);assert(display.frames==1);
 BLE_Utils::pairing=true;
 displayShowPairing("BLE PAIR","Enter PIN","123456");
 assert(display.frames==2 && display.header=="BLE PAIR");
 for(int i=0;i<10;i++){
   displayShow("MAIN","","","","","",0);
   displayShow("RX","","packet",1000);
 }
 assert(display.frames==2 && display.header=="BLE PAIR" && delays==0);
 BLE_Utils::pairing=false;
 displayShowPairing("BLE PAIR","","");assert(display.frames==2);
 displayShow("MAIN","","","","","",0);assert(display.frames==3 && display.header=="MAIN");
 displayShow("TX","","packet",1000);assert(display.frames==4 && delays==1000);
 puts("PASS: PIN owns OLED across repeated main/message redraws; rendering resumes after pairing");
}
'''
    source = Path(temp) / 'pairing-display-test.cpp'
    source.write_text(code)
    executable = str(Path(temp) / 'pairing-display-test')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(source), '-o', executable], check=True)
    subprocess.run([executable], check=True)

    code = r'''
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
using String=std::string;
bool bluetoothActive=false, bluetoothConnected=false;
struct { struct {bool useKISS=false;} bluetooth;} Config;
std::vector<String> queued;
namespace APRSPacketLib {
 String generateAckMessage(const String& payload) {
   auto pos=payload.find('{');
   return pos==String::npos || pos+1==payload.size() ? "" : "ack"+payload.substr(pos+1);
 }
}
void addToOutputBuffer(int, const String& sender, const String& text) {
 queued.push_back(sender+":"+text);
}
'''
    code += function('src/msg_utils.cpp', 'static void acknowledgeReceivedMessage(')
    code += r'''
int main(){
 for(int active=0;active<2;active++) for(int connected=0;connected<2;connected++)
 for(int kiss=0;kiss<2;kiss++){
   bluetoothActive=active;bluetoothConnected=connected;Config.bluetooth.useKISS=kiss;
   queued.clear();
   acknowledgeReceivedMessage("DA6PG-1","test{031");
   if(active && connected && kiss) assert(queued.empty());
   else assert(queued.size()==1 && queued[0]=="DA6PG-1:ack031");
 }
 bluetoothActive=true;Config.bluetooth.useKISS=true;
 queued.clear();bluetoothConnected=true;
 acknowledgeReceivedMessage("DA6PG-1","test{032");assert(queued.empty());
 bluetoothConnected=false;
 acknowledgeReceivedMessage("DA6PG-1","test{033");assert(queued.size()==1);
 bluetoothConnected=true;
 acknowledgeReceivedMessage("DA6PG-1","test{034");assert(queued.size()==1);
 bluetoothConnected=false;
 acknowledgeReceivedMessage("DA6PG-1","test{");assert(queued.size()==1);
 puts("PASS: connected KISS owns ACKs; standalone/TNC2 and disconnect restore tracker ACKs");
}
'''
    source = Path(temp) / 'ack-test.cpp'
    source.write_text(code)
    executable = str(Path(temp) / 'ack-test')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(source), '-o', executable], check=True)
    subprocess.run([executable], check=True)
