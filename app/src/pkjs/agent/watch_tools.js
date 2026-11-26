/**
 * Copyright 2025 Google LLC
 * Modified in 2026 for Mica.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var actions = require('../actions');
var diagnostics = require('./diagnostics');

function schema(properties, required) {
    var result = {
        type: 'object',
        properties: properties
    };
    if (required && required.length > 0) {
        result.required = required;
    }
    return result;
}

function nullableString(description) {
    return {
        type: 'string',
        description: description
    };
}

function nullableInteger(description) {
    return {
        type: 'integer',
        description: description,
        format: 'int32'
    };
}

function nullableBoolean(description) {
    return {
        type: 'boolean',
        description: description
    };
}

function callAction(session, action, callback) {
    var ws = {
        send: function(resultString) {
            try {
                callback(JSON.parse(resultString));
            } catch (e) {
                callback({error: e.message});
            }
        }
    };
    actions.handleAction(session, ws, JSON.stringify(action));
}

exports.getDeclarations = function() {
    return [
        {
            type: 'function',
            name: 'set_alarm',
            description: 'Set an alarm for a given time.',
            parameters: schema({
                time: nullableString("The time to schedule the alarm for in ISO 8601 format with the user's current local UTC offset, e.g. '2023-07-12T00:00:00+02:00'. Must always be in the future. Use the phone/watch timezone from the system instruction unless the user explicitly names another timezone."),
                name: nullableString("Only if explicitly specified by the user, the name of the alarm. Use title case. If the user didn't ask to name the alarm, leave it empty.")
            }, ['time'])
        },
        {
            type: 'function',
            name: 'get_alarms',
            description: 'Get any existing alarms.',
            parameters: schema({}, [])
        },
        {
            type: 'function',
            name: 'delete_alarm',
            description: 'Delete a specific alarm by its expiration time.',
            parameters: schema({
                time: nullableString("The time of the alarm to delete in ISO 8601 format with the user's current local UTC offset, e.g. '2023-07-12T00:00:00+02:00'.")
            }, ['time'])
        },
        {
            type: 'function',
            name: 'set_timer',
            description: 'Set a timer for a given duration.',
            parameters: schema({
                duration_seconds: nullableInteger('The number of seconds to set the timer for.'),
                name: nullableString("Only if explicitly specified by the user, the name of the timer. Use title case. If the user didn't ask to name the timer, leave it empty.")
            }, ['duration_seconds'])
        },
        {
            type: 'function',
            name: 'get_timers',
            description: 'Get any existing timers.',
            parameters: schema({}, [])
        },
        {
            type: 'function',
            name: 'delete_timer',
            description: 'Delete a specific timer by its expiration time.',
            parameters: schema({
                time: nullableString("The expiration time of the timer to delete in ISO 8601 format, e.g. '2023-07-12T00:00:00-07:00'.")
            }, ['time'])
        },
        {
            type: 'function',
            name: 'set_reminder',
            description: 'Set a reminder for the user to perform a task at a time. Either time or delay must be provided, but not both. If the user specifies a time but not a day, assume they meant the next time that time will happen.',
            parameters: schema({
                time: nullableString("The time to schedule the reminder for in ISO 8601 format with the user's current local UTC offset, e.g. '2023-07-12T00:00:00+02:00'. Always assume the phone/watch timezone unless otherwise specified."),
                delay_mins: nullableInteger('The delay from now to when the reminder should be scheduled, in minutes.'),
                what: nullableString('What to remind the user to do.')
            }, ['what'])
        },
        {
            type: 'function',
            name: 'get_reminders',
            description: 'Get a list of all active reminders.',
            parameters: schema({}, [])
        },
        {
            type: 'function',
            name: 'delete_reminder',
            description: 'Delete a specific reminder by its ID.',
            parameters: schema({
                id: nullableString('The ID of the reminder to delete. You must call get_reminders first to discover the ID of the correct reminder.')
            }, ['id'])
        },
        {
            type: 'function',
            name: 'update_settings',
            description: 'Change Mica watch settings such as units, response language, vibration pattern, quick launch behavior, or prompt confirmation.',
            parameters: schema({
                unitSystem: nullableString("Optional. One of: 'imperial', 'metric', 'uk hybrid', 'both', or 'auto'."),
                responseLanguage: nullableString("Optional. Use a supported language code such as 'en_US', 'de_DE', 'fr_FR', or 'auto'."),
                alarmVibrationPattern: nullableString("Optional. One of: 'Reveille', 'Mario', 'Nudge Nudge', 'Jackhammer', or 'Standard'."),
                timerVibrationPattern: nullableString("Optional. One of: 'Reveille', 'Mario', 'Nudge Nudge', 'Jackhammer', or 'Standard'."),
                quickLaunchBehaviour: nullableString("Optional. One of: 'start conversation and time out', 'start conversation and stay open', or 'open home screen'."),
                confirmPrompts: nullableBoolean('Optional. True to confirm dictated prompts before responding, false to respond immediately.')
            }, [])
        }
    ];
}

exports.shouldExpose = function(prompt) {
    return /\b(timer|timers|alarm|alarms|remind|reminder|reminders|wake\s+me)\b/i.test(prompt) ||
        /\b(cancel|delete|remove|list|show|check|get|create|add|make|schedule|start)\b.*\b(timer|timers|alarm|alarms|reminder|reminders)\b/i.test(prompt) ||
        /\b(set|use|change|switch|enable|disable|turn)\b.*\b(unit|units|metric|imperial|language|vibration|vibrate|quick launch|confirm|transcript|transcripts)\b/i.test(prompt) ||
        // Chinese equivalents of the three patterns above. CJK text has no
        // word boundaries, so these match substrings instead of \b terms.
        /(提醒|闹钟|起床|叫醒|叫我|计时器|倒计时|定时|分钟后|小时后)/.test(prompt) ||
        /(取消|删除|删掉|查看|列出|关掉).*(提醒|闹钟|计时器|倒计时|定时)/.test(prompt) ||
        /(设置|改成|换成|切换|打开|关闭|开启|启用|禁用).*(语言|振动|单位|公制|英制|快捷启动|确认)/.test(prompt) ||
        /(语言|振动|单位|公制|英制|快捷启动|确认).*(改成|换成|切换|设置)/.test(prompt);
}

function normalizeArguments(call) {
    var args = call.arguments || {};
    if (typeof args === 'string') {
        try {
            return JSON.parse(args);
        } catch (e) {
            return {};
        }
    }
    return args;
}

function getTimerDuration(args) {
    var duration = parseInt(args.duration_seconds, 10) || 0;
    duration += (parseInt(args.duration_minutes, 10) || 0) * 60;
    duration += (parseInt(args.duration_hours, 10) || 0) * 3600;
    return duration;
}

function executeSetAlarm(session, args, callback) {
    if (!args.time) {
        callback({error: 'Alarm time is required.'});
        return;
    }
    callAction(session, {
        action: 'set_alarm',
        isTimer: false,
        time: args.time,
        name: args.name || null,
        cancel: false
    }, callback);
}

function executeSetTimer(session, args, callback) {
    var seconds = getTimerDuration(args);
    if (seconds < 1) {
        callback({error: 'Timer duration must be a positive number of seconds.'});
        return;
    }
    callAction(session, {
        action: 'set_alarm',
        isTimer: true,
        duration: seconds,
        name: args.name || null,
        cancel: false
    }, callback);
}

function executeSetReminder(session, args, callback) {
    if (!args.what) {
        callback({error: 'Reminder text is required.'});
        return;
    }
    if (!args.time && !args.delay_mins) {
        callback({error: 'Either reminder time or delay_mins is required.'});
        return;
    }
    if (args.time && args.delay_mins) {
        callback({error: 'Only one of reminder time or delay_mins may be provided.'});
        return;
    }
    var time = args.time;
    if (args.delay_mins) {
        var delay = parseInt(args.delay_mins, 10);
        if (isNaN(delay) || delay < 1) {
            callback({error: 'Reminder delay_mins must be a positive number.'});
            return;
        }
        time = new Date(Date.now() + delay * 60000).toISOString();
    }
    callAction(session, {
        action: 'set_reminder',
        what: args.what,
        time: time
    }, callback);
}

function summarizeToolArgs(args) {
    var parts = [];
    if (args.time) {
        parts.push('time=' + args.time);
    }
    if (args.delay_mins !== undefined && args.delay_mins !== null) {
        parts.push('delay_mins=' + args.delay_mins);
    }
    if (args.duration_seconds !== undefined && args.duration_seconds !== null) {
        parts.push('duration_seconds=' + args.duration_seconds);
    }
    if (args.what) {
        parts.push('what=' + String(args.what).substring(0, 60));
    }
    if (args.name) {
        parts.push('name=' + args.name);
    }
    if (args.id) {
        parts.push('id=' + args.id);
    }
    return parts.join(' ');
}

exports.execute = function(session, call, callback) {
    var args = normalizeArguments(call);
    var summary = summarizeToolArgs(args);
    diagnostics.record('tool_call', call.name + (summary ? ' ' + summary : ''));
    var outerCallback = callback;
    callback = function(result) {
        if (result && result.error) {
            diagnostics.record('tool_result', call.name + ' error: ' + result.error);
        } else {
            diagnostics.record('tool_result', call.name + ' ok' + (result && result.warning ? ' (with warning)' : ''));
        }
        outerCallback(result);
    };
    try {
        executeCall(session, call, args, callback);
    } catch (e) {
        diagnostics.record('exception', 'watch_tools.execute ' + call.name + ': ' + (e && e.message ? e.message : e));
        throw e;
    }
}

function executeCall(session, call, args, callback) {
    switch (call.name) {
    case 'set_alarm':
        executeSetAlarm(session, args, callback);
        return;
    case 'get_alarms':
    case 'get_alarm':
        callAction(session, {action: 'get_alarm', isTimer: false}, callback);
        return;
    case 'delete_alarm':
        if (!args.time) {
            callback({error: 'Alarm time is required.'});
            return;
        }
        callAction(session, {action: 'set_alarm', isTimer: false, time: args.time, cancel: true}, callback);
        return;
    case 'set_timer':
        executeSetTimer(session, args, callback);
        return;
    case 'get_timers':
    case 'get_timer':
        callAction(session, {action: 'get_alarm', isTimer: true}, callback);
        return;
    case 'delete_timer':
        if (!args.time) {
            callback({error: 'Timer expiration time is required.'});
            return;
        }
        callAction(session, {action: 'set_alarm', isTimer: true, time: args.time, cancel: true}, callback);
        return;
    case 'set_reminder':
        executeSetReminder(session, args, callback);
        return;
    case 'get_reminders':
        callAction(session, {action: 'get_reminders'}, callback);
        return;
    case 'delete_reminder':
        if (!args.id) {
            callback({error: 'Reminder ID is required. Call get_reminders first to find it.'});
            return;
        }
        callAction(session, {action: 'delete_reminder', id: args.id}, callback);
        return;
    case 'update_settings':
        args.action = 'update_settings';
        callAction(session, args, callback);
        return;
    default:
        callback({error: 'Unknown local tool: ' + call.name});
        return;
    }
}
