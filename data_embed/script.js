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
            <div class="form-floating col-6 col-md-6 px-1 mb-2" style="margin-left: 50px;">
                <input 
                    type="text" 
                    class="form-control form-control-sm" 
                    name="beacons.${index}.comment" 
                    id="beacons.${index}.comment" 
                    value="${beacons.comment}">
                <label for="beacons.${index}.comment">Comment</label>
            </div>
        `;
        beaconContainer.appendChild(beaconElement);
    });
    //  gpsEcoMode
    //  smartBeaconActive
	//  smartBeaconSetting

    // ADITIONAL STATION CONFIG
    document.getElementById("simplifiedTrackerMode").checked            = settings.other.simplifiedTrackerMode;
    document.getElementById("sendCommentAfterXBeacons").value           = settings.other.sendCommentAfterXBeacons;
    document.getElementById("path").value                               = settings.other.path;
    document.getElementById("nonSmartBeaconRate").value                 = settings.other.nonSmartBeaconRate;
    document.getElementById("rememberStationTime").value                = settings.other.rememberStationTime;
    document.getElementById("standingUpdateTime").value                 = settings.other.standingUpdateTime;
    document.getElementById("sendAltitude").checked                     = settings.other.sendAltitude ;
    document.getElementById("disableGPS").checked                       = settings.other.disableGPS;

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

    // WINLINK
    document.getElementById("winlink.password").value                   = settings.winlink.password;

    // TELEMETRY BME/WX
    document.getElementById("bme.active").checked                       = settings.bme.active;
    document.getElementById("bme.temperatureCorrection").value          = settings.bme.temperatureCorrection.toFixed(1);
    document.getElementById("bme.sendTelemetry").checked                = settings.bme.sendTelemetry;
    
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
    document.getElementById("bluetooth.type").value                     = settings.bluetooth.type;
    
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

const bmeCheckbox = document.querySelector("input[name='bme.active']");

const stationModeSelect = document.querySelector("select[name='stationMode']");

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

/*document.querySelector(".new button").addEventListener("click", function () {
    const trackersContainer = document.querySelector(".list-tracker");

    let trackerCount = document.querySelectorAll(".network").length;

    const trackerElement = document.createElement("div");

    trackerElement.classList.add("row", "network", "border-bottom", "py-2");

    // Increment the name, id, and for attributes
    const attributeName = `beacons.${trackerCount}`;
    trackerElement.innerHTML = `
                <div class="form-floating col-6 col-md-5 px-1 mb-2">
                <input type="text" class="form-control form-control-sm" name="${attributeName}.callsign" id="${attributeName}.callsign" placeholder="" >
                <label for="${attributeName}.callsign">callsign</label>
                </div>
                <div class="form-floating col-6 col-md-5 px-1 mb-2">
                <input type="password" class="form-control form-control-sm" name="${attributeName}.password" id="${attributeName}.password" placeholder="">
                <label for="${attributeName}.password">Passphrase</label>
                </div>
                <div class="col-4 col-md-2 d-flex align-items-center justify-content-end">
                <div class="btn-group" role="group">
                    <button type="button" class="btn btn-sm btn-danger" title="Delete" onclick="return this.parentNode.parentNode.parentNode.remove();"><svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" fill="currentColor" class="bi bi-trash3-fill" viewBox="0 0 16 16">
            <path d="M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5m-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5M4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06m6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528M8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5"/>
        </svg><span class="visually-hidden">Delete</span></button>
                </div>
                </div>
            `;
    trackersContainer.appendChild(trackerElement);

    trackerCount++;

    // Add the new network element to the end of the document
    document.querySelector(".new").before(trackerElement);
});*/

/*document
    .getElementById("action.symbol")
    .addEventListener("change", function () {
        const value = document.getElementById("action.symbol").value;

        document.getElementById("beacon.overlay").value = value[0];
        document.getElementById("beacon.symbol").value = value[1];
    });*/

/*const speedStandards = {
    300: [125, 5, 12],
    244: [125, 6, 12],
    209: [125, 7, 12],
    183: [125, 8, 12],
    610: [125, 8, 10],
    1200: [125, 7, 9],
};

function refreshSpeedStandard() {
    const bw = Number(document.getElementById("lora.signalBandwidth").value);
    const cr4 = Number(document.getElementById("lora.codingRate4").value);
    const sf = Number(document.getElementById("lora.spreadingFactor").value);

    let found = false;

    for (const speed in speedStandards) {
        const standard = speedStandards[speed];

        if (standard[0] !== bw / 1000) continue;
        if (standard[1] !== cr4) continue;
        if (standard[2] !== sf) continue;

        document.getElementById("action.speed").value = speed;
        found = true;

        break;
    }

    if (!found) {
        document.getElementById("action.speed").value = "";
    }
}*/

/*document.getElementById("lora.signalBandwidth").addEventListener("focusout", refreshSpeedStandard);
document.getElementById("lora.codingRate4").addEventListener("focusout", refreshSpeedStandard);
document.getElementById("lora.spreadingFactor").addEventListener("focusout", refreshSpeedStandard);

document.getElementById("action.speed").addEventListener("change", function () {
    const speed = document.getElementById("action.speed").value;

    if (speed !== "") {
        const value = speedStandards[Number(speed)];

        const bw = value[0];
        const cr4 = value[1];
        const sf = value[2];

        document.getElementById("lora.signalBandwidth").value = bw * 1000;
        document.getElementById("lora.codingRate4").value = cr4;
        document.getElementById("lora.spreadingFactor").value = sf;
    }
});*/











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

/*form.addEventListener("submit", async (event) => {
    event.preventDefault();

    //document.getElementById("beacons").value =         document.querySelectorAll(".beacons").length;

    fetch(form.action, {
        method: form.method,
        body: new FormData(form),
    });
    saveModal.show();
    setTimeout(checkConnection, 2000);
});*/

form.addEventListener("submit", async (event) => {
    event.preventDefault();

    // Optional: update the beacons count
    // document.getElementById("beacons").value = document.querySelectorAll(".beacons").length;

    try {
        const response = await fetch(form.action, {
            method: form.method,
            body: new FormData(form),
        });

        if (!response.ok) {
            throw new Error('Form submission failed');
        }
        saveModal.show();
        setTimeout(checkConnection, 2000);

    } catch (error) {
        console.error(error);
        // Optionally handle errors (e.g., show error modal/message)
    }
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