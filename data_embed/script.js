// Custom scripts

let currentSettings = null;

function backupSettings() {
    const data =
        "data:text/json;charset=utf-8," +
        encodeURIComponent(JSON.stringify(currentSettings));
    const a = document.createElement("a");
    a.setAttribute("href", data);
    a.setAttribute("download", "TrackerConfigurationBackup.json");
    a.click();
}

document.getElementById("backup").onclick = backupSettings;

document.getElementById("restore").onclick = function (e) {
    e.preventDefault();

    document.querySelector("input[type=file]").click();
};

document.querySelector("input[type=file]").onchange = function () {
    const files = document.querySelector("input[type=file]").files;

    if (!files.length) return;

    const file = files.item(0);

    const reader = new FileReader();
    reader.readAsText(file);
    reader.onload = () => {
        const data = JSON.parse(reader.result);

        loadSettings(data);
    };
};

function fetchSettings() {
    fetch("/configuration.json")
        .then((response) => response.json())
        .then((settings) => {
            console.log(settings);
            loadSettings(settings);
        })
        .catch((err) => {
            console.error(err);

            alert(`Failed to load configuration`);
        });
}

function loadSettings(settings) {
    currentSettings = settings;
    
    // BEACONS
    const beaconContainer = document.getElementById("beacon-settings");
    beaconContainer.innerHTML = ""; // Clear previous content

    settings.beacons.forEach((beacons, index) => {
        const beaconElement = document.createElement("div");
        beaconElement.classList.add("row", "beacons", "border-bottom", "py-2");

        beaconElement.innerHTML = `
            <div class="col-1 px-1 mb-2 d-flex align-items-center">
                <strong>${index + 1})</strong> <!-- Adding numbering here -->
            </div>
            <div class="form-floating col-6 col-md-3 px-1 mb-2">
                <input 
                    type="text" 
                    class="form-control form-control-sm" 
                    name="beacons.${index}.callsign" 
                    id="beacons.${index}.callsign" 
                    value="${beacons.callsign}">
                <label for="beacons.${index}.callsign">Callsign</label>
            </div>
            <div class="form-floating col-6 col-md-2 px-1 mb-2">
                <input 
                    type="text" 
                    class="form-control form-control-sm" 
                    name="beacons.${index}.symbol" 
                    id="beacons.${index}.symbol" 
                    value="${beacons.symbol}">
                <label for="beacons.${index}.symbol">Symbol</label>
            </div>
            <div class="form-floating col-6 col-md-2 px-1 mb-2">
                <input 
                    type="text" 
                    class="form-control form-control-sm" 
                    name="beacons.${index}.overlay" 
                    id="beacons.${index}.overlay" 
                    value="${beacons.overlay}">
                <label for="beacons.${index}.overlay">Overlay</label>
            </div>
            <div class="form-floating col-6 col-md-2 px-1 mb-2">
                <input 
                    type="text" 
                    class="form-control form-control-sm" 
                    name="beacons.${index}.micE" 
                    id="beacons.${index}.micE" 
                    value="${beacons.micE}">
                <label for="beacons.${index}.micE">Mic-E</label>
            </div>
            <div class="form-floating col-12 col-md-9 px-1 mb-2" style="margin-left: 50px;">
                <input 
                    type="text" 
                    class="form-control form-control-sm" 
                    name="beacons.${index}.comment" 
                    id="beacons.${index}.comment" 
                    value="${beacons.comment}">
                <label for="beacons.${index}.comment">Comment</label>
            </div>
            <div class="form-check form-switch col-6 col-md-5 px-1 mb-2" style="margin-left: 90px;">
                <input 
                    class="form-check-input" 
                    type="checkbox" 
                    name="beacons.${index}.smartBeaconActive" 
                    id="beacons.${index}.smartBeaconActive" 
                    value="1" 
                    ${beacons.smartBeaconActive ? 'checked' : ''}>
                <label class="form-check-label" for="beacons.${index}.smartBeaconActive">
                    Smart Beacon Active
                </label>
            </div>
            <div class="form-check form-switch col-6 col-md-3 px-1 mb-2">
                <input 
                    class="form-check-input" 
                    type="checkbox" 
                    name="beacons.${index}.gpsEcoMode" 
                    id="beacons.${index}.gpsEcoMode"
                    value="1"
                    ${beacons.gpsEcoMode ? 'checked' : ''}>
                <label class="form-check-label" for="beacons.${index}.gpsEcoMode">
                    GPS Eco Mode
                </label>
            </div>
            <div class="form-check form-switch col-6 col-md-5 px-1 mb-2" style="margin-left: 50px;">
                <label for="beacons.${index}.smartBeaconSetting" class="form-label"><small>Smart Beacon Setting</small></label>
                <select name="beacons.${index}.smartBeaconSetting" id="beacons.${index}.smartBeaconSetting" class="form-control">
                    <option value="0" ${beacons.smartBeaconSetting == 0 ? 'selected' : ''}>Human/Runner (Slow Speed)</option>
                    <option value="1" ${beacons.smartBeaconSetting == 1 ? 'selected' : ''}>Bicycle (Mid Speed)</option>
                    <option value="2" ${beacons.smartBeaconSetting == 2 ? 'selected' : ''}>Car/Motorcycle (Fast Speed)</option>
                </select>
            </div>
            <div class="form-floating col-12 col-md-9 px-1 mb-2" style="margin-left: 50px;">
                 <input 
                     type="text" 
                     class="form-control form-control-sm" 
                     name="beacons.${index}.profileLabel" 
                     id="beacons.${index}.profileLabel" 
                     value="${beacons.profileLabel}">
                 <label for="beacons.${index}.profileLabel">Profile Label</label>
             </div>
        `;
        beaconContainer.appendChild(beaconElement);
    });

    // ADITIONAL STATION CONFIG
    document.getElementById("simplifiedTrackerMode").checked            = settings.other.simplifiedTrackerMode;
    document.getElementById("sendCommentAfterXBeacons").value           = settings.other.sendCommentAfterXBeacons;
    document.getElementById("path").value                               = settings.other.path;
    document.getElementById("nonSmartBeaconRate").value                 = settings.other.nonSmartBeaconRate;
    document.getElementById("rememberStationTime").value                = settings.other.rememberStationTime;
    document.getElementById("standingUpdateTime").value                 = settings.other.standingUpdateTime;
    document.getElementById("sendAltitude").checked                     = settings.other.sendAltitude ;
    document.getElementById("disableGPS").checked                       = settings.other.disableGPS;
    document.getElementById("email").value                              = settings.other.email;

    // DISPLAY
    document.getElementById("display.ecoMode").checked                  = settings.display.ecoMode;
    document.getElementById("display.turn180").checked                  = settings.display.turn180;
    document.getElementById("display.timeout").value                    = settings.display.timeout;
    document.getElementById("display.showSymbol").checked               = settings.display.showSymbol;
    DisplayEcoModeCheckbox.checked  = settings.display.ecoMode;
    DisplayTimeout.disabled         = !DisplayEcoModeCheckbox.checked;

    // BLUETOOTH
    document.getElementById("bluetooth.active").checked                 = settings.bluetooth.active;
    document.getElementById("bluetooth.deviceName").value               = settings.bluetooth.deviceName;
    document.getElementById("bluetooth.useBLE").checked                 = settings.bluetooth.useBLE;
    document.getElementById("bluetooth.useKISS").checked                = settings.bluetooth.useKISS;
    BluetoothActiveCheckbox.checked = settings.bluetooth.active;
    BluetoothDeviceName.disabled    = !BluetoothActiveCheckbox.checked;
    BluetoothUseBle.disabled        = !BluetoothActiveCheckbox.checked;
    BluetoothUseKiss.disabled       = !BluetoothActiveCheckbox.checked;

    // LORA
    const loraContainer = document.getElementById("lora-settings");
    loraContainer.innerHTML = ""; // Clear previous content

    settings.lora.forEach((lora, index) => {
        const loraElement = document.createElement("div");
        loraElement.classList.add("row", "lora", "border-bottom", "py-2");

        loraElement.innerHTML = `
            <div class="col-1 px-1 mb-2 d-flex align-items-center">
                <strong>${index + 1})</strong> <!-- Adding numbering here -->
            </div>
            <div class="form-floating col-6 col-md-3 px-1 mb-2">
                <input 
                    type="number" 
                    class="form-control form-control-sm" 
                    name="lora.${index}.frequency" 
                    id="lora.${index}.frequency" 
                    value="${lora.frequency}">
                <label for="lora.${index}.frequency">Frequency</label>
            </div>
            <div class="form-floating col-4 col-md-2 px-1 mb-2">
                <input 
                    type="number" 
                    class="form-control form-control-sm" 
                    name="lora.${index}.spreadingFactor" 
                    id="lora.${index}.spreadingFactor" 
                    value="${lora.spreadingFactor}"
                    min="7"
                    max="12">
                <label for="lora.${index}.spreadingFactor">SF</label>
            </div>
            <div class="form-floating col-4 col-md-2 px-1 mb-2">
                <input 
                    type="number" 
                    class="form-control form-control-sm" 
                    name="lora.${index}.codingRate4" 
                    id="lora.${index}.codingRate4" 
                    value="${lora.codingRate4}"
                    min="5"
                    max="8">
                <label for="lora.${index}.codingRate4">CR4</label>
            </div>
            <div class="form-floating col-4 col-md-2 px-1 mb-2">
                <input 
                    type="number" 
                    class="form-control form-control-sm" 
                    name="lora.${index}.signalBandwidth" 
                    id="lora.${index}.signalBandwidth" 
                    value="${lora.signalBandwidth}"
                    min="62500"
                    max="500000">
                <label for="lora.${index}.signalBandwidth">BW</label>
            </div>
        `;
        loraContainer.appendChild(loraElement);
    });

    // BATTERY
    document.getElementById("battery.sendVoltage").checked              = settings.battery.sendVoltage;
    document.getElementById("battery.voltageAsTelemetry").checked       = settings.battery.voltageAsTelemetry;
    document.getElementById("battery.sendVoltageAlways").checked        = settings.battery.sendVoltageAlways;
    BatterySendVoltageCheckbox.checked  = settings.battery.sendVoltage;
    BatteryVoltageAsTelemetry.disabled  = !BatterySendVoltageCheckbox.checked;
    BatteryForceSendVoltage.disabled    = !BatterySendVoltageCheckbox.checked;

    document.getElementById("battery.monitorVoltage").checked           = settings.battery.monitorVoltage;
    document.getElementById("battery.sleepVoltage").value               = settings.battery.sleepVoltage.toFixed(1);
    BatteryMonitorVoltageCheckbox.checked   = settings.battery.monitorVoltage;
    BatteryMonitorSleepVoltage.disabled     = !BatteryMonitorVoltageCheckbox.checked;

    // TELEMETRY (WX Sensor)
    document.getElementById("telemetry.active").checked                  = settings.telemetry.active;
    document.getElementById("telemetry.sendTelemetry").checked           = settings.telemetry.sendTelemetry;
    document.getElementById("telemetry.temperatureCorrection").value     = settings.telemetry.temperatureCorrection.toFixed(1);
    TelemetryCheckbox.checked           = settings.telemetry.active;
    TelemetrySendCheckbox.disabled      = !TelemetryCheckbox.checked;
    TelemetryTempCorrection.disabled    = !TelemetryCheckbox.checked;

    // WINLINK
    document.getElementById("winlink.password").value                   = settings.winlink.password;

    // WiFi AP
    document.getElementById("wifiAP.password").value                    = settings.wifiAP.password;

    // NOTIFICATION
    document.getElementById("notification.ledTx").checked               = settings.notification.ledTx;
    document.getElementById("notification.ledTxPin").value              = settings.notification.ledTxPin;
    NotificationLedTxCheckbox.checked   = settings.notification.ledTx;
    NotificationLedTxPin.disabled       = !NotificationLedTxCheckbox.checked;

    document.getElementById("notification.ledMessage").checked          = settings.notification.ledMessage;
    document.getElementById("notification.ledMessagePin").value         = settings.notification.ledMessagePin;
    NotificationLedMessageCheckbox.checked  = settings.notification.ledMessage;
    NotificationLedMessagePin.disabled      = !NotificationLedMessageCheckbox.checked;
    
    document.getElementById("notification.buzzerActive").checked        = settings.notification.buzzerActive;
    document.getElementById("notification.buzzerPinTone").value         = settings.notification.buzzerPinTone;
    document.getElementById("notification.buzzerPinVcc").value          = settings.notification.buzzerPinVcc;
    document.getElementById("notification.bootUpBeep").checked          = settings.notification.bootUpBeep;
    document.getElementById("notification.txBeep").checked              = settings.notification.txBeep;
    document.getElementById("notification.messageRxBeep").checked       = settings.notification.messageRxBeep;
    document.getElementById("notification.stationBeep").checked         = settings.notification.stationBeep;
    document.getElementById("notification.lowBatteryBeep").checked      = settings.notification.lowBatteryBeep;
    document.getElementById("notification.shutDownBeep").checked        = settings.notification.shutDownBeep;
    NotificationBuzzerCheckbox.checked          = settings.notification.buzzerActive;
    NotificationBuzzerTonePin.disabled          = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerVccPin.disabled           = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerBootUpBeep.disabled       = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerTxBeep.disabled           = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerMessageBeep.disabled      = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerStationBeep.disabled      = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerLowBatteryBeep.disabled   = !NotificationBuzzerCheckbox.checked;
    NotificationBuzzerShutDownBeep.disabled     = !NotificationBuzzerCheckbox.checked;

    document.getElementById("notification.ledFlashlight").checked       = settings.notification.ledFlashlight;
    document.getElementById("notification.ledFlashlightPin").value      = settings.notification.ledFlashlightPin;
    NotificationLedFlashlightCheckbox.checked   = settings.notification.ledFlashlight;
    NotificationLedFlashlightPin.disabled       = !NotificationLedFlashlightCheckbox.checked;
    
    //  PTT Trigger
    document.getElementById("ptt.active").checked                       = settings.pttTrigger.active;
    document.getElementById("ptt.reverse").checked                      = settings.pttTrigger.reverse;
    document.getElementById("ptt.preDelay").value                       = settings.pttTrigger.preDelay;
    document.getElementById("ptt.postDelay").value                      = settings.pttTrigger.postDelay;
    document.getElementById("ptt.io_pin").value                         = settings.pttTrigger.io_pin;
    pttTriggerCheckbox.checked  = settings.pttTrigger.active;
    pttReverseCheckbox.disabled = !pttTriggerCheckbox.checked;
    pttPreDelayInput.disabled   = !pttTriggerCheckbox.checked;
    pttPostDelayInput.disabled  = !pttTriggerCheckbox.checked;
    pttPinInput.disabled        = !pttTriggerCheckbox.checked;

}

function showToast(message) {
    const el = document.querySelector('#toast');

    el.querySelector('.toast-body').innerHTML = message;

    (new bootstrap.Toast(el)).show();
}

document.getElementById('reboot').addEventListener('click', function (e) {
    e.preventDefault();

    fetch("/action?type=reboot", { method: "POST" });

    showToast("Your device will be rebooted in a while");
});


// Display Switches
const DisplayEcoModeCheckbox    = document.querySelector('input[name="display.ecoMode"]');
const DisplayTimeout            = document.querySelector('input[name="display.timeout"]');
DisplayEcoModeCheckbox.addEventListener("change", function () {
    DisplayTimeout.disabled     = !this.checked;
});

// Bluetooth Switches
const BluetoothActiveCheckbox   = document.querySelector('input[name="bluetooth.active"]');
const BluetoothDeviceName       = document.querySelector('input[name="bluetooth.deviceName"]');
const BluetoothUseBle           = document.querySelector('input[name="bluetooth.useBLE"]');
const BluetoothUseKiss          = document.querySelector('input[name="bluetooth.useKISS"]');
BluetoothActiveCheckbox.addEventListener("change", function () {
    BluetoothDeviceName.disabled    = !this.checked;
    BluetoothUseBle.disabled        = !this.checked;
    BluetoothUseKiss.disabled       = !this.checked;
});

// Battery Switches
const BatterySendVoltageCheckbox    = document.querySelector('input[name="battery.sendVoltage"]');
const BatteryVoltageAsTelemetry     = document.querySelector('input[name="battery.voltageAsTelemetry"]');
const BatteryForceSendVoltage       = document.querySelector('input[name="battery.sendVoltageAlways"]');
BatterySendVoltageCheckbox.addEventListener("change", function () {
    BatteryVoltageAsTelemetry.disabled  = !this.checked;
    BatteryForceSendVoltage.disabled    = !this.checked;
});

const BatteryMonitorVoltageCheckbox = document.querySelector('input[name="battery.monitorVoltage"]');
const BatteryMonitorSleepVoltage    = document.querySelector('input[name="battery.sleepVoltage"]');
BatteryMonitorVoltageCheckbox.addEventListener("change", function () {
    BatteryMonitorSleepVoltage.disabled = !this.checked;
});

// Telemetry Switches
const TelemetryCheckbox         = document.querySelector('input[name="telemetry.active"]');
const TelemetrySendCheckbox     = document.querySelector('input[name="telemetry.sendTelemetry"]');
const TelemetryTempCorrection   = document.querySelector('input[name="telemetry.temperatureCorrection"]');
TelemetryCheckbox.addEventListener("change", function () {
    TelemetrySendCheckbox.disabled      = !this.checked;
    TelemetryTempCorrection.disabled    = !this.checked;
});

// Notifications Switches
const NotificationLedTxCheckbox         = document.querySelector('input[name="notification.ledTx"]');
const NotificationLedTxPin              = document.querySelector('input[name="notification.ledTxPin"]');
NotificationLedTxCheckbox.addEventListener("change", function () {
    NotificationLedTxPin.disabled       = !this.checked;
});

const NotificationLedMessageCheckbox    = document.querySelector('input[name="notification.ledMessage"]');
const NotificationLedMessagePin         = document.querySelector('input[name="notification.ledMessagePin"]');
NotificationLedMessageCheckbox.addEventListener("change", function () {
    NotificationLedMessagePin.disabled  = !this.checked;
});

const NotificationBuzzerCheckbox        = document.querySelector('input[name="notification.buzzerActive"]');
const NotificationBuzzerTonePin         = document.querySelector('input[name="notification.buzzerPinTone"]');
const NotificationBuzzerVccPin          = document.querySelector('input[name="notification.buzzerPinVcc"]');
const NotificationBuzzerBootUpBeep      = document.querySelector('input[name="notification.bootUpBeep"]');
const NotificationBuzzerTxBeep          = document.querySelector('input[name="notification.txBeep"]');
const NotificationBuzzerMessageBeep     = document.querySelector('input[name="notification.messageRxBeep"]');
const NotificationBuzzerStationBeep     = document.querySelector('input[name="notification.stationBeep"]');
const NotificationBuzzerLowBatteryBeep  = document.querySelector('input[name="notification.lowBatteryBeep"]');
const NotificationBuzzerShutDownBeep    = document.querySelector('input[name="notification.shutDownBeep"]');
NotificationBuzzerCheckbox.addEventListener("change", function () {
    NotificationBuzzerTonePin.disabled          = !this.checked;
    NotificationBuzzerVccPin.disabled           = !this.checked;
    NotificationBuzzerBootUpBeep.disabled       = !this.checked;
    NotificationBuzzerTxBeep.disabled           = !this.checked;
    NotificationBuzzerMessageBeep.disabled      = !this.checked;
    NotificationBuzzerStationBeep.disabled      = !this.checked;
    NotificationBuzzerLowBatteryBeep.disabled   = !this.checked;
    NotificationBuzzerShutDownBeep.disabled     = !this.checked;
});

const NotificationLedFlashlightCheckbox = document.querySelector('input[name="notification.ledFlashlight"]');
const NotificationLedFlashlightPin      = document.querySelector('input[name="notification.ledFlashlightPin"]');
NotificationLedFlashlightCheckbox.addEventListener("change", function () {
    NotificationLedFlashlightPin.disabled  = !this.checked;
});

// PTT Switches
const pttTriggerCheckbox    = document.querySelector('input[name="ptt.active"]');
const pttReverseCheckbox    = document.querySelector('input[name="ptt.reverse"]');
const pttPreDelayInput      = document.querySelector('input[name="ptt.preDelay"]');
const pttPostDelayInput     = document.querySelector('input[name="ptt.postDelay"]');
const pttPinInput           = document.querySelector('input[name="ptt.io_pin"]');
pttTriggerCheckbox.addEventListener("change", function () {
    pttReverseCheckbox.disabled = !this.checked;
    pttPreDelayInput.disabled   = !this.checked;
    pttPostDelayInput.disabled  = !this.checked;
    pttPinInput.disabled        = !this.checked;
});




const form = document.querySelector("form");

const saveModal = new bootstrap.Modal(document.getElementById("saveModal"), {
    backdrop: "static",
    keyboard: false,
});

const savedModal = new bootstrap.Modal(
    document.getElementById("savedModal"),
    {}
);

function checkConnection() {
    const controller = new AbortController();

    setTimeout(() => controller.abort(), 2000);

    fetch("/status?_t=" + Date.now(), { signal: controller.signal })
        .then(() => {
            saveModal.hide();

            savedModal.show();

            setTimeout(function () {
                savedModal.hide();
            }, 3000);

            fetchSettings();
        })
        .catch((err) => {
            setTimeout(checkConnection, 0);
        });
}

form.addEventListener("submit", async (event) => {
    event.preventDefault();

    fetch(form.action, {
        method: form.method,
        body: new FormData(form),
    });
    saveModal.show();
    setTimeout(checkConnection, 2000);
});


fetchSettings();