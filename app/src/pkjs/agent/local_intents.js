/**
 * Copyright 2026 for Mica.
 *
 * Local intent layer: dictated text is matched here, on the phone, BEFORE
 * it is sent to the AI. A hit is executed locally (watch wakeup alarms,
 * phone-app plugins, one cached weather lookup) and answered with a short
 * templated reply, so the common asks never pay an LLM round trip. A miss
 * returns null and the prompt flows to the AI exactly as before.
 *
 * Selection rule (jian, 2026-11-27): only build intents that invoke a
 * system tool/plugin, produce a real action, or read local data. Anything
 * the AI can answer in one sentence stays with the AI.
 *
 * Pure and dependency-free so it can be unit tested in node:
 * match(text, nowMs) -> intent object | null.
 *
 * Intent shapes:
 *   {kind: 'timer', seconds, name}
 *   {kind: 'alarm', at: Date, name, repeat}   repeat: weekday bitmask,
 *                                             bit0 = Monday .. bit6 = Sunday,
 *                                             0 = one-shot, 127 = daily,
 *                                             31 = Mon-Fri workdays
 *   {kind: 'cancel', target: 'timer'|'alarm'|'all', at: Date|null, name}
 *   {kind: 'list', target: 'timer'|'alarm'|'both'}
 *   {kind: 'plugin', plugin, action, args, doneText, failText}
 *   {kind: 'weather', dayOffset, rainOnly}
 *   {kind: 'coach', brief: boolean}
 */

'use strict';

// Plugin UUIDs verified against Bucky (github.com/blacklava2002/Bucky)
// src/pkjs/actions.js and src/pkjs/music.js on 2026-11-27. Both plugins are
// built into the Pebble phone app; actions go through Pebble.invokeAction,
// the same API lib/quiet.js already uses successfully on the real watch.
var FIND_MY_PHONE = '00000000-0000-0000-0001-000000000008';
var MUSIC = '00000000-0000-0000-0001-000000000003';

var REPEAT_DAILY = 127;
var REPEAT_WEEKDAYS = 31; // Mon..Fri = bits 0..4 = 1+2+4+8+16

// ---------------------------------------------------------------------------
// Normalisation and Chinese number parsing
// ---------------------------------------------------------------------------

var FULLWIDTH = '０１２３４５６７８９';

function normalize(text) {
    var t = String(text || '');
    var out = '';
    for (var i = 0; i < t.length; i++) {
        var ch = t[i];
        var fw = FULLWIDTH.indexOf(ch);
        out += fw >= 0 ? String(fw) : ch;
    }
    var collapsed = out.replace(/[\s，。！？、；：,.!?;:~～·…]+/g, ' ').trim();
    // Pebble dictation often inserts spaces between Chinese tokens
    // ("设置 9 点 30 的闹钟", "定个 10 分钟的倒计时"). The matchers below
    // expect compact text, so drop any space that touches a CJK character
    // or a digit on either side.
    return collapsed
        .replace(/([一-鿿0-9])\s+/g, '$1')
        .replace(/\s+(?=[一-鿿0-9])/g, '');
}

var DIGITS = {'零': 0, '一': 1, '二': 2, '两': 2, '三': 3, '四': 4, '五': 5, '六': 6, '七': 7, '八': 8, '九': 9};

// Parses an Arabic or Chinese cardinal (0..999), or returns null.
// Handles 十, 十一, 二十, 二十三, 一百, 一百二十, 半 (0.5, only alone).
function parseNumber(str) {
    if (str === undefined || str === null || str === '') {
        return null;
    }
    if (/^\d+(\.\d+)?$/.test(str)) {
        return parseFloat(str);
    }
    if (str === '半') {
        return 0.5;
    }
    var total = 0;
    var section = 0;
    var seen = false;
    for (var i = 0; i < str.length; i++) {
        var ch = str[i];
        if (ch === '百') {
            section = (section || 1) * 100;
            total += section;
            section = 0;
            seen = true;
        } else if (ch === '十') {
            section = (section || 1) * 10;
            total += section;
            section = 0;
            seen = true;
        } else if (DIGITS.hasOwnProperty(ch)) {
            section = DIGITS[ch];
            seen = true;
        } else {
            return null;
        }
    }
    return seen ? total + section : null;
}

var NUM = '(\\d+(?:\\.\\d+)?|[零一二两三四五六七八九十百]+)';

// Extracts a duration in seconds from anywhere in the text, or null.
// Supports: 一个半小时, 半小时, X小时Y分钟, X个小时, X分钟, X分, X秒钟, X秒, 一刻钟.
function parseDurationSeconds(t) {
    var m;
    m = new RegExp(NUM + '个半小时').exec(t);
    if (m) {
        var nh = parseNumber(m[1]);
        if (nh !== null) return Math.round(nh * 3600 + 1800);
    }
    if (t.indexOf('半小时') >= 0 || t.indexOf('半个小时') >= 0) {
        return 1800;
    }
    if (t.indexOf('一刻钟') >= 0) {
        return 900;
    }
    m = new RegExp(NUM + '(?:小时|个小时|钟头)' + NUM + '(?:分钟|分)').exec(t);
    if (m) {
        var h = parseNumber(m[1]);
        var mi = parseNumber(m[2]);
        if (h !== null && mi !== null) return Math.round(h * 3600 + mi * 60);
    }
    var seconds = 0;
    var found = false;
    m = new RegExp(NUM + '(?:小时|个小时|钟头)').exec(t);
    if (m) {
        var hv = parseNumber(m[1]);
        if (hv !== null) { seconds += hv * 3600; found = true; }
    }
    m = new RegExp(NUM + '(?:分钟|分)').exec(t);
    if (m) {
        var mv = parseNumber(m[1]);
        if (mv !== null) { seconds += mv * 60; found = true; }
    }
    m = new RegExp(NUM + '(?:秒钟|秒)').exec(t);
    if (m) {
        var sv = parseNumber(m[1]);
        if (sv !== null) { seconds += sv; found = true; }
    }
    return found ? Math.round(seconds) : null;
}

// ---------------------------------------------------------------------------
// Clock-time parsing
// ---------------------------------------------------------------------------

var WEEKDAY_CHARS = {'一': 0, '二': 1, '三': 2, '四': 3, '五': 4, '六': 5, '日': 6, '天': 6};

// Finds a clock time. Returns {hour, minute, dayOffset, repeat, consumed}
// where dayOffset is null when no explicit day was named (caller decides),
// or null when there is no clock time in the text.
function parseClock(t, now) {
    var dayOffset = null;
    var repeat = 0;
    var rest = t;

    if (/每天|每日/.test(rest)) {
        repeat = REPEAT_DAILY;
    } else if (/工作日/.test(rest)) {
        repeat = REPEAT_WEEKDAYS;
    } else {
        var wm = /(下)?(?:周|星期|礼拜)([一二三四五六日天])/.exec(rest);
        if (wm) {
            var targetDay = WEEKDAY_CHARS[wm[2]]; // 0 = Monday
            var todayDay = (now.getDay() + 6) % 7;
            var delta = (targetDay - todayDay + 7) % 7;
            if (wm[1] === '下') {
                // 下周一: the Monday of next week, a one-shot.
                delta += 7;
            } else if (rest.indexOf('每') >= 0) {
                // 每周一: weekly recurring.
                repeat = 1 << targetDay;
            }
            dayOffset = delta;
        }
    }
    if (dayOffset === null) {
        if (/后天/.test(rest)) dayOffset = 2;
        else if (/明天|明早/.test(rest)) dayOffset = 1;
        else if (/今天|今晚|今早/.test(rest)) dayOffset = 0;
    }

    var m = new RegExp(NUM + '点半').exec(rest);
    var hour = null;
    var minute = 0;
    if (m) {
        hour = parseNumber(m[1]);
        minute = 30;
    } else {
        m = new RegExp(NUM + '点' + NUM + '分').exec(rest);
        if (m) {
            hour = parseNumber(m[1]);
            minute = parseNumber(m[2]);
        } else if ((m = new RegExp(NUM + '点' + NUM).exec(rest))) {
            // Bare minutes without 分 ("9点30", "九点三十"): spoken Chinese
            // drops the 分 all the time. Without this branch the trailing
            // number was silently ignored and the alarm landed on the hour.
            hour = parseNumber(m[1]);
            minute = parseNumber(m[2]);
        } else {
            m = new RegExp(NUM + '点').exec(rest);
            if (m) {
                hour = parseNumber(m[1]);
            } else {
                m = /(\d{1,2}):(\d{2})/.exec(rest);
                if (m) {
                    hour = parseInt(m[1], 10);
                    minute = parseInt(m[2], 10);
                }
            }
        }
    }
    if (hour === null || hour === undefined || isNaN(hour)) {
        return null;
    }
    if (/凌晨/.test(rest)) {
        if (hour === 12) hour = 0;
    } else if (/下午|晚上|今晚|夜里/.test(rest)) {
        if (hour < 12) hour += 12;
    } else if (/中午/.test(rest)) {
        if (hour < 11) hour += 12;
    }
    if (hour > 23 || minute > 59) {
        return null;
    }
    return {hour: Math.floor(hour), minute: Math.floor(minute), dayOffset: dayOffset, repeat: repeat};
}

function dateAt(now, dayOffset, hour, minute) {
    var d = new Date(now.getFullYear(), now.getMonth(), now.getDate() + dayOffset, hour, minute, 0, 0);
    return d;
}

// Text after a 提醒我/叫我 marker, cleaned up, or null.
function extractName(t, markers) {
    for (var i = 0; i < markers.length; i++) {
        var idx = t.indexOf(markers[i]);
        if (idx >= 0) {
            var name = t.substring(idx + markers[i].length).replace(/^[\s的去一下吧哦了]+/, '').replace(/[\s吧哦了]+$/, '').trim();
            return name.length > 0 ? name : null;
        }
    }
    return null;
}

// ---------------------------------------------------------------------------
// Intent matchers (order matters: first hit wins)
// ---------------------------------------------------------------------------

function matchPlugin(t) {
    if (/找到了手机|手机找到了|别响了|停止找手机|手机别响/.test(t)) {
        return {kind: 'plugin', plugin: FIND_MY_PHONE, action: 'stop', args: null,
            doneText: '已停止响铃', failText: '没能联系上手机应用'};
    }
    if (/找手机|找一下手机|手机在哪|手机找不到了|让手机响|手机响一下|找不到手机/.test(t)) {
        return {kind: 'plugin', plugin: FIND_MY_PHONE, action: 'ring', args: null,
            doneText: '正在让手机响铃，响约一分钟，静音也会响。找到后说「找到了手机」可以停。',
            failText: '没能联系上手机应用'};
    }
    if (/下一首|下个歌|切歌/.test(t)) {
        return {kind: 'plugin', plugin: MUSIC, action: 'next_track', args: null,
            doneText: '已切到下一首', failText: '音乐控制失败'};
    }
    if (/上一首|上个歌/.test(t)) {
        return {kind: 'plugin', plugin: MUSIC, action: 'previous_track', args: null,
            doneText: '已切到上一首', failText: '音乐控制失败'};
    }
    if (/暂停音乐|暂停播放|停止音乐|暂停$/.test(t) || t === '暂停') {
        return {kind: 'plugin', plugin: MUSIC, action: 'set_playing', args: {on: false},
            doneText: '已暂停', failText: '音乐控制失败'};
    }
    if (/播放音乐|继续播放|恢复播放|放音乐/.test(t)) {
        return {kind: 'plugin', plugin: MUSIC, action: 'set_playing', args: {on: true},
            doneText: '已继续播放', failText: '音乐控制失败'};
    }
    return null;
}

function matchCancel(t, now) {
    if (!/取消|删掉|删除|关掉|不要了/.test(t)) {
        return null;
    }
    // Targets: 'timer' / 'alarm' (one, or the one at a named time),
    // 'timers' / 'alarms' (every one of that kind), 'all' (everything).
    var all = /所有|全部|都取消|都删/.test(t);
    if (/番茄钟/.test(t)) {
        return {kind: 'cancel', target: 'timer', name: '番茄钟'};
    }
    var hasTimer = /计时器|倒计时|定时器/.test(t);
    var hasAlarm = /闹钟|提醒/.test(t);
    if (hasTimer && hasAlarm) {
        return {kind: 'cancel', target: 'all', name: null};
    }
    if (hasTimer) {
        return {kind: 'cancel', target: all ? 'timers' : 'timer', name: null};
    }
    if (hasAlarm) {
        var clock = parseClock(t, now);
        return {kind: 'cancel', target: all ? 'alarms' : 'alarm',
            at: clock ? dateAt(now, clock.dayOffset || 0, clock.hour, clock.minute) : null, name: null};
    }
    if (all) {
        return {kind: 'cancel', target: 'all', name: null};
    }
    return null;
}

function matchList(t) {
    var asks = /查看|看看|列出|显示|有哪些|有什么|查一下|列表/.test(t);
    if (!asks && !/闹钟列表|计时器列表/.test(t)) {
        return null;
    }
    var timer = /计时器|倒计时|定时器/.test(t);
    var alarm = /闹钟/.test(t);
    if (timer && alarm) return {kind: 'list', target: 'both'};
    if (timer) return {kind: 'list', target: 'timer'};
    if (alarm) return {kind: 'list', target: 'alarm'};
    return null;
}

function matchPomodoro(t) {
    if (!/番茄钟|番茄工作法/.test(t)) {
        return null;
    }
    if (/什么|介绍|原理|意思/.test(t)) {
        return null; // a question about the technique, not a start request
    }
    var seconds = parseDurationSeconds(t);
    return {kind: 'timer', seconds: seconds || 25 * 60, name: '番茄钟'};
}

function matchTimer(t) {
    if (!/计时器|倒计时|定时器|计时/.test(t)) {
        return null;
    }
    var seconds = parseDurationSeconds(t);
    if (!seconds || seconds <= 0) {
        return null;
    }
    return {kind: 'timer', seconds: seconds, name: extractName(t, ['叫做', '叫'])};
}

// "X分钟后提醒我Y" / "X分钟后叫我" -> a ringing watch alarm named Y.
function matchDelayedReminder(t, now) {
    if (!/提醒我|叫我|提醒/.test(t)) {
        return null;
    }
    var seconds = parseDurationSeconds(t);
    if (!seconds || seconds <= 0) {
        return null;
    }
    if (!/后/.test(t)) {
        return null;
    }
    return {kind: 'alarm', at: new Date(now.getTime() + seconds * 1000),
        name: extractName(t, ['提醒我', '叫我', '提醒']), repeat: 0, delayedSeconds: seconds};
}

function matchAlarm(t, now) {
    var clock = parseClock(t, now);
    if (!clock) {
        return null;
    }
    var hasKeyword = /闹钟|叫我|叫醒|起床|提醒/.test(t) || clock.repeat !== 0 || clock.dayOffset !== null;
    if (!hasKeyword) {
        return null;
    }
    var at;
    if (clock.repeat && clock.dayOffset === null) {
        // Recurring with no explicit start day: first occurrence is the
        // next day whose weekday is in the mask and whose time is future.
        at = null;
        for (var offset = 0; offset < 8; offset++) {
            var candidate = dateAt(now, offset, clock.hour, clock.minute);
            var bit = 1 << ((candidate.getDay() + 6) % 7);
            if (candidate.getTime() > now.getTime() && (clock.repeat & bit)) {
                at = candidate;
                break;
            }
        }
        if (!at) {
            at = dateAt(now, 1, clock.hour, clock.minute);
        }
    } else {
        at = dateAt(now, clock.dayOffset || 0, clock.hour, clock.minute);
        if (clock.dayOffset === null && at.getTime() <= now.getTime()) {
            at = dateAt(now, 1, clock.hour, clock.minute);
        }
        if (clock.repeat && at.getTime() <= now.getTime()) {
            for (var o2 = 1; o2 < 9; o2++) {
                var c2 = dateAt(now, o2, clock.hour, clock.minute);
                if (c2.getTime() > now.getTime() && (clock.repeat & (1 << ((c2.getDay() + 6) % 7)))) {
                    at = c2;
                    break;
                }
            }
        }
    }
    return {kind: 'alarm', at: at, name: extractName(t, ['提醒我', '叫我']), repeat: clock.repeat};
}

// Common other-city names: a weather ask naming one of these goes to the
// AI (which geocodes); the local path only answers phone-location weather.
// Suzhou is deliberately absent: home base, the phone is there.
var OTHER_CITIES = ['北京', '上海', '广州', '深圳', '杭州', '成都', '武汉', '西安', '南京', '重庆', '天津', '香港', '台北', '东京', '纽约', '伦敦', '巴黎', '首尔'];

function matchWeather(t) {
    var weatherish = /天气|下雨|下雪|多少度|气温|冷不冷|热不热|带伞|有雨/.test(t);
    if (!weatherish) {
        return null;
    }
    for (var i = 0; i < OTHER_CITIES.length; i++) {
        if (t.indexOf(OTHER_CITIES[i]) >= 0) {
            return null;
        }
    }
    // Long or place-specific questions keep going to the AI, which has the
    // geocoding weather tool; the local path answers phone-location weather
    // for short, day-scoped asks only.
    var scoped = /今天|今日|明天|明早|后天|现在|今晚/.test(t) || t.length <= 8;
    if (!scoped) {
        return null;
    }
    var dayOffset = 0;
    if (/后天/.test(t)) dayOffset = 2;
    else if (/明天|明早/.test(t)) dayOffset = 1;
    return {kind: 'weather', dayOffset: dayOffset, rainOnly: /下雨|带伞|有雨/.test(t)};
}

function matchCoach(t) {
    if (/今日简报|健康简报|今日健康|今天的简报|每日简报/.test(t)) {
        return {kind: 'coach', brief: true};
    }
    // General health-knowledge questions ("什么是心率变异性", "高血压怎么办",
    // "心率多少正常") need no local data: leave them to the AI.
    if (/什么是|什么叫|是什么|啥是|怎么办|怎么治|如何治疗|正常值|多少正常|标准多少/.test(t)) {
        return null;
    }
    var asks = /怎么样|怎样|如何|多少|吗|好不好|够不够|看看|看一下|查一下|帮我看/.test(t);
    if (!asks) {
        return null;
    }
    var metric = /睡眠|睡得|步数|走了多少|心率|锻炼|运动量|活动量|卡路里|跑了多少/.test(t);
    if (metric) {
        return {kind: 'coach', brief: false};
    }
    // Broad self-health phrasings ("我的健康数据怎么样", "看看我的身体状况"):
    // jian asked exactly this and the narrow metric list missed it, so the
    // AI refused with "没有接入". A data/self noun plus a question tone is
    // enough; bare 健康 alone is not (avoids "如何保持健康" style asks).
    var broad = /健康数据|身体数据|健康状况|身体状况|身体情况|我的身体|我身体|我的健康/.test(t);
    if (broad) {
        return {kind: 'coach', brief: false};
    }
    return null;
}

function match(text, nowMs) {
    var t = normalize(text);
    if (!t) {
        return null;
    }
    var now = nowMs ? new Date(nowMs) : new Date();
    return matchPlugin(t) ||
        matchCancel(t, now) ||
        matchList(t) ||
        matchPomodoro(t) ||
        matchDelayedReminder(t, now) ||
        matchTimer(t) ||
        matchAlarm(t, now) ||
        matchWeather(t) ||
        matchCoach(t) ||
        null;
}

exports.match = match;
exports.normalize = normalize;
exports.parseNumber = parseNumber;
exports.parseDurationSeconds = function(text) { return parseDurationSeconds(normalize(text)); };
exports.parseClock = function(text, nowMs) { return parseClock(normalize(text), nowMs ? new Date(nowMs) : new Date()); };
exports.FIND_MY_PHONE = FIND_MY_PHONE;
exports.MUSIC = MUSIC;
exports.REPEAT_DAILY = REPEAT_DAILY;
exports.REPEAT_WEEKDAYS = REPEAT_WEEKDAYS;
