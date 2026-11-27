/**
 * Copyright 2026 for Mica.
 *
 * Executor for the local intent layer (see local_intents.js). tryHandle()
 * is called from index.js for every incoming PROMPT: when the intent
 * matcher hits, the request is executed locally and answered with a short
 * templated reply through the normal conversation frames, and tryHandle
 * returns true so the AI session is never started. On a miss it returns
 * false and nothing changes.
 */

'use strict';

var actions = require('../actions');
var Session = require('../session').Session;
var localHistory = require('./local_history');
var localIntents = require('./local_intents');
var localWeather = require('./local_weather');
var coach = require('./coach');
var diagnostics = require('./diagnostics');

var PLUGIN_TIMEOUT_MS = 6000;

// ---------------------------------------------------------------------------
// Reply plumbing: same frame protocol the AI path uses (companionless.js).
// ---------------------------------------------------------------------------

function reply(session, userText, text) {
    var threadId = localHistory.ensureThreadId(session);
    var chunk = '';
    for (var i = 0; i < text.length; i++) {
        var next = text[i];
        if (chunk.length > 0 && (chunk + next).length > 80) {
            session.handleMessage({data: 'c' + chunk});
            chunk = '';
        }
        chunk += next;
    }
    if (chunk.length > 0) {
        session.handleMessage({data: 'c' + chunk});
    }
    localHistory.recordTurn(threadId, userText, text);
    session.handleMessage({data: 'd'});
    session.handleClose({code: 1000, reason: '', wasClean: true});
}

// Runs one entry of the actions chain (alarms etc.) with a ws-shaped shim,
// exactly like watch_tools.callAction does for AI tool calls.
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

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

function pad2(n) {
    return (n < 10 ? '0' : '') + n;
}

function humanDuration(seconds) {
    if (seconds < 60) {
        return seconds + ' 秒';
    }
    var minutes = Math.round(seconds / 60);
    if (minutes < 60) {
        return minutes + ' 分钟';
    }
    var h = Math.floor(minutes / 60);
    var m = minutes % 60;
    return m > 0 ? h + ' 小时 ' + m + ' 分钟' : h + ' 小时';
}

function formatClock(date, now) {
    var dayMs = 24 * 60 * 60 * 1000;
    var startOf = function(d) { return new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime(); };
    var days = Math.round((startOf(date) - startOf(now)) / dayMs);
    var label;
    if (days === 0) label = '今天';
    else if (days === 1) label = '明天';
    else if (days === 2) label = '后天';
    else label = (date.getMonth() + 1) + '月' + date.getDate() + '日';
    return label + ' ' + pad2(date.getHours()) + ':' + pad2(date.getMinutes());
}

var WEEKDAY_NAMES = ['一', '二', '三', '四', '五', '六', '日'];

function repeatText(repeat) {
    if (!repeat) return '';
    if (repeat === localIntents.REPEAT_DAILY) return '，每天重复';
    if (repeat === localIntents.REPEAT_WEEKDAYS) return '，工作日重复';
    var days = [];
    for (var i = 0; i < 7; i++) {
        if (repeat & (1 << i)) days.push('周' + WEEKDAY_NAMES[i]);
    }
    return days.length ? '，每' + days.join('、') + '重复' : '';
}

function alarmErrorText(result) {
    var message = (result && result.error) || '';
    if (/past/.test(message)) return '这个时间已经过了，换一个未来的时间吧。';
    if (/limit of eight|already reached/.test(message)) return '闹钟和计时器加起来最多 8 个，已经满了，先取消几个吧。';
    if (/already scheduled/.test(message)) return '那个时间已经有一个安排了。';
    if (/Timed out/.test(message)) return '手表没有回应，请再试一次。';
    if (/no alarm set|No .* were set/.test(message)) return '没有找到要取消的安排。';
    return '没成功：' + (message || '未知错误');
}

// ---------------------------------------------------------------------------
// Timers and alarms (watch wakeups via the existing actions chain)
// ---------------------------------------------------------------------------

function doTimer(session, userText, intent) {
    callAction(session, {
        action: 'set_alarm', isTimer: true, duration: intent.seconds,
        name: intent.name || null, cancel: false
    }, function(result) {
        if (result && result.status === 'ok') {
            if (intent.name === '番茄钟') {
                reply(session, userText, '番茄钟已开始：专注 ' + humanDuration(intent.seconds) + '，到点手表会震动。');
            } else {
                reply(session, userText, '计时器已开始：' + humanDuration(intent.seconds) + '，到点手表会震动提醒。');
            }
        } else {
            reply(session, userText, alarmErrorText(result));
        }
    });
}

function doAlarm(session, userText, intent) {
    var action = {
        action: 'set_alarm', isTimer: false, time: intent.at.toISOString(),
        name: intent.name || null, cancel: false
    };
    if (intent.repeat) {
        action.repeat = intent.repeat;
    }
    callAction(session, action, function(result) {
        if (result && result.status === 'ok') {
            var when = formatClock(intent.at, new Date());
            if (intent.delayedSeconds) {
                reply(session, userText, '好，' + humanDuration(intent.delayedSeconds) + '后提醒你' +
                    (intent.name ? '：' + intent.name : '') + '（' + when + '）。');
            } else {
                reply(session, userText, '闹钟已定：' + when + repeatText(intent.repeat) +
                    (intent.name ? '，' + intent.name : '') + '。');
            }
        } else {
            reply(session, userText, alarmErrorText(result));
        }
    });
}

function getList(session, isTimer, callback) {
    callAction(session, {action: 'get_alarm', isTimer: isTimer}, function(result) {
        if (result && result.status === 'ok') {
            callback(null, (isTimer ? result.timers : result.alarms) || []);
        } else {
            callback(result);
        }
    });
}

function cancelOne(session, isTimer, timeIso, callback) {
    var action = {action: 'set_alarm', isTimer: isTimer, cancel: true};
    if (timeIso) {
        action.time = timeIso;
    }
    callAction(session, action, callback);
}

function cancelKind(session, userText, isTimer) {
    getList(session, isTimer, function(err, list) {
        if (err) {
            reply(session, userText, alarmErrorText(err));
            return;
        }
        if (!list.length) {
            reply(session, userText, isTimer ? '现在没有计时器。' : '现在没有闹钟。');
            return;
        }
        var done = 0;
        var ok = 0;
        list.forEach(function(entry) {
            cancelOne(session, isTimer, entry.expirationTimeForDeletingAndWidgets || entry.time, function(result) {
                if (result && result.status === 'ok') ok++;
                if (++done === list.length) {
                    reply(session, userText, '已取消全部 ' + ok + ' 个' + (isTimer ? '计时器' : '闹钟') + '。');
                }
            });
        });
    });
}

function doCancel(session, userText, intent) {
    if (intent.target === 'all') {
        cancelAll(session, userText);
        return;
    }
    if (intent.target === 'timers' || intent.target === 'alarms') {
        cancelKind(session, userText, intent.target === 'timers');
        return;
    }
    var isTimer = intent.target === 'timer';
    if (intent.name) {
        // Named cancel (e.g. 番茄钟): find it in the list, cancel by time.
        getList(session, isTimer, function(err, list) {
            if (err) {
                reply(session, userText, alarmErrorText(err));
                return;
            }
            var found = null;
            list.forEach(function(entry) {
                if (entry.name === intent.name) found = entry;
            });
            if (!found) {
                reply(session, userText, '现在没有' + intent.name + '在跑。');
                return;
            }
            cancelOne(session, isTimer, found.expirationTimeForDeletingAndWidgets || found.time, function(result) {
                if (result && result.status === 'ok') {
                    reply(session, userText, '已取消' + intent.name + '。');
                } else {
                    reply(session, userText, alarmErrorText(result));
                }
            });
        });
        return;
    }
    if (intent.at) {
        cancelOne(session, isTimer, intent.at.toISOString(), function(result) {
            if (result && result.status === 'ok') {
                reply(session, userText, '已取消' + formatClock(intent.at, new Date()) + '的闹钟。');
            } else {
                reply(session, userText, alarmErrorText(result));
            }
        });
        return;
    }
    // No qualifier: the watch cancels the earliest one of this kind.
    cancelOne(session, isTimer, null, function(result) {
        if (result && result.status === 'ok') {
            reply(session, userText, isTimer ? '已取消最近的一个计时器。' : '已取消最近的一个闹钟。');
        } else {
            reply(session, userText, alarmErrorText(result));
        }
    });
}

function cancelAll(session, userText) {
    getList(session, false, function(errA, alarms) {
        getList(session, true, function(errT, timers) {
            if (errA || errT) {
                reply(session, userText, '读取列表失败，请再试一次。');
                return;
            }
            var jobs = [];
            (alarms || []).forEach(function(a) { jobs.push({isTimer: false, time: a.time}); });
            (timers || []).forEach(function(t) { jobs.push({isTimer: true, time: t.expirationTimeForDeletingAndWidgets}); });
            if (!jobs.length) {
                reply(session, userText, '现在没有闹钟或计时器。');
                return;
            }
            var done = 0;
            var ok = 0;
            jobs.forEach(function(job) {
                cancelOne(session, job.isTimer, job.time, function(result) {
                    if (result && result.status === 'ok') ok++;
                    if (++done === jobs.length) {
                        reply(session, userText, '已取消全部 ' + ok + ' 个闹钟和计时器。');
                    }
                });
            });
        });
    });
}

function doList(session, userText, intent) {
    var wantAlarms = intent.target === 'alarm' || intent.target === 'both';
    var wantTimers = intent.target === 'timer' || intent.target === 'both';
    var pending = (wantAlarms ? 1 : 0) + (wantTimers ? 1 : 0);
    var parts = [];
    function step() {
        if (--pending === 0) {
            reply(session, userText, parts.length ? parts.join('\n') : '现在什么都没有。');
        }
    }
    if (wantAlarms) {
        getList(session, false, function(err, alarms) {
            if (err) {
                parts.push('闹钟列表读取失败。');
            } else if (!alarms.length) {
                parts.push('没有闹钟。');
            } else {
                var lines = alarms.map(function(a) {
                    var d = new Date(a.time);
                    return '· ' + formatClock(d, new Date()) + (a.name ? ' ' + a.name : '');
                });
                parts.push('闹钟 ' + alarms.length + ' 个：\n' + lines.join('\n'));
            }
            step();
        });
    }
    if (wantTimers) {
        getList(session, true, function(err, timers) {
            if (err) {
                parts.push('计时器列表读取失败。');
            } else if (!timers.length) {
                parts.push('没有计时器。');
            } else {
                var lines = timers.map(function(t) {
                    return '· 还剩 ' + t.formattedTimeLeft + (t.name ? ' ' + t.name : '');
                });
                parts.push('计时器 ' + timers.length + ' 个：\n' + lines.join('\n'));
            }
            step();
        });
    }
}

// ---------------------------------------------------------------------------
// Phone-app plugins (find my phone, music) via Pebble.invokeAction
// ---------------------------------------------------------------------------

function doPlugin(session, userText, intent) {
    if (typeof Pebble === 'undefined' || !Pebble.invokeAction) {
        reply(session, userText, intent.failText + '（手机应用不支持插件调用）');
        return;
    }
    var finished = false;
    var timer = setTimeout(function() {
        if (!finished) {
            finished = true;
            reply(session, userText, intent.failText + '（超时）');
        }
    }, PLUGIN_TIMEOUT_MS);
    var call = {plugin: intent.plugin, action: intent.action};
    if (intent.args) {
        call.args = intent.args;
    }
    var promise;
    try {
        promise = Pebble.invokeAction(call);
    } catch (e) {
        clearTimeout(timer);
        reply(session, userText, intent.failText);
        return;
    }
    Promise.resolve(promise).then(function(res) {
        if (finished) return;
        finished = true;
        clearTimeout(timer);
        if (res && res.ok) {
            reply(session, userText, intent.doneText + '。');
        } else {
            reply(session, userText, intent.failText + '。');
        }
    }, function() {
        if (finished) return;
        finished = true;
        clearTimeout(timer);
        reply(session, userText, intent.failText + '。');
    });
}

// ---------------------------------------------------------------------------
// Coach: health snapshot from the watch, then a normal AI session whose
// prompt carries the data as context.
// ---------------------------------------------------------------------------

function doCoach(session, userText, intent) {
    coach.requestHealth(session, function(data) {
        var block = coach.formatDataBlock(data);
        if (!block) {
            reply(session, userText, '手表暂时读不到健康数据（可能还没同步或没有可穿戴数据），稍后再试一次。');
            return;
        }
        session.prompt = intent.brief ?
            coach.buildBriefPrompt(data) :
            coach.buildQuestionPrompt(userText, data);
        session.run();
    });
}

// ---------------------------------------------------------------------------

function execute(session, userText, intent) {
    switch (intent.kind) {
    case 'timer':
        doTimer(session, userText, intent);
        return;
    case 'alarm':
        doAlarm(session, userText, intent);
        return;
    case 'cancel':
        doCancel(session, userText, intent);
        return;
    case 'list':
        doList(session, userText, intent);
        return;
    case 'plugin':
        doPlugin(session, userText, intent);
        return;
    case 'weather':
        localWeather.getReport(intent.dayOffset, intent.rainOnly, function(text) {
            reply(session, userText, text);
        });
        return;
    case 'coach':
        doCoach(session, userText, intent);
        return;
    }
}

// data is the raw AppMessage payload carrying PROMPT. Returns true when
// the prompt was handled locally.
exports.tryHandle = function(data) {
    var intent = localIntents.match(data.PROMPT, Date.now());
    if (!intent) {
        return false;
    }
    diagnostics.record('local_intent', intent.kind + ' <- ' + String(data.PROMPT).substring(0, 60));
    var session = new Session(data.PROMPT, data.THREAD_ID, data.ANDROID_REQUEST_ID);
    try {
        execute(session, data.PROMPT, intent);
    } catch (e) {
        diagnostics.record('exception', 'local_actions: ' + (e && e.message ? e.message : e));
        reply(session, data.PROMPT, '本地处理出了点问题，请再说一次。');
    }
    return true;
};

exports.formatClock = formatClock;
exports.humanDuration = humanDuration;
exports.repeatText = repeatText;
