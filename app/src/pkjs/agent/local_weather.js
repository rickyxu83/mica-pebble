/**
 * Copyright 2026 for Mica.
 *
 * Local weather for the intent layer: one lightweight Open-Meteo request
 * (no API key, no Mica server, same provider as the AI weather tool),
 * answered with a short templated Chinese sentence — never an LLM round
 * trip. Results are cached in localStorage for about 30 minutes, keyed by
 * a coarse location cell, so repeat asks inside the window are instant.
 */

'use strict';

var config = require('../config');
var location = require('../location');

var CACHE_KEY = 'mica_local_weather_v1';
var CACHE_MS = 30 * 60 * 1000;

var DESCRIPTIONS = {
    0: '晴', 1: '晴', 2: '多云', 3: '阴',
    45: '有雾', 48: '有雾',
    51: '毛毛雨', 53: '毛毛雨', 55: '毛毛雨',
    56: '冻雨', 57: '冻雨',
    61: '小雨', 63: '中雨', 65: '大雨', 66: '冻雨', 67: '冻雨',
    71: '小雪', 73: '中雪', 75: '大雪', 77: '雪粒',
    80: '阵雨', 81: '阵雨', 82: '暴雨',
    85: '阵雪', 86: '阵雪',
    95: '雷阵雨', 96: '雷阵雨', 99: '雷阵雨'
};

function dayWord(dayOffset) {
    return dayOffset === 0 ? '今天' : (dayOffset === 1 ? '明天' : '后天');
}

// Pure formatter over an Open-Meteo forecast response (daily arrays with
// forecast_days >= dayOffset + 1). Exported for node tests.
function formatReport(json, dayOffset, rainOnly) {
    var daily = (json && json.daily) || {};
    var current = (json && json.current) || {};
    var code = daily.weather_code ? daily.weather_code[dayOffset] : undefined;
    var desc = DESCRIPTIONS[code] || '天气一般';
    var high = daily.temperature_2m_max ? Math.round(daily.temperature_2m_max[dayOffset]) : null;
    var low = daily.temperature_2m_min ? Math.round(daily.temperature_2m_min[dayOffset]) : null;
    var rain = daily.precipitation_probability_max ? Math.round(daily.precipitation_probability_max[dayOffset]) : null;
    var word = dayWord(dayOffset);
    if (rainOnly) {
        if (rain === null) {
            return word + desc + '，暂时查不到降雨概率。';
        }
        if (rain >= 50) {
            return word + '降雨概率 ' + rain + '%，很可能下雨，出门记得带伞。';
        }
        if (rain >= 25) {
            return word + '降雨概率 ' + rain + '%，有下雨的可能，带把伞稳妥些。';
        }
        return word + '降雨概率只有 ' + rain + '%，基本不会下雨。';
    }
    var parts = [word + desc];
    if (dayOffset === 0 && typeof current.temperature_2m === 'number') {
        parts.push('现在 ' + Math.round(current.temperature_2m) + '°');
    }
    if (high !== null && low !== null) {
        parts.push('最高 ' + high + '°、最低 ' + low + '°');
    }
    if (rain !== null) {
        parts.push('降雨概率 ' + rain + '%');
    }
    if (dayOffset === 0 && typeof current.wind_speed_10m === 'number') {
        parts.push('风速 ' + Math.round(current.wind_speed_10m) + ' km/h');
    }
    return parts.join('，') + '。';
}

function loadCache(lat, lon) {
    try {
        var raw = localStorage.getItem(CACHE_KEY);
        if (!raw) return null;
        var entry = JSON.parse(raw);
        if (!entry || Date.now() - entry.at > CACHE_MS) return null;
        // Coarse cell (~1 km) so small GPS drift still hits the cache.
        if (Math.abs(entry.lat - lat) > 0.01 || Math.abs(entry.lon - lon) > 0.01) return null;
        return entry.json;
    } catch (e) {
        return null;
    }
}

function saveCache(lat, lon, json) {
    try {
        localStorage.setItem(CACHE_KEY, JSON.stringify({at: Date.now(), lat: lat, lon: lon, json: json}));
    } catch (e) {
        // Cache is best-effort.
    }
}

function fetchJson(url, callback) {
    var finished = false;
    function finish(err, value) {
        if (finished) return;
        finished = true;
        callback(err, value);
    }
    var req = new XMLHttpRequest();
    req.open('GET', url, true);
    req.timeout = 8000;
    req.setRequestHeader('Accept', 'application/json');
    req.onload = function() {
        if (req.readyState !== 4) return;
        if (req.status < 200 || req.status >= 300) {
            finish(new Error('HTTP ' + req.status));
            return;
        }
        try {
            finish(null, JSON.parse(req.responseText));
        } catch (e) {
            finish(e);
        }
    };
    req.onerror = function() { finish(new Error('network error')); };
    req.ontimeout = function() { finish(new Error('timeout')); };
    req.send();
}

// callback(replyText): always a user-ready Chinese sentence.
function getReport(dayOffset, rainOnly, callback) {
    if (!config.isLocationEnabled()) {
        callback('定位没开：在 Mica 设置里允许定位后，我就能直接报本地天气了。');
        return;
    }
    if (!location.isReady()) {
        location.update();
        callback('暂时拿不到手机定位，稍后再问我一次。');
        return;
    }
    var pos = location.getPos();
    var cached = loadCache(pos.lat, pos.lon);
    if (cached) {
        callback(formatReport(cached, dayOffset, rainOnly));
        return;
    }
    var url = 'https://api.open-meteo.com/v1/forecast' +
        '?latitude=' + encodeURIComponent(pos.lat) +
        '&longitude=' + encodeURIComponent(pos.lon) +
        '&current=temperature_2m,weather_code,wind_speed_10m' +
        '&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max' +
        '&forecast_days=3&timezone=auto';
    fetchJson(url, function(err, json) {
        if (err) {
            callback('天气查询失败（' + (err.message || '网络错误') + '），稍后再试。');
            return;
        }
        saveCache(pos.lat, pos.lon, json);
        callback(formatReport(json, dayOffset, rainOnly));
    });
}

exports.getReport = getReport;
exports.formatReport = formatReport;
