/**
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 *
 * Diagnostics: a small localStorage-backed event log that records what the
 * assistant pipeline actually did (prompts, tool exposure decisions, tool
 * calls and results, watch replies, provider errors, exceptions) so a
 * failure can be diagnosed after the fact from the settings page.
 *
 * Everything here is best-effort: every storage access is guarded and this
 * module must never throw into app code. No API keys or credentials are
 * ever recorded.
 */

var config = require('../config');

var STORAGE_KEY = 'mica_diagnostics_events';
var MAX_EVENTS = 120;
var MAX_DETAIL_LENGTH = 300;

// Collection is opt-in: events are recorded only while the user has the
// Diagnostics log toggle (DIAGNOSTICS_ENABLED) on in settings. Default off.
function isEnabled() {
    try {
        return !!config.getSetting('DIAGNOSTICS_ENABLED', false);
    } catch (e) {
        return false;
    }
}

exports.isEnabled = isEnabled;

function loadEvents() {
    try {
        var raw = localStorage.getItem(STORAGE_KEY);
        if (!raw) {
            return [];
        }
        var parsed = JSON.parse(raw);
        return Array.isArray(parsed) ? parsed : [];
    } catch (e) {
        return [];
    }
}

function saveEvents(events) {
    try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(events));
    } catch (e) {
        // Diagnostics must never break the app.
    }
}

function truncate(text, maxLength) {
    if (text.length <= maxLength) {
        return text;
    }
    return text.substring(0, maxLength - 3) + '...';
}

function pad2(number) {
    return (number < 10 ? '0' : '') + number;
}

function formatClock(timestamp) {
    var date = new Date(timestamp);
    return pad2(date.getHours()) + ':' + pad2(date.getMinutes()) + ':' + pad2(date.getSeconds());
}

// Record one event. type is a short tag ('prompt', 'tools', 'tool_call',
// 'tool_result', 'alarm_result', 'reminder_result', 'timeline',
// 'provider_error', 'exception', ...); detail is a short human-readable
// string. Never pass API keys or credentials.
exports.record = function(type, detail) {
    if (!isEnabled()) {
        return;
    }
    try {
        var events = loadEvents();
        events.push({
            ts: Date.now(),
            type: String(type || 'event'),
            detail: truncate(detail === undefined || detail === null ? '' : String(detail), MAX_DETAIL_LENGTH)
        });
        while (events.length > MAX_EVENTS) {
            events.shift();
        }
        saveEvents(events);
    } catch (e) {
        // Diagnostics must never break the app.
    }
};

exports.getEvents = function() {
    return loadEvents();
};

exports.clear = function() {
    try {
        localStorage.removeItem(STORAGE_KEY);
    } catch (e) {
        // Diagnostics must never break the app.
    }
};

// Plain-text report for the settings page. Events are oldest first; each
// dictation session is preceded by a separator so the most recent session
// (at the end) is easy to find and read.
exports.formatReport = function() {
    var lines = [];
    lines.push('Mica diagnostics 诊断日志');
    lines.push('生成时间 Generated: ' + new Date().toString());
    try {
        var packageJson = require('package.json');
        if (packageJson && packageJson.version) {
            lines.push('应用版本 App version: ' + packageJson.version);
        }
    } catch (e) {
        // Version is nice to have, not essential.
    }
    var events = loadEvents();
    if (events.length === 0) {
        lines.push('');
        lines.push('暂无诊断事件记录。No diagnostics events recorded.');
        return lines.join('\n');
    }
    lines.push('事件 Events: ' + events.length + '（最早在前，最新会话在最后；oldest first, most recent session last）');
    events.forEach(function(event) {
        if (event.type === 'session_start') {
            lines.push('--- 新会话 new session ---');
        }
        lines.push('[' + formatClock(event.ts) + '] ' + event.type + ': ' + event.detail);
    });
    return lines.join('\n');
};
