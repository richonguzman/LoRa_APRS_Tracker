// Custom scripts

let currentSettings = null;

function escapeHtml(value) {
    return String(value)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#39;");
}

const BANDWIDTHS = [[62500, "62.5 kHz"], [125000, "125 kHz"], [250000, "250 kHz"], [500000, "500 kHz"]];
const SPREADING_FACTORS = [7, 8, 9, 10, 11, 12].map((sf) => [sf, "SF" + sf]);
const CODING_RATES = [5, 6, 7, 8].map((cr) => [cr, "4/" + cr]);

// A dropdown from [value, label] pairs. A stored value that is not in the list (for example 7800 from an
// old file) is shown as "choose" and blocks saving until the operator picks one.
function selectHtml(id, value, options) {
    const known = options.some(([v]) => v === Number(value));
    let html = `<select class="form-select form-select-sm" name="${id}" id="${id}" required>`;
    if (!known) html += `<option value="" selected disabled>${escapeHtml(value)}: choose</option>`;
    options.forEach(([v, label]) => {
        html += `<option value="${v}"${Number(value) === v ? " selected" : ""}>${label}</option>`;
    });
    return html + "</select>";
}

// The firmware gives unnamed entries these names, so an old backup file shows the same ones.
function defaultName(prefix, index) {
    return prefix + (index + 1);
}

// Frequency shown as MHz with at least 3 and at most 6 decimals, from integer hertz so nothing is rounded.
function formatMHz(hz) {
    if (!Number.isFinite(hz)) return "";
    const whole = Math.floor(hz / 1e6);
    let fraction = String(hz % 1e6).padStart(6, "0");
    while (fraction.length > 3 && fraction.endsWith("0")) fraction = fraction.slice(0, -1);
    return whole + "." + fraction;
}

// Text typed in the MHz box -> integer hertz, or NaN. Exact for up to 6 decimals.
function mhzToHz(text) {
    const match = /^\s*(\d+)(?:[.,](\d{0,6}))?\s*$/.exec(String(text));
    if (match) return Number(match[1]) * 1e6 + Number((match[2] || "").padEnd(6, "0"));
    const approx = Math.round(Number(text) * 1e6);
    return Number.isFinite(approx) && String(text).trim() !== "" ? approx : NaN;
}

// The tracker still receives hertz: every MHz box also sends its value as ".frequency".
function addFrequenciesInHz(formData) {
    document.querySelectorAll('input[name$=".frequencyMHz"]').forEach((el) => {
        formData.set(el.name.replace(".frequencyMHz", ".frequency"), String(mhzToHz(el.value)));
    });
}

// One message per entry, or "" when the name is fine. Names must be filled in and different (case ignored).
function nameProblems(values) {
    const seen = new Map();
    return values.map((value, index) => {
        const key = String(value).trim().toLowerCase();
        if (key === "") return "Name cannot be empty";
        if (seen.has(key)) return "Name already used by entry " + seen.get(key);
        seen.set(key, index + 1);
        return "";
    });
}

function validateNames() {
    ['input[name$=".profileLabel"]', 'input[name^="lora."][name$=".name"]'].forEach((selector) => {
        const inputs = Array.from(document.querySelectorAll(selector));
        const problems = nameProblems(inputs.map((el) => el.value));
        inputs.forEach((el, i) => el.setCustomValidity(problems[i]));
    });
}

function backupSettings() {
    const data =
        "data:text/json;charset=utf-8," +
        encodeURIComponent(JSON.stringify(currentSettings));
    const a = document.createElement("a");
    a.setAttribute("href", data);
    a.setAttribute("download", "TrackerConfigurationBackup.json");
    a.click();
}

// Mic-E status codes. The three digits are what the firmware's APRSPacketLib accepts: "000" to "111".
// An empty value (None) sends normal position packets.
const MICE_CODES = [
    ["", "None"],
    ["111", "Off Duty"],
    ["110", "En Route"],
    ["101", "In Service"],
    ["100", "Returning"],
    ["011", "Committed"],
    ["010", "Special"],
    ["001", "Priority"],
    ["000", "Emergency"],
];

// Anything the firmware would not accept (for example "Y" typed into the old text box) shows as None,
// which is what the firmware does with it. No placeholder: the field must always be sent with the form.
function miceSelectHtml(id, value) {
    const known = typeof value === "string" && MICE_CODES.some(([code]) => code === value);
    let html = `<select class="form-select form-select-sm" name="${id}" id="${id}">`;
    MICE_CODES.forEach(([code, label]) => {
        const chosen = known ? code === value : code === "";
        html += `<option value="${code}"${chosen ? " selected" : ""}>${label}</option>`;
    });
    return html + "</select>";
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
        beaconElement.classList.add("row", "g-0", "beacons", "border-bottom", "py-2");

        beaconElement.innerHTML = `
            <div class="d-flex align-items-start pt-3" style="flex: 0 0 50px; width: 50px;">
                <strong>${index + 1})</strong>
            </div>
            <div class="col">
                            <div class="row g-2 mb-2">
                    <div class="form-floating col-12 col-md-6">
                        <input
                            type="text"
                            class="form-control form-control-sm"
                            name="beacons.${index}.profileLabel"
                            id="beacons.${index}.profileLabel"
                            value="${escapeHtml(beacons.profileLabel || defaultName("PROFILE", index))}"
                            maxlength="8"
                            pattern="[ -~]+"
                            required>
                        <label for="beacons.${index}.profileLabel">Profile Label (8 max)</label>
                    </div>
                </div>
                <div class="row g-2 mb-2">
                    <div class="form-floating col-12 col-md-6">
                        <input 
                            type="text" 
                            class="form-control form-control-sm" 
                            name="beacons.${index}.callsign" 
                            id="beacons.${index}.callsign" 
                            required
                            value="${beacons.callsign}"
                            oninput="this.value = this.value.toUpperCase();">
                        <label for="beacons.${index}.callsign">Callsign</label>
                    </div>
                    <div class="form-floating col-6 col-sm-4 col-md-2">
                        <input 
                            type="text" 
                            class="form-control form-control-sm" 
                            name="beacons.${index}.symbol" 
                            id="beacons.${index}.symbol" 
                            value="${beacons.symbol}">
                        <label for="beacons.${index}.symbol">Symbol</label>
                    </div>
                    <div class="form-floating col-6 col-sm-4 col-md-2">
                        <input 
                            type="text" 
                            class="form-control form-control-sm" 
                            name="beacons.${index}.overlay" 
                            id="beacons.${index}.overlay" 
                            value="${beacons.overlay}">
                        <label for="beacons.${index}.overlay">Overlay</label>
                    </div>
                     <div class="form-floating col-12 col-sm-4 col-md-2">
                        ${miceSelectHtml(`beacons.${index}.micE`, beacons.micE)}
                        <label for="beacons.${index}.micE">Mic-E</label>
                    </div>
                </div>
                <div class="row g-2 mb-2">
                    <div class="form-floating col-12">
                        <input 
                            type="text" 
                            class="form-control form-control-sm" 
                            name="beacons.${index}.comment" 
                            id="beacons.${index}.comment" 
                            value="${beacons.comment}">
                        <label for="beacons.${index}.comment">Comment</label>
                    </div>
                </div>
                <div class="row g-2 mb-2">
                    <div class="col-12 col-sm-6">
                        <div class="form-check form-switch">
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
                    </div>
                    <div class="col-12 col-sm-6">
                        <div class="form-check form-switch">
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
                    </div>
                </div>
                <div class="row g-2 mb-2">
                    <div class="col-12 col-md-6">
                        <label for="beacons.${index}.smartBeaconSetting" class="form-label"><small>Smart Beacon Setting</small></label>
                        <select name="beacons.${index}.smartBeaconSetting" id="beacons.${index}.smartBeaconSetting" class="form-control">
                            <option value="0" ${beacons.smartBeaconSetting == 0 ? 'selected' : ''}>SLOW</option>
                            <option value="1" ${beacons.smartBeaconSetting == 1 ? 'selected' : ''}>MEDIUM</option>
                            <option value="2" ${beacons.smartBeaconSetting == 2 ? 'selected' : ''}>FAST</option>
                        </select>
                    </div>
                </div>
                <div class="row g-2">
                    <div class="form-floating col-12 col-sm-12">
                        <input 
                            type="text" 
                            class="form-control form-control-sm" 
                            name="beacons.${index}.status" 
                            id="beacons.${index}.status" 
                            value="${beacons.status}">
                        <label for="beacons.${index}.status">Status</label>
                    </div>
                </div>
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
    document.getElementById("cadActive").checked                        = settings.other.cadActive;
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
    document.getElementById("bluetooth.useBLE").value                   = settings.bluetooth.useBLE ? "1" : "0";
    document.getElementById("bluetooth.useKISS").value                  = settings.bluetooth.useKISS ? "1" : "0";
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
                <strong>${index + 1})</strong>
            </div>
            <div class="form-floating col-6 col-md-2 px-1 mb-2">
                <input
                    type="text"
                    class="form-control form-control-sm"
                    name="lora.${index}.name"
                    id="lora.${index}.name"
                    maxlength="8"
                    pattern="[ -~]+"
                    title="Up to 8 letters, digits or symbols"
                    required
                    value="${escapeHtml(lora.name || defaultName("LORA", index))}">
                <label for="lora.${index}.name">Name</label>
            </div>
            <div class="form-floating col-6 col-md-3 px-1 mb-2">
                <input
                    type="number"
                    step="0.0001"
                    class="form-control form-control-sm"
                    name="lora.${index}.frequencyMHz"
                    id="lora.${index}.frequencyMHz"
                    min="137"
                    max="1020"
                    required
                    value="${formatMHz(lora.frequency)}">
                <label for="lora.${index}.frequencyMHz">Frequency (MHz)</label>
            </div>
            <div class="form-floating col-6 col-sm-4 col-md-2 px-1 mb-2">
                ${selectHtml(`lora.${index}.spreadingFactor`, lora.spreadingFactor, SPREADING_FACTORS)}
                <label for="lora.${index}.spreadingFactor">SF</label>
            </div>
            <div class="form-floating col-6 col-sm-4 col-md-2 px-1 mb-2">
                ${selectHtml(`lora.${index}.codingRate4`, lora.codingRate4, CODING_RATES)}
                <label for="lora.${index}.codingRate4">CR</label>
            </div>
            <div class="form-floating col-12 col-sm-4 col-md-2 px-1 mb-2">
                ${selectHtml(`lora.${index}.signalBandwidth`, lora.signalBandwidth, BANDWIDTHS)}
                <label for="lora.${index}.signalBandwidth">BW</label>
            </div>
            <div class="form-floating col-6 col-md-2 offset-md-1 px-1 mb-2">
                <input
                    type="number"
                    class="form-control form-control-sm"
                    name="lora.${index}.power"
                    id="lora.${index}.power"
                    value="${lora.power}"
                    min="2"
                    max="22"
                    required>
                <label for="lora.${index}.power">Power (dBm)</label>
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

    validateNames();
    clampOutOfRangeInputs();
}

function showToast(message) {
    const el = document.querySelector('#toast');

    el.querySelector('.toast-body').innerHTML = message;

    (new bootstrap.Toast(el)).show();
}

function clampOutOfRangeInputs() {
    const fixed = [];

    document.querySelectorAll("#configuration input[type=number]").forEach((el) => {
        if (el.disabled || el.value === "") return;

        const value = parseFloat(el.value);
        if (isNaN(value)) return;

        const min = el.min !== "" ? parseFloat(el.min) : null;
        const max = el.max !== "" ? parseFloat(el.max) : null;

        let clamped = value;
        if (max !== null && value > max) clamped = max;
        if (min !== null && value < min) clamped = min;

        if (clamped !== value) {
            el.value = clamped;
            fixed.push((el.name || el.id) + ": " + value + " &rarr; " + clamped);
        }
    });

    if (fixed.length) {
        showToast("Stored values outside the allowed range, adjusted:<br>" + fixed.join("<br>")
                  + "<br><small>Save to keep them.</small>");
    }
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
const BluetoothUseBle           = document.querySelector('select[name="bluetooth.useBLE"]');
const BluetoothUseKiss          = document.querySelector('select[name="bluetooth.useKISS"]');
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

    saveModal.show();
    try {
        const data = new FormData(form);
        addFrequenciesInHz(data);
        const response = await fetch(form.action, {
            method: form.method,
            body: data,
        });
        if (response.status === 400) {           // the tracker refused the values and saved nothing
            saveModal.hide();
            showToast("<b>Not saved:</b> " + escapeHtml(await response.text()));
            return;
        }
    } catch (err) {
        // the tracker restarts while its answer is on the way: that is how a normal save ends
    }
    setTimeout(checkConnection, 2000);
});

let invalidFieldReported = false;

form.addEventListener("invalid", function (event) {
    const el = event.target;

    if (!invalidFieldReported) {          // only the first one: the rest would pile up toasts
        invalidFieldReported = true;
        setTimeout(() => { invalidFieldReported = false; }, 500);

        const section = el.closest(".panel-section");
        if (section) {
            const link = document.querySelector('.side-link[data-target="' + section.id + '"]');
            if (link) link.click();       // reuses the sidebar switcher
        }

        showToast("<b>" + (el.name || el.id) + "</b>: " + (el.validationMessage || "invalid value"));

        setTimeout(() => {
            el.focus();
            el.scrollIntoView({ block: "center", behavior: "smooth" });
        }, 0);
    }
}, true);

document.getElementById("configuration").addEventListener("input", (event) => {
    if (event.target.matches('input[name$=".profileLabel"], input[name$=".name"]')) validateNames();
});

fetchSettings();