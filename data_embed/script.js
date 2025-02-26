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
            <div class="form-floating col-6 col-md-3 px-1 mb-2">
                <input 
                    type="number" 
                    class="form-control form-control-sm" 
                    name="lora.${index}.spreadingFactor" 
                    id="lora.${index}.spreadingFactor" 
                    value="${lora.spreadingFactor}"
                    min="9"
                    max="12">
                <label for="lora.${index}.spreadingFactor">Spreading Factor</label>
            </div>
            <div class="form-floating col-6 col-md-3 px-1 mb-2">
                <input 
                    type="number" 
                    class="form-control form-control-sm" 
                    name="lora.${index}.codingRate4" 
                    id="lora.${index}.codingRate4" 
                    value="${lora.codingRate4}"
                    min="5"
                    max="7">
                <label for="lora.${index}.codingRate4">Coding Rate</label>
            </div>            
        `;
        loraContainer.appendChild(loraElement);
    });

    // DISPLAY
    document.getElementById("display.showSymbol").checked               = settings.display.showSymbol;
    document.getElementById("display.ecoMode").checked                  = settings.display.ecoMode;
    document.getElementById("display.timeout").value                    = settings.display.timeout;
    document.getElementById("display.turn180").checked                  = settings.display.turn180;
    /*if (settings.display.alwaysOn) {
        timeoutInput.disabled = true;
    }*/

    // BATTERY
    document.getElementById("battery.sendVoltage").checked              = settings.battery.sendVoltage;
    document.getElementById("battery.voltageAsTelemetry").checked       = settings.battery.voltageAsTelemetry;
    document.getElementById("battery.sendVoltageAlways").checked        = settings.battery.sendVoltageAlways;
    document.getElementById("battery.monitorVoltage").checked           = settings.battery.monitorVoltage;
    document.getElementById("battery.sleepVoltage").value               = settings.battery.sleepVoltage.toFixed(1);

    // WINLINK
    document.getElementById("winlink.password").value                   = settings.winlink.password;

    // TELEMETRY WX Sensor
    document.getElementById("wxsensor.active").checked                  = settings.wxsensor.active;
    document.getElementById("wxsensor.temperatureCorrection").value     = settings.wxsensor.temperatureCorrection.toFixed(1);
    document.getElementById("wxsensor.sendTelemetry").checked           = settings.wxsensor.sendTelemetry;
    
    // NOTIFICATION
    document.getElementById("notification.ledTx").checked               = settings.notification.ledTx;
    document.getElementById("notification.ledTxPin").value              = settings.notification.ledTxPin;
    document.getElementById("notification.ledMessage").checked          = settings.notification.ledMessage;
    document.getElementById("notification.ledMessagePin").value         = settings.notification.ledMessagePin;
    document.getElementById("notification.ledFlashlight").checked       = settings.notification.ledFlashlight;
    document.getElementById("notification.ledFlashlightPin").value      = settings.notification.ledFlashlightPin;
    document.getElementById("notification.buzzerActive").checked        = settings.notification.buzzerActive;
    document.getElementById("notification.buzzerPinTone").value         = settings.notification.buzzerPinTone;
    document.getElementById("notification.buzzerPinVcc").value          = settings.notification.buzzerPinVcc;
    document.getElementById("notification.bootUpBeep").checked          = settings.notification.bootUpBeep;
    document.getElementById("notification.txBeep").checked              = settings.notification.txBeep;
    document.getElementById("notification.messageRxBeep").checked       = settings.notification.messageRxBeep;
    document.getElementById("notification.stationBeep").checked         = settings.notification.stationBeep;
    document.getElementById("notification.lowBatteryBeep").checked      = settings.notification.lowBatteryBeep;
    document.getElementById("notification.shutDownBeep").checked        = settings.notification.shutDownBeep;

    // BLUETOOTH
    document.getElementById("bluetooth.active").checked                 = settings.bluetooth.active;
    document.getElementById("bluetooth.deviceName").value               = settings.bluetooth.deviceName;
    document.getElementById("bluetooth.useBLE").checked                 = settings.bluetooth.useBLE;
    document.getElementById("bluetooth.useKISS").checked                = settings.bluetooth.useKISS;
    
    //  PTT Trigger
    document.getElementById("ptt.active").checked                       = settings.pttTrigger.active;
    document.getElementById("ptt.io_pin").value                         = settings.pttTrigger.io_pin;
    document.getElementById("ptt.preDelay").value                       = settings.pttTrigger.preDelay;
    document.getElementById("ptt.postDelay").value                      = settings.pttTrigger.postDelay;
    document.getElementById("ptt.reverse").checked                      = settings.pttTrigger.reverse;

    // WiFi AP
    document.getElementById("wifiAP.password").value                    = settings.wifiAP.password;

    
    //refreshSpeedStandard();
    toggleFields();
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

//const bmeCheckbox = document.querySelector("input[name='bme.active']");

function toggleFields() {
    /*
    // Display - timeout box enable
    const ecoModeCheckbox       = document.querySelector('input[name="display.ecoMode"]');
    const timeoutInput          = document.querySelector('input[name="display.timeout"]');
    ecoModeCheckbox.addEventListener("change", function () {
        timeoutInput.disabled           = !this.checked;
    });

    // pttTrigger boxes enable
    const pttTriggerCheckbox    = document.querySelector('input[name="ptt.active"]');
    const pttPinInput           = document.querySelector('input[name="ptt.io_pin"]');
    const pttPreDelayInput      = document.querySelector('input[name="ptt.preDelay"]');
    const pttPostDelayInput     = document.querySelector('input[name="ptt.postDelay"]');
    pttTriggerCheckbox.addEventListener("change", function () {
        pttPinInput.disabled            = !this.checked;
        pttPreDelayInput.disabled       = !this.checked;
        pttPostDelayInput.disabled      = !this.checked;
    });

    // notifications boxes enable
    const ledTxCheckbox         = document.querySelector('input[name="notification.ledTx"]');
    const letTxPinInput         = document.querySelector('input[name="notification.ledTxPin"]');
    ledTxCheckbox.addEventListener("change", function () {
        letTxPinInput.disabled          = !this.checked;
    });

    const ledMessageCheckbox    = document.querySelector('input[name="notification.ledMessage"]');
    const ledMessagePinInput    = document.querySelector('input[name="notification.ledMessagePin"]');
    ledMessageCheckbox.addEventListener("change", function () {
        ledMessagePinInput.disabled     = !this.checked;
    });

    const ledFlashlightCheckbox = document.querySelector('input[name="notification.ledFlashlight"]');
    const ledFlashlightPinInput = document.querySelector('input[name="notification.ledFlashlightPin"]');
    ledFlashlightCheckbox.addEventListener("change", function () {
        ledFlashlightPinInput.disabled  = !this.checked;
    });

    const buzzerActiveCheckbox  = document.querySelector('input[name="notification.buzzerActive"]');
    const buzzerPinToneInput    = document.querySelector('input[name="notification.buzzerPinTone"]');
    const buzzerPinVccInput     = document.querySelector('input[name="notification.buzzerPinVcc"]');
    buzzerActiveCheckbox.addEventListener("change", function () {
        buzzerPinToneInput.disabled     = !this.checked;
        buzzerPinVccInput.disabled      = !this.checked;
    });*/
}

/*
// Display - timeout box enable
const ecoModeCheckbox               = document.querySelector('input[name="display.ecoMode"]');
const timeoutInput                  = document.querySelector('input[name="display.timeout"]');
ecoModeCheckbox.addEventListener("change", function () {
    timeoutInput.disabled           = !this.checked;
});

// pttTrigger boxes enable
const pttTriggerCheckbox            = document.querySelector('input[name="ptt.active"]');
const pttPinInput                   = document.querySelector('input[name="ptt.io_pin"]');
const pttPreDelayInput              = document.querySelector('input[name="ptt.preDelay"]');
const pttPostDelayInput             = document.querySelector('input[name="ptt.postDelay"]');
pttTriggerCheckbox.addEventListener("change", function () {
    pttPinInput.disabled            = !this.checked;
    pttPreDelayInput.disabled       = !this.checked;
    pttPostDelayInput.disabled      = !this.checked;
});

// notifications boxes enable
const ledTxCheckbox                 = document.querySelector('input[name="notification.ledTx"]');
const letTxPinInput                 = document.querySelector('input[name="notification.ledTxPin"]');
ledTxCheckbox.addEventListener("change", function () {
    letTxPinInput.disabled          = !this.checked;
});

const ledMessageCheckbox            = document.querySelector('input[name="notification.ledMessage"]');
const ledMessagePinInput            = document.querySelector('input[name="notification.ledMessagePin"]');
ledMessageCheckbox.addEventListener("change", function () {
    ledMessagePinInput.disabled     = !this.checked;
});

const ledFlashlightCheckbox         = document.querySelector('input[name="notification.ledFlashlight"]');
const ledFlashlightPinInput         = document.querySelector('input[name="notification.ledFlashlightPin"]');
ledFlashlightCheckbox.addEventListener("change", function () {
    ledFlashlightPinInput.disabled  = !this.checked;
});

const buzzerActiveCheckbox          = document.querySelector('input[name="notification.buzzerActive"]');
const buzzerPinToneInput            = document.querySelector('input[name="notification.buzzerPinTone"]');
const buzzerPinVccInput             = document.querySelector('input[name="notification.buzzerPinVcc"]');
buzzerActiveCheckbox.addEventListener("change", function () {
    buzzerPinToneInput.disabled     = !this.checked;
    buzzerPinVccInput.disabled      = !this.checked;
});
*/

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

function loadReceivedPackets(packets) {
    if (packets) {
        document.querySelector('#received-packets tbody').innerHTML = '';

        const container = document.querySelector("#received-packets tbody");

        container.innerHTML = '';

        const date = new Date();

        packets.forEach((packet) => {
            const element = document.createElement("tr");

            date.setTime(packet.millis);

            const p = date.toUTCString().split(' ')
        
            element.innerHTML = `
                        <td>${p[p.length-2]}</td>
                        <td>${packet.packet}</td>
                        <td>${packet.RSSI}</td>
                        <td>${packet.SNR}</td>
                    `;

            container.appendChild(element);
        })
    }

    setTimeout(fetchReceivedPackets, 15000);
}

function fetchReceivedPackets() {
    fetch("/received-packets.json")
    .then((response) => response.json())
    .then((packets) => {
        loadReceivedPackets(packets);
    })
    .catch((err) => {
        console.error(err);

        console.error(`Failed to load received packets`);
    });
}

document.querySelector('a[href="/received-packets"]').addEventListener('click', function (e) {
    e.preventDefault();

    document.getElementById('received-packets').classList.remove('d-none');
    document.getElementById('configuration').classList.add('d-none');
    
    document.querySelector('button[type=submit]').remove();

    fetchReceivedPackets();
})