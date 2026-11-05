// Host-side behavioral tests of the production BLE transport; no radio needed.
#include "../../src/ble_utils.cpp"
Configuration Config;
logging::Logger logger;
bool bluetoothConnected=false,bluetoothActive=true;
int main(){
 BLE_Utils::setup();
 assert(NimBLEDevice::bonding && NimBLEDevice::mitm);
 assert(pCharacteristicRx->props & NIMBLE_PROPERTY::WRITE_AUTHEN);
 assert(pCharacteristicRx->props & NIMBLE_PROPERTY::WRITE_ENC);
 auto connect=[](uint16_t h){peers[h]={h,{false,false}};pServer->cb->onConnect(pServer,&peers[h]);};
 auto authorize=[](uint16_t h){peers[h].sec_state={true,true};pServer->cb->onAuthenticationComplete(&peers[h]);};
 auto write=[](uint16_t h,const std::string& s){pCharacteristicRx->value=s;pCharacteristicRx->cb->onWrite(pCharacteristicRx,&peers[h]);};
 connect(1);
 write(1,"unauthenticated"); BLE_Utils::loop(); assert(radioPackets.empty());
 BLE_Utils::sendToPhone("secret");assert(pCharacteristicTx->notifications==0);
 auto pin=pServer->cb->onPassKeyRequest();assert(pin>=100000 && pin<=999999);
 assert(BLE_Utils::isPairing());
 BLE_Utils::showPairing();assert(wakes>0 && displayText.find(std::to_string(pin))!=std::string::npos);
 peers[1].sec_state={true,false};pServer->cb->onAuthenticationComplete(&peers[1]);
 write(1,"encrypted but no MITM");BLE_Utils::loop();assert(radioPackets.empty());
 assert(!pServer->disconnected.empty());
 assert(!BLE_Utils::isPairing());
 authorize(1);write(1,"authorized");BLE_Utils::loop();assert(radioPackets.size()==1);
 BLE_Utils::sendToPhone("reply");assert(pCharacteristicTx->notifications>0);
 connect(2);assert(activeHandle==1 && pServer->disconnected.back()==2);
 write(2,"second peer");BLE_Utils::loop();assert(radioPackets.size()==1);
 write(1,"queued before disconnect");
 pServer->cb->onDisconnect(pServer,&peers[1]);peers.erase(1);BLE_Utils::loop();assert(radioPackets.size()==1);
 connect(1);authorize(1);BLE_Utils::loop();assert(radioPackets.size()==1);
 Config.bluetooth.useKISS=true;
 write(1,std::string("\xc0\x00",2));BLE_Utils::loop();assert(radioPackets.size()==1);
 write(1,std::string("x\xc0",2));BLE_Utils::loop();assert(radioPackets.size()==2);
 write(1,std::string("\xc0\x00",2));BLE_Utils::loop();
 pServer->cb->onDisconnect(pServer,&peers[1]);BLE_Utils::loop();
 connect(1);authorize(1);BLE_Utils::loop();
 write(1,std::string("x\xc0",2));BLE_Utils::loop();assert(radioPackets.size()==2);
 Config.bluetooth.useKISS=false;
 for(int i=0;i<9;i++) { write(1,"overflow"); }
 BLE_Utils::loop();assert(radioPackets.size()==2);
 displayHook=[](){pServer->cb->onDisconnect(pServer,&peers[1]);};
 write(1,"disconnect during display");BLE_Utils::loop();assert(radioPackets.size()==2);
 displayHook=nullptr;
 connect(1);authorize(1);BLE_Utils::loop();
 pServer->cb->onDisconnect(pServer,&peers[1]);BLE_Utils::loop();
 connect(3);clockMs+=60001;BLE_Utils::loop();assert(pServer->disconnected.back()==3);
 puts("PASS: unauthenticated/encrypted-only blocked; authenticated TX/RX; PIN; second peer; reconnect; partial frames; overflow; timeout");
}
