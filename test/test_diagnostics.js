// Node tests for the diagnostics log, the Chinese shouldExpose gate, and
// an end-to-end set_reminder tool call flowing through the real actions
// chain (with Pebble/XHR stubbed) to prove the diagnostics trace records
// every step. Run: node test/test_diagnostics.js
'use strict';

const assert = require('assert');
const path = require('path');
const Module = require('module');

// Stub modules that only exist inside the Pebble build.
const origLoad = Module._load;
Module._load = function(request, parent, isMain) {
    if (request === 'message_keys') {
        return new Proxy({}, {get: function() { return 0; }});
    }
    if (request === 'package.json') {
        return {version: '0.3'};
    }
    return origLoad.apply(this, arguments);
};

// In-memory localStorage.
const store = {};
global.localStorage = {
    getItem: k => (k in store ? store[k] : null),
    setItem: (k, v) => { store[k] = String(v); },
    removeItem: k => { delete store[k]; }
};
global.window = {};
global.Pebble = {
    addEventListener: function() {},
    removeEventListener: function() {},
    getTimelineToken: function(ok) { ok('test-token'); },
    sendAppMessage: function() {},
    platform: 'node'
};
// Fake XHR that immediately succeeds, so the timeline pin PUT "lands".
global.XMLHttpRequest = function() {};
global.XMLHttpRequest.prototype.open = function() {};
global.XMLHttpRequest.prototype.setRequestHeader = function() {};
global.XMLHttpRequest.prototype.send = function() {
    this.status = 200;
    this.responseText = '{}';
    if (this.onload) {
        this.onload();
    }
};

const root = path.resolve(__dirname, '../app/src/pkjs');
const diagnostics = require(path.join(root, 'agent/diagnostics'));
const watchTools = require(path.join(root, 'agent/watch_tools'));

// ---- shouldExpose: Chinese prompts expose the watch tools ----
assert.strictEqual(watchTools.shouldExpose('提醒我10点钟起床'), true);
assert.strictEqual(watchTools.shouldExpose('明天早上7点叫我起床'), true);
assert.strictEqual(watchTools.shouldExpose('定个20分钟的计时器'), true);
assert.strictEqual(watchTools.shouldExpose('取消闹钟'), true);
assert.strictEqual(watchTools.shouldExpose('20分钟后提醒我开会'), true);
assert.strictEqual(watchTools.shouldExpose('把语言改成英文'), true);
// Unrelated prompts must not expose watch tools. (Weather is served by the
// always-exposed UI tools, not by watch_tools.)
assert.strictEqual(watchTools.shouldExpose('给我讲个笑话'), false);
assert.strictEqual(watchTools.shouldExpose('今天天气怎么样'), false);
// English behaviour is unchanged.
assert.strictEqual(watchTools.shouldExpose('set an alarm for 7am'), true);
assert.strictEqual(watchTools.shouldExpose('remind me in 20 minutes'), true);
assert.strictEqual(watchTools.shouldExpose('cancel my timer'), true);
assert.strictEqual(watchTools.shouldExpose('hello there'), false);
console.log('SHOULDEXPOSE TESTS PASSED');

// ---- diagnostics opt-in gate: off means zero collection ----
function setDiagnosticsEnabled(on) {
    store['clay-settings'] = JSON.stringify({DIAGNOSTICS_ENABLED: on});
}
delete store['clay-settings'];
diagnostics.clear();
diagnostics.record('session_start', 'thread=test');
assert.strictEqual(diagnostics.getEvents().length, 0, 'unset toggle defaults to off: nothing recorded');
assert.ok(!('mica_diagnostics_events' in store), 'off: storage key never written');
setDiagnosticsEnabled(false);
diagnostics.record('prompt', '提醒我10点钟起床');
assert.strictEqual(diagnostics.getEvents().length, 0, 'explicitly off: nothing recorded');
assert.ok(!('mica_diagnostics_events' in store), 'explicitly off: storage key never written');
setDiagnosticsEnabled(true);
diagnostics.record('session_start', 'thread=test');
assert.strictEqual(diagnostics.getEvents().length, 1, 'on: events recorded again');
console.log('DIAGNOSTICS GATE TESTS PASSED');

// ---- diagnostics module basics ----
setDiagnosticsEnabled(true);
diagnostics.clear();
assert.strictEqual(diagnostics.getEvents().length, 0);
diagnostics.record('session_start', 'thread=test');
diagnostics.record('prompt', '提醒我10点钟起床');
assert.strictEqual(diagnostics.getEvents().length, 2);
for (let i = 0; i < 200; i++) {
    diagnostics.record('spam', 'event ' + i);
}
assert.strictEqual(diagnostics.getEvents().length, 120, 'log is capped at 120 events');
const report = diagnostics.formatReport();
assert.ok(report.indexOf('Mica diagnostics') === 0);
assert.ok(report.indexOf('App version: 0.3') !== -1);
assert.ok(report.indexOf('--- new session ---') === -1, 'oldest session marker rotated out by the cap');
diagnostics.clear();
assert.strictEqual(diagnostics.getEvents().length, 0);
assert.ok(diagnostics.formatReport().indexOf('No diagnostics events recorded.') !== -1);
console.log('DIAGNOSTICS MODULE TESTS PASSED');

// ---- end-to-end: a Chinese set_reminder tool call leaves a full trace ----
setDiagnosticsEnabled(true);
diagnostics.clear();
diagnostics.record('session_start', 'thread=test');
diagnostics.record('prompt', '提醒我10点钟起床');
const fakeSession = {
    prompt: '提醒我10点钟起床',
    handleMessage: function() {},
    enqueue: function() {}
};
let toolResult = null;
watchTools.execute(fakeSession, {
    name: 'set_reminder',
    arguments: {what: '起床', time: '2026-11-26T10:00:00+08:00'}
}, function(result) {
    toolResult = result;
});
assert.ok(toolResult, 'tool callback fired');
assert.ok(toolResult.status === 'ok' || toolResult.warning, 'reminder tool succeeded: ' + JSON.stringify(toolResult));
const types = diagnostics.getEvents().map(e => e.type);
assert.ok(types.indexOf('tool_call') !== -1, 'tool_call recorded');
assert.ok(types.indexOf('reminder') !== -1, 'reminder attempt recorded');
assert.ok(types.indexOf('timeline') !== -1, 'timeline pin result recorded');
assert.ok(types.indexOf('reminder_result') !== -1, 'reminder_result recorded');
assert.ok(types.indexOf('tool_result') !== -1, 'tool_result recorded');
assert.ok(types.indexOf('tool_call') < types.indexOf('tool_result'), 'tool_call precedes tool_result');
const timelineEvent = diagnostics.getEvents().filter(e => e.type === 'timeline')[0];
assert.ok(timelineEvent.detail.indexOf('HTTP 200') !== -1, 'pin PUT succeeded: ' + timelineEvent.detail);
console.log('END-TO-END TRACE TEST PASSED');
console.log('--- sample report ---');
console.log(diagnostics.formatReport());
console.log('ALL DIAGNOSTICS TESTS PASSED');
