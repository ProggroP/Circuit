// PebbleKit JS: Clay-Einstellungen plus Wetter von Open-Meteo.
// Die Uhr fragt alle 30 Minuten per REQUEST_WEATHER an; beim Start und nach
// dem Speichern der Einstellungen wird von hier aus selbst abgefragt.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

var clay = new Clay(clayConfig);

var WEATHER_URL = 'https://api.open-meteo.com/v1/forecast';
var REQUEST_TIMEOUT_MS = 15000;
var SEND_DELAY_MS = 1500;   // nach Clays eigener Nachricht, sonst "busy"

// Zustandscodes, muessen zu COND_* in circuit.c passen
var COND_CLEAR = 0, COND_PARTLY = 1, COND_CLOUDY = 2, COND_FOG = 3,
    COND_RAIN = 4, COND_SNOW = 5, COND_STORM = 6,
    COND_CLEAR_NIGHT = 7, COND_PARTLY_NIGHT = 8;

// WMO-Wettercodes auf die neun Symbole der Uhr
function conditionFor(code, isDay) {
    var c;
    if (code === 0)                          c = COND_CLEAR;
    else if (code === 1 || code === 2)       c = COND_PARTLY;
    else if (code === 3)                     c = COND_CLOUDY;
    else if (code === 45 || code === 48)     c = COND_FOG;
    else if (code >= 71 && code <= 77)       c = COND_SNOW;
    else if (code === 85 || code === 86)     c = COND_SNOW;
    else if (code >= 95)                     c = COND_STORM;
    else                                     c = COND_RAIN;
    if (!isDay && c === COND_CLEAR)  c = COND_CLEAR_NIGHT;
    if (!isDay && c === COND_PARTLY) c = COND_PARTLY_NIGHT;
    return c;
}

function getStoredSettings() {
    try {
        return JSON.parse(localStorage.getItem('clay-settings') || '{}');
    } catch (e) {
        return {};
    }
}

// Keine Schnittstelle zur Einheiten-Einstellung der Pebble-App, also die
// Region des Telefons auswerten -- wer damit falsch liegt, stellt es um.
var FAHRENHEIT_REGIONS = /-(US|BS|BZ|KY|FM|MH|PW|LR)\b/i;

function useFahrenheit() {
    var unit = getStoredSettings().TEMP_UNIT;
    if (unit === 'celsius')    return false;
    if (unit === 'fahrenheit') return true;
    var locale = (navigator && navigator.language) || '';
    return FAHRENHEIT_REGIONS.test(locale);
}

function fetchWeather(lat, lon) {
    var fahrenheit = useFahrenheit();
    var url = WEATHER_URL
            + '?latitude='  + lat.toFixed(4)
            + '&longitude=' + lon.toFixed(4)
            + '&current=temperature_2m,weather_code,is_day'
            + '&temperature_unit=' + (fahrenheit ? 'fahrenheit' : 'celsius');

    var request = new XMLHttpRequest();
    request.open('GET', url, true);
    request.timeout = REQUEST_TIMEOUT_MS;
    request.onload = function() {
        if (request.status !== 200) {
            console.log('weather request failed: HTTP ' + request.status);
            return;
        }
        try {
            var current = (JSON.parse(request.responseText) || {}).current;
            if (!current || current.temperature_2m === undefined) return;
            Pebble.sendAppMessage({
                'TEMP': Math.round(current.temperature_2m),
                'CONDITION': conditionFor(current.weather_code, current.is_day)
            }, null, function() { console.log('weather: send failed'); });
        } catch (e) {
            console.log('weather not readable: ' + e);
        }
    };
    request.onerror   = function() { console.log('weather: network error'); };
    request.ontimeout = function() { console.log('weather: timeout'); };
    request.send();
}

function refreshWeather() {
    navigator.geolocation.getCurrentPosition(
        function(pos) { fetchWeather(pos.coords.latitude, pos.coords.longitude); },
        function(err) { console.log('location unavailable: ' + err.message); },
        { enableHighAccuracy: false, maximumAge: 1800000, timeout: 15000 }
    );
}

Pebble.addEventListener('ready', function() {
    refreshWeather();
});

Pebble.addEventListener('appmessage', function(e) {
    if (e.payload && e.payload.REQUEST_WEATHER !== undefined) refreshWeather();
});

// Feuert nach Clays Handler; die Einheit kann sich geaendert haben
Pebble.addEventListener('webviewclosed', function(e) {
    if (e && e.response && e.response !== 'CANCELLED') {
        setTimeout(refreshWeather, SEND_DELAY_MS);
    }
});
