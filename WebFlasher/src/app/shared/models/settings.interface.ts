export interface Settings {
  beacons:      Beacon[];
  lora:         Lora;
  other:        Other;
  pttTrigger:   PttTrigger;
  bme:          Bme;
  notification: Notification;
}

export interface Beacon {
  callsign:     string;
  symbol:       string;
  overlay:      string;
  comment:      string;
  smart_beacon: SmartBeacon;
}

export interface SmartBeacon {
  active:         boolean;
  slowRate:       number;
  slowSpeed:      number;
  fastRate:       number;
  fastSpeed:      number;
  minTxDist:      number;
  minDeltaBeacon: number;
  turnMinDeg:     number;
  turnSlope:      number;
}

export interface Bme {
  active:           boolean;
  sendTelemetry:    boolean;
  heightCorrection: number;
}

export interface Lora {
  frequency:       number;
  spreadingFactor: number;
  signalBandwidth: number;
  codingRate4:     number;
  power:           number;
}

export interface Notification {
  ledTx:          boolean;
  ledTxPin:       number;
  ledMessage:     boolean;
  ledMessagePin:  number;
  buzzerActive:   boolean;
  buzzerPinTone:  number;
  buzzerPinVcc:   number;
  bootUpBeep:     boolean;
  txBeep:         boolean;
  messageRxBeep:  boolean;
  stationBeep:    boolean;
  lowBatteryBeep: boolean;
}

export interface Other {
  sendAltitude:             boolean;
  sendBatteryInfo:          boolean;
  showSymbolOnScreen:       boolean;
  displayEcoMode:           boolean;
  bluetooth:                boolean;
  disableGps:               boolean;
  simplifiedTrackerMode:    boolean;
  path:                     string;
  sendCommentAfterXBeacons: number;
  displayTimeout:           number;
  standingUpdateTime:       number;
  nonSmartBeaconRate:       number;
  rememberStationTime:      number;
  maxDistanceToTracker:     number;
}

export interface PttTrigger {
  active:    boolean;
  reverse:   boolean;
  io_pin:    number;
  preDelay:  number;
  postDelay: number;
}
