/**
 * Copyright 2026 for Mica.
 *
 * AI coach: asks the watch for a snapshot of its LOCAL health data
 * (HealthService on the watch: today's steps, last night's sleep, heart
 * rate, activity, recent workouts), then hands the AI the user's question
 * plus that snapshot as context. The watch has no HRV or SpO2, and the
 * snapshot never pretends otherwise: missing metrics are omitted, and the
 * injected instructions tell the model to say so instead of inventing
 * numbers.
 */

'use strict';

var HEALTH_TIMEOUT_MS = 3000;

var KEYS = ['HEALTH_STEPS_TODAY', 'HEALTH_SLEEP_MINUTES', 'HEALTH_SLEEP_RESTFUL_MINUTES',
    'HEALTH_RESTING_HR', 'HEALTH_CURRENT_HR', 'HEALTH_ACTIVE_MINUTES_TODAY',
    'HEALTH_DISTANCE_TODAY', 'HEALTH_WORKOUT_COUNT_7D',
    'HEALTH_LAST_WORKOUT_AGO_MINUTES', 'HEALTH_LAST_WORKOUT_DURATION_MINUTES'];

// session.enqueue a HEALTH_REQUEST and wait for the watch's answer.
// callback(data|null): data maps the HEALTH_* keys to numbers, with -1 for
// "metric unavailable on the watch"; null when the watch never answered.
function requestHealth(session, callback) {
    var finished = false;
    var timer = setTimeout(function() {
        finish(null);
    }, HEALTH_TIMEOUT_MS);
    function finish(data) {
        if (finished) return;
        finished = true;
        clearTimeout(timer);
        Pebble.removeEventListener('appmessage', onMessage);
        callback(data);
    }
    function onMessage(event) {
        var payload = event && event.payload;
        if (!payload || !('HEALTH_STEPS_TODAY' in payload)) {
            return;
        }
        var data = {};
        KEYS.forEach(function(key) {
            var value = payload[key];
            data[key] = (typeof value === 'number') ? value : -1;
        });
        finish(data);
    }
    Pebble.addEventListener('appmessage', onMessage);
    session.enqueue({HEALTH_REQUEST: 1});
}

function formatDuration(minutes) {
    if (minutes >= 90) {
        var h = Math.floor(minutes / 60);
        var m = minutes % 60;
        return m > 0 ? h + ' 小时 ' + m + ' 分钟' : h + ' 小时';
    }
    return minutes + ' 分钟';
}

// The human-readable snapshot block, or '' when the watch reported
// nothing usable at all.
function formatDataBlock(data) {
    if (!data) return '';
    var lines = [];
    if (data.HEALTH_STEPS_TODAY >= 0) {
        lines.push('今日步数：' + data.HEALTH_STEPS_TODAY);
    }
    if (data.HEALTH_SLEEP_MINUTES >= 0) {
        var sleep = '昨晚睡眠：' + formatDuration(data.HEALTH_SLEEP_MINUTES);
        if (data.HEALTH_SLEEP_RESTFUL_MINUTES >= 0) {
            sleep += '（深睡 ' + formatDuration(data.HEALTH_SLEEP_RESTFUL_MINUTES) + '）';
        }
        lines.push(sleep);
    }
    if (data.HEALTH_RESTING_HR > 0) {
        lines.push('睡眠期最低心率：' + data.HEALTH_RESTING_HR + ' bpm（手表没有独立的静息心率指标，用睡眠期最低值近似）');
    }
    if (data.HEALTH_CURRENT_HR > 0) {
        lines.push('当前心率：' + data.HEALTH_CURRENT_HR + ' bpm');
    }
    var activity = [];
    if (data.HEALTH_ACTIVE_MINUTES_TODAY >= 0) {
        activity.push('活动 ' + data.HEALTH_ACTIVE_MINUTES_TODAY + ' 分钟');
    }
    if (data.HEALTH_DISTANCE_TODAY >= 0) {
        activity.push('步行 ' + (data.HEALTH_DISTANCE_TODAY / 1000).toFixed(1) + ' 公里');
    }
    if (activity.length) {
        lines.push('今日活动：' + activity.join('，'));
    }
    if (data.HEALTH_WORKOUT_COUNT_7D >= 0) {
        var workout = '近 7 天锻炼：' + data.HEALTH_WORKOUT_COUNT_7D + ' 次';
        if (data.HEALTH_LAST_WORKOUT_AGO_MINUTES >= 0 && data.HEALTH_LAST_WORKOUT_DURATION_MINUTES > 0) {
            var ago = data.HEALTH_LAST_WORKOUT_AGO_MINUTES;
            var agoText = ago >= 1440 ? Math.round(ago / 1440) + ' 天前' :
                (ago >= 60 ? Math.round(ago / 60) + ' 小时前' : ago + ' 分钟前');
            workout += '，最近一次在' + agoText + '，时长 ' + data.HEALTH_LAST_WORKOUT_DURATION_MINUTES + ' 分钟';
        }
        lines.push(workout);
    }
    if (!lines.length) {
        return '';
    }
    return '[手表本地健康数据]\n' + lines.join('\n');
}

var HONESTY = '只依据上面给出的数据回答；数据里没有的指标（例如 HRV、血氧）手表测不到，直接说明，不要编造数字。';

function buildBriefPrompt(data) {
    return '请以健康教练的口吻，用中文给我一份简短的今日健康简报：一句话总结状态，再给 2 到 3 条具体、可执行的建议。\n\n' +
        formatDataBlock(data) + '\n\n' + HONESTY;
}

function buildQuestionPrompt(question, data) {
    return question + '\n\n' + formatDataBlock(data) + '\n\n' + HONESTY;
}

exports.requestHealth = requestHealth;
exports.formatDataBlock = formatDataBlock;
exports.buildBriefPrompt = buildBriefPrompt;
exports.buildQuestionPrompt = buildQuestionPrompt;
