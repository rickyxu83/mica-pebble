// Node tests for the local intent layer: the Chinese intent parser
// (agent/local_intents.js), the executor (agent/local_actions.js) with the
// watch and plugins stubbed, the local weather formatter, and the coach
// health-snapshot assembly. Run: node test/test_local_intents.js
'use strict';

const assert = require('assert');
const path = require('path');
const Module = require('module');

// Stub modules that only exist inside the Pebble build. message_keys gets
// fixed, distinct indices for the two array keys the alarm actions read.
const KEY_INDICES = {GET_ALARM_RESULT: 1000, GET_ALARM_NAME: 2000};
const origLoad = Module._load;
Module._load = function(request, parent, isMain) {
    if (request === 'message_keys') {
        return new Proxy({}, {get: function(t, prop) {
            return Object.prototype.hasOwnProperty.call(KEY_INDICES, prop) ? KEY_INDICES[prop] : 0;
        }});
    }
    if (request === 'package.json') {
        return {version: '0.3.6'};
    }
    return origLoad.apply(this, arguments);
};

// In-memory localStorage with location consent granted.
const store = {'clay-settings': JSON.stringify({LOCATION_ENABLED: true})};
global.localStorage = {
    getItem: k => (k in store ? store[k] : null),
    setItem: (k, v) => { store[k] = String(v); },
    removeItem: k => { delete store[k]; }
};
global.window = {};

// Pebble stub: appmessage listener registry + AppMessage capture with an
// auto-responder that plays the watch for alarm/health requests.
const listeners = [];
const sentMessages = [];
let invokeCalls = [];
let healthResponder = null;
let alarmListResponder = null;

function dispatch(payload) {
    listeners.slice().forEach(function(fn) { fn({payload: payload}); });
}

global.Pebble = {
    addEventListener: function(name, fn) { if (name === 'appmessage') listeners.push(fn); },
    removeEventListener: function(name, fn) {
        const i = listeners.indexOf(fn);
        if (i >= 0) listeners.splice(i, 1);
    },
    getTimelineToken: function(ok) { ok('test-token'); },
    sendAppMessage: function(msg, ok, err) {
        sentMessages.push(msg);
        if (ok) ok();
        if ('SET_ALARM_TIME' in msg || 'CANCEL_ALARM_TIME' in msg) {
            setTimeout(() => dispatch({SET_ALARM_RESULT: 0}), 0);
        } else if ('GET_ALARM_OR_TIMER' in msg) {
            setTimeout(() => dispatch(alarmListResponder(msg.GET_ALARM_OR_TIMER)), 0);
        } else if ('HEALTH_REQUEST' in msg && healthResponder) {
            setTimeout(() => dispatch(healthResponder()), 0);
        }
    },
    invokeAction: function(call) {
        invokeCalls.push(call);
        return Promise.resolve({ok: true});
    },
    platform: 'node'
};

// Geolocation: fixed Suzhou position. (Node ships a getter-only
// navigator global, so define the property instead of assigning.)
Object.defineProperty(global, 'navigator', {value: {
    geolocation: {
        getCurrentPosition: function(success) {
            success({coords: {latitude: 31.2989, longitude: 120.5853, accuracy: 20}});
        }
    },
    language: 'zh-CN'
}, configurable: true});

// XHR stub answering the canned Open-Meteo response.
let xhrCount = 0;
const WEATHER_JSON = JSON.stringify({
    current: {temperature_2m: 26.4, weather_code: 0, wind_speed_10m: 12},
    daily: {
        weather_code: [0, 2, 61],
        temperature_2m_max: [29.2, 27.5, 24.1],
        temperature_2m_min: [21.3, 20.2, 18.9],
        precipitation_probability_max: [10, 30, 80]
    }
});
global.XMLHttpRequest = function() {};
global.XMLHttpRequest.prototype.open = function() {};
global.XMLHttpRequest.prototype.setRequestHeader = function() {};
global.XMLHttpRequest.prototype.send = function() {
    xhrCount++;
    this.status = 200;
    this.readyState = 4;
    this.responseText = WEATHER_JSON;
    if (this.onload) this.onload();
};

const root = path.resolve(__dirname, '../app/src/pkjs');
const intents = require(path.join(root, 'agent/local_intents'));
const localActions = require(path.join(root, 'agent/local_actions'));
const localWeather = require(path.join(root, 'agent/local_weather'));
const coach = require(path.join(root, 'agent/coach'));
const location = require(path.join(root, 'location'));
const sessionModule = require(path.join(root, 'session'));

// Friday 2026-11-27 01:30 local time.
const NOW = new Date(2026, 10, 27, 1, 30, 0).getTime();
let passed = 0;
function ok(name, cond) {
    assert(cond, name);
    passed++;
}

// ---------------------------------------------------------------- parser
function match(text) { return intents.match(text, NOW); }

let m = match('定个10分钟的计时器');
ok('timer 10min', m && m.kind === 'timer' && m.seconds === 600);
m = match('倒计时5分钟');
ok('timer 5min', m && m.kind === 'timer' && m.seconds === 300);
m = match('计时90秒');
ok('timer 90s', m && m.kind === 'timer' && m.seconds === 90);
m = match('定一个半小时的计时器');
ok('timer 1.5h', m && m.kind === 'timer' && m.seconds === 5400);
m = match('倒计时二十分钟');
ok('timer chinese numerals', m && m.kind === 'timer' && m.seconds === 1200);
m = match('10分钟后提醒我开会');
ok('delayed reminder', m && m.kind === 'alarm' && m.name === '开会' &&
    Math.abs(m.at.getTime() - (NOW + 600000)) < 2000);
m = match('20分钟后提醒我');
ok('delayed reminder bare', m && m.kind === 'alarm' && !m.name);
m = match('30分钟后叫我起床');
ok('delayed reminder name', m && m.kind === 'alarm' && m.name === '起床');
m = match('明天早上8点叫我起床');
ok('alarm tomorrow 8', m && m.kind === 'alarm' && m.at.getDate() === 28 && m.at.getHours() === 8 && m.name === '起床');
m = match('7点半叫我');
ok('alarm today 7:30', m && m.kind === 'alarm' && m.at.getDate() === 27 && m.at.getHours() === 7 && m.at.getMinutes() === 30);
m = match('下午3点提醒我开会');
ok('alarm pm', m && m.kind === 'alarm' && m.at.getHours() === 15 && m.name === '开会');
m = match('1点叫我');
ok('alarm past rolls to tomorrow', m && m.kind === 'alarm' && m.at.getDate() === 28 && m.at.getHours() === 1);
m = match('每天早上7点叫我');
ok('alarm daily', m && m.kind === 'alarm' && m.repeat === 127 && m.at.getDate() === 27 && m.at.getHours() === 7);
m = match('工作日8点半闹钟');
ok('alarm weekdays', m && m.kind === 'alarm' && m.repeat === 31 && m.at.getHours() === 8 && m.at.getMinutes() === 30);
m = match('每周一9点提醒我交周报');
ok('alarm weekly mon', m && m.kind === 'alarm' && m.repeat === 1 && m.at.getDate() === 30 && m.at.getHours() === 9);
m = match('下周三10点提醒我开会');
ok('alarm next wed one-shot', m && m.kind === 'alarm' && m.repeat === 0 && m.at.getDate() === 9);
m = match('帮我设置9点半的闹钟');
ok('alarm 9:30 set phrasing', m && m.kind === 'alarm' && m.at.getHours() === 9 && m.at.getMinutes() === 30);
m = match('设置9点30的闹钟');
ok('alarm bare minutes', m && m.kind === 'alarm' && m.at.getHours() === 9 && m.at.getMinutes() === 30);
m = match('九点三十叫我');
ok('alarm bare minutes chinese', m && m.kind === 'alarm' && m.at.getHours() === 9 && m.at.getMinutes() === 30);
m = match('设置 9 点 30 的闹钟');
ok('alarm spaced dictation', m && m.kind === 'alarm' && m.at.getHours() === 9 && m.at.getMinutes() === 30);
m = match('帮我设置 9 点半的闹钟');
ok('alarm spaced half', m && m.kind === 'alarm' && m.at.getHours() === 9 && m.at.getMinutes() === 30);
m = match('定个 10 分钟的倒计时');
ok('timer spaced dictation', m && m.kind === 'timer' && m.seconds === 600);
m = match('番茄钟');
ok('pomodoro default', m && m.kind === 'timer' && m.seconds === 1500 && m.name === '番茄钟');
m = match('番茄钟30分钟');
ok('pomodoro 30', m && m.kind === 'timer' && m.seconds === 1800 && m.name === '番茄钟');
m = match('取消计时器');
ok('cancel timer', m && m.kind === 'cancel' && m.target === 'timer');
m = match('取消7点的闹钟');
ok('cancel alarm at time', m && m.kind === 'cancel' && m.target === 'alarm' && m.at && m.at.getHours() === 7);
m = match('取消所有闹钟');
ok('cancel all alarms', m && m.kind === 'cancel' && m.target === 'alarms');
m = match('全部取消');
ok('cancel everything', m && m.kind === 'cancel' && m.target === 'all');
m = match('取消番茄钟');
ok('cancel pomodoro', m && m.kind === 'cancel' && m.target === 'timer' && m.name === '番茄钟');
m = match('看看我的闹钟');
ok('list alarms', m && m.kind === 'list' && m.target === 'alarm');
m = match('我有哪些计时器');
ok('list timers', m && m.kind === 'list' && m.target === 'timer');
m = match('找手机');
ok('find phone', m && m.kind === 'plugin' && m.action === 'ring' && m.plugin === intents.FIND_MY_PHONE);
m = match('找到了手机');
ok('stop phone', m && m.kind === 'plugin' && m.action === 'stop');
m = match('暂停音乐');
ok('music pause', m && m.kind === 'plugin' && m.action === 'set_playing' && m.args.on === false);
m = match('播放音乐');
ok('music play', m && m.kind === 'plugin' && m.action === 'set_playing' && m.args.on === true);
m = match('下一首');
ok('music next', m && m.kind === 'plugin' && m.action === 'next_track');
m = match('上一首');
ok('music prev', m && m.kind === 'plugin' && m.action === 'previous_track');
m = match('今天天气怎么样');
ok('weather today', m && m.kind === 'weather' && m.dayOffset === 0 && !m.rainOnly);
m = match('明天下雨吗');
ok('weather rain tomorrow', m && m.kind === 'weather' && m.dayOffset === 1 && m.rainOnly);
m = match('今日简报');
ok('coach brief', m && m.kind === 'coach' && m.brief === true);
m = match('我昨晚睡得怎么样');
ok('coach sleep question', m && m.kind === 'coach' && m.brief === false);
m = match('今天走了多少步');
ok('coach steps question', m && m.kind === 'coach' && m.brief === false);
m = match('我的健康数据怎么样');
ok('coach broad health data', m && m.kind === 'coach' && m.brief === false);
m = match('看看我的身体状况');
ok('coach broad body status', m && m.kind === 'coach' && m.brief === false);
m = match('查一下我的健康数据');
ok('coach broad lookup', m && m.kind === 'coach' && m.brief === false);
m = match('我身体最近怎么样');
ok('coach broad my body', m && m.kind === 'coach' && m.brief === false);
m = match('我的身体数据如何');
ok('coach broad body data', m && m.kind === 'coach' && m.brief === false);
m = match('看看我昨晚的睡眠');
ok('coach metric look', m && m.kind === 'coach' && m.brief === false);

// Misses: these must keep flowing to the AI.
['帮我写一首诗', '北京明天天气怎么样', '什么是番茄工作法', '8点开会', '定个闹钟',
 '提醒我明天去银行', '讲个笑话', '翻译一下这句话',
 '什么是心率变异性', '高血压怎么办', '心率多少正常', '如何保持健康',
 '健康饮食有什么好处'].forEach(function(text) {
    ok('miss: ' + text, match(text) === null);
});

// ------------------------------------------------------------- formatting
ok('humanDuration', localActions.humanDuration(1500) === '25 分钟' &&
    localActions.humanDuration(5400) === '1 小时 30 分钟' && localActions.humanDuration(45) === '45 秒');
ok('repeatText daily', localActions.repeatText(127) === '，每天重复');
ok('repeatText weekdays', localActions.repeatText(31) === '，工作日重复');
ok('repeatText mon', localActions.repeatText(1) === '，每周一重复');
ok('formatClock', localActions.formatClock(new Date(2026, 10, 28, 7, 5), new Date(NOW)) === '明天 07:05');

// --------------------------------------------------------------- weather
const weatherJson = JSON.parse(WEATHER_JSON);
ok('weather format today', localWeather.formatReport(weatherJson, 0, false).indexOf('今天晴') === 0);
ok('weather format rain', localWeather.formatReport(weatherJson, 2, true).indexOf('带伞') > 0);
ok('weather format no-rain', localWeather.formatReport(weatherJson, 0, true).indexOf('基本不会下雨') > 0);

// ----------------------------------------------------------------- coach
const HEALTH = {
    HEALTH_STEPS_TODAY: 8123, HEALTH_SLEEP_MINUTES: 383, HEALTH_SLEEP_RESTFUL_MINUTES: 130,
    HEALTH_RESTING_HR: 58, HEALTH_CURRENT_HR: 72, HEALTH_ACTIVE_MINUTES_TODAY: 95,
    HEALTH_DISTANCE_TODAY: 5200, HEALTH_WORKOUT_COUNT_7D: 3,
    HEALTH_LAST_WORKOUT_AGO_MINUTES: 1560, HEALTH_LAST_WORKOUT_DURATION_MINUTES: 35
};
const block = coach.formatDataBlock(HEALTH);
ok('coach block steps', block.indexOf('今日步数：8123') > 0);
ok('coach block sleep', block.indexOf('昨晚睡眠：6 小时 23 分钟') > 0);
ok('coach block honest hr label', block.indexOf('睡眠期最低心率：58 bpm') > 0);
ok('coach block workout', block.indexOf('近 7 天锻炼：3 次') > 0);
ok('coach block empty', coach.formatDataBlock({HEALTH_STEPS_TODAY: -1, HEALTH_SLEEP_MINUTES: -1,
    HEALTH_SLEEP_RESTFUL_MINUTES: -1, HEALTH_RESTING_HR: -1, HEALTH_CURRENT_HR: -1,
    HEALTH_ACTIVE_MINUTES_TODAY: -1, HEALTH_DISTANCE_TODAY: -1, HEALTH_WORKOUT_COUNT_7D: -1,
    HEALTH_LAST_WORKOUT_AGO_MINUTES: -1, HEALTH_LAST_WORKOUT_DURATION_MINUTES: -1}) === '');
ok('coach brief prompt has data + honesty', coach.buildBriefPrompt(HEALTH).indexOf('今日步数：8123') > 0 &&
    coach.buildBriefPrompt(HEALTH).indexOf('不要编造') > 0);

// -------------------------------------------------------------- executor
function chatText() {
    return sentMessages.filter(m2 => 'CHAT' in m2).map(m2 => m2.CHAT).join('');
}
function resetCapture() {
    sentMessages.length = 0;
    invokeCalls = [];
}
function tick(ms) {
    return new Promise(resolve => setTimeout(resolve, ms || 30));
}

(async function() {
    // Timer end-to-end: intent -> SET_ALARM_TIME on the wire -> reply.
    resetCapture();
    alarmListResponder = () => ({});
    assert.strictEqual(localActions.tryHandle({PROMPT: '定个10分钟的计时器'}), true);
    await tick(50);
    ok('exec timer sent', sentMessages.some(m2 => m2.SET_ALARM_TIME === 600 && m2.SET_ALARM_IS_TIMER === true));
    ok('exec timer reply', chatText().indexOf('计时器已开始：10 分钟') >= 0);

    // Recurring alarm carries SET_ALARM_REPEAT.
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '每天早上7点叫我'}), true);
    await tick(50);
    ok('exec daily repeat key', sentMessages.some(m2 => m2.SET_ALARM_REPEAT === 127));
    ok('exec daily reply', chatText().indexOf('每天重复') >= 0);

    // List alarms formats the watch's answer.
    resetCapture();
    const alarmUnix = Math.floor(new Date(2026, 10, 28, 7, 30).getTime() / 1000);
    alarmListResponder = (isTimer) => {
        const payload = {CURRENT_TIME: Math.floor(NOW / 1000)};
        payload[KEY_INDICES.GET_ALARM_RESULT] = 1;
        payload[KEY_INDICES.GET_ALARM_RESULT + 1] = alarmUnix;
        payload[KEY_INDICES.GET_ALARM_NAME + 1] = '吃药';
        return payload;
    };
    assert.strictEqual(localActions.tryHandle({PROMPT: '看看我的闹钟'}), true);
    await tick(50);
    ok('exec list reply', chatText().indexOf('闹钟 1 个') >= 0 && chatText().indexOf('吃药') >= 0);

    // Find my phone goes through invokeAction with the verified UUID.
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '找手机'}), true);
    await tick(50);
    ok('exec plugin call', invokeCalls.length === 1 && invokeCalls[0].plugin === intents.FIND_MY_PHONE &&
        invokeCalls[0].action === 'ring');
    ok('exec plugin reply', chatText().indexOf('正在让手机响铃') >= 0);

    // Weather: first ask fetches, second ask is served from the cache.
    location.update();
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '今天天气怎么样'}), true);
    await tick(50);
    ok('exec weather reply', chatText().indexOf('今天晴') >= 0);
    const afterFirst = xhrCount;
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '明天下雨吗'}), true);
    await tick(50);
    ok('exec weather cached', xhrCount === afterFirst && chatText().indexOf('降雨概率 30%') >= 0);

    // Coach: health snapshot is assembled into the AI session prompt.
    let capturedPrompt = null;
    const origRun = sessionModule.Session.prototype.run;
    sessionModule.Session.prototype.run = function() { capturedPrompt = this.prompt; };
    healthResponder = () => Object.assign({}, HEALTH);
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '今日简报'}), true);
    await tick(50);
    sessionModule.Session.prototype.run = origRun;
    ok('exec coach prompt', !!capturedPrompt && capturedPrompt.indexOf('今日步数：8123') > 0 &&
        capturedPrompt.indexOf('健康教练') >= 0);

    // Coach with no health answer at all replies locally (3 s timeout).
    healthResponder = null;
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '我昨晚睡得怎么样'}), true);
    await tick(3200);
    ok('exec coach no-data reply', chatText().indexOf('读不到健康数据') >= 0);

    // A plain question is NOT handled locally.
    resetCapture();
    assert.strictEqual(localActions.tryHandle({PROMPT: '帮我写一首诗'}), false);

    console.log('test_local_intents: all ' + passed + ' checks passed');
})().catch(function(e) {
    console.error('FAILED:', e && e.stack || e);
    process.exit(1);
});
