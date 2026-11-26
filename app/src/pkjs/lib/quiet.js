/**
 * Copyright 2026 for Mica.
 *
 * Quiet Time while dictating. A phone notification that pops up mid-sentence
 * aborts the watch's dictation, so while the user dictates, this module turns
 * the watch's manual Quiet Time on AND hides its notifications, then puts
 * both settings back exactly as they were. Both changes go through the Pebble
 * app's built-in Watch Settings plugin (Pebble.invokeAction /
 * Pebble.subscribeToSource); plain Quiet Time alone is not enough, because
 * its notifications still pop up by default.
 *
 * The restore record lives in localStorage until the watch confirms the
 * put-back, so a crash or a lost reply still gets undone: on the next pkjs
 * start (stop() from index.js), by the safety timer, or by retry.
 *
 * Ported from Bucky (github.com/blacklava2002/Bucky, MIT) src/pkjs/quiet.js
 * and src/pkjs/sources.js, adapted to Mica's storage and logging.
 */

var WATCH_SETTINGS = '00000000-0000-0000-0001-000000000006';
var MANUAL = 'dndManuallyEnabled';     // Quiet Time - Manual (a switch)
var SHOW = 'dndShowNotifications';     // Quiet Time - Show Notifications: Show or Hide
var HIDE = 'Hide';
var STORE_KEY = 'mica_quiet_restore';
var READ_MS = 1500;
var SET_MS = 2500;
var SAFETY_MS = 90 * 1000;
// A put-back the watch didn't confirm is tried again, this often and this many times.
var RETRY_MS = 30 * 1000;
var RETRIES = 5;

function log(msg) {
    console.log('quiet: ' + msg);
}

function loadRecord() {
    try {
        var raw = localStorage.getItem(STORE_KEY);
        if (!raw) {
            return null;
        }
        var r = JSON.parse(raw);
        return r && typeof r === 'object' && (r.manual !== undefined || r.show !== undefined) ? r : null;
    } catch (e) {
        return null;
    }
}

function saveRecord(value) {
    try {
        if (value === null || value === undefined) {
            localStorage.removeItem(STORE_KEY);
        } else {
            localStorage.setItem(STORE_KEY, JSON.stringify(value));
        }
    } catch (e) {
        log('could not persist the restore record: ' + e);
    }
}

function invoke(args) {
    if (typeof Pebble === 'undefined' || !Pebble.invokeAction) {
        return Promise.reject(new Error('no plugin API'));
    }
    return Pebble.invokeAction(args);
}

// One reading of a plugin source (Bucky's sources.js): subscribes, takes the
// first answer, unsubscribes. done(err, instances) gets every instance when
// request.all is set. Without the plugin API it fails fast with UNSUPPORTED,
// which start() treats as "nothing to change" and releases immediately.
function readSource(request, done, timeoutMs) {
    var finished = false;
    var subscription = null;
    var timer = null;
    function finish(err, instances) {
        if (finished) {
            return;
        }
        finished = true;
        if (timer) {
            clearTimeout(timer);
        }
        setTimeout(function () {
            if (subscription) {
                subscription.unsubscribe();
            }
        }, 0);
        done(err, instances);
    }
    if (typeof Pebble === 'undefined' || !Pebble.subscribeToSource) {
        finish({code: 'UNSUPPORTED'});
        return;
    }
    timer = setTimeout(function () {
        finish({code: 'TIMEOUT'});
    }, timeoutMs || 20000);
    var config = {
        category: request.category,
        item: request.item,
        properties: request.properties,
        onData: function (envelope) {
            var instances = (envelope && envelope.instances) || [];
            if (request.all) {
                finish(null, instances);
            } else if (instances[0]) {
                finish(null, [instances[0]]);
            } else {
                finish({code: 'NO_DATA'});
            }
        },
        onError: function (err) {
            finish(err || {code: 'ERROR'});
        }
    };
    if (request.plugin) {
        config.plugin = request.plugin;
    }
    try {
        subscription = Pebble.subscribeToSource(config);
    } catch (e) {
        finish({code: 'ERROR', message: String((e && e.message) || e)});
    }
}

var starting = false;
var restoring = null;   // callbacks waiting for a restore to finish
var waiters = [];
var stopAfterStart = false;
var stopWaiters = [];
var safety = null;

// Runs each change, then done(allOk) once all have answered or SET_MS has passed.
function apply(changes, done) {
    var left = changes.length;
    var finished = false;
    var timer = null;
    var allOk = true;
    function one(ok) {
        if (!ok) {
            allOk = false;
        }
        if (finished || --left > 0) {
            return;
        }
        finished = true;
        if (timer) {
            clearTimeout(timer);
        }
        done(allOk);
    }
    if (!left) {
        done(true);
        return;
    }
    timer = setTimeout(function () {
        left = 1;
        one(false);
    }, SET_MS);
    changes.forEach(function (change) {
        var call;
        try {
            call = invoke({plugin: WATCH_SETTINGS, action: change.action, args: change.args});
        } catch (e) {
            log(change.args.instanceId + ' failed: ' + e);
            one(false);
            return;
        }
        Promise.resolve(call).then(function (res) {
            var ok = !!(res && res.ok);
            if (!ok) {
                log(change.args.instanceId + ' refused: ' + JSON.stringify(res));
            }
            one(ok);
        }, function (err) {
            log(change.args.instanceId + ' failed: ' + ((err && err.message) || err));
            one(false);
        });
    });
}

function findInstance(instances, id) {
    for (var i = 0; i < (instances || []).length; i++) {
        if (instances[i] && instances[i].instanceId === id) {
            return instances[i];
        }
    }
    return null;
}

function instanceValue(instance) {
    return (instance && instance.properties && instance.properties.value) || {};
}

function armSafety() {
    if (safety) {
        clearTimeout(safety);
    }
    safety = setTimeout(function () {
        safety = null;
        stop();
    }, SAFETY_MS);
}

function finishStart() {
    starting = false;
    var list = waiters;
    waiters = [];
    list.forEach(function (fn) {
        fn();
    });
    if (stopAfterStart) {
        stopAfterStart = false;
        var after = stopWaiters;
        stopWaiters = [];
        stop(function () {
            after.forEach(function (fn) {
                fn();
            });
        });
    }
}

// Turns Quiet Time on and its notifications off; done() when the watch
// should start listening. With no plugin API (e.g. the emulator) or nothing
// to change, done() runs immediately so dictation is never blocked.
function start(done) {
    stopAfterStart = false;
    if (restoring) {
        restoring.push(function () {
            start(done);
        });
        return;
    }
    waiters.push(done);
    if (starting) {
        return;
    }
    if (loadRecord()) {
        armSafety();
        finishStart();
        return;
    }
    starting = true;
    var switches = null;
    var texts = null;
    function readBoth() {
        if (switches === null || texts === null) {
            return;
        }
        var manual = findInstance(switches, MANUAL);
        var show = findInstance(texts, SHOW);
        var restore = {};
        var changes = [];
        var on = instanceValue(manual).boolean;
        if (manual && on && on.value === false) {
            restore.manual = false;
            changes.push({action: 'set_on', args: {instanceId: MANUAL, on: true}});
        }
        var shown = instanceValue(show).shortText;
        if (show && shown && shown.text && shown.text !== HIDE) {
            restore.show = String(shown.text);
            changes.push({action: 'set_value', args: {instanceId: SHOW, value: HIDE}});
        }
        if (!manual && !show) {
            log('the Pebble app shows no Quiet Time settings; leaving them alone');
        }
        if (!changes.length) {
            finishStart();
            return;
        }
        saveRecord(restore);
        armSafety();
        apply(changes, finishStart);
    }
    readSource({plugin: WATCH_SETTINGS, category: 'watch', item: 'switch_setting', all: true}, function (err, list) {
        switches = err ? [] : list || [];
        readBoth();
    }, READ_MS);
    readSource({plugin: WATCH_SETTINGS, category: 'watch', item: 'text_setting', all: true}, function (err, list) {
        texts = err ? [] : list || [];
        readBoth();
    }, READ_MS);
}

// Puts back whatever start() changed. done() is optional. What to put back
// stays in storage until the watch confirms it, so a lost or refused change
// is tried again (and on the next start) rather than leaving Quiet Time's
// notifications hidden for good.
function stop(done) {
    if (starting) {
        stopAfterStart = true;
        if (done) {
            stopWaiters.push(done);
        }
        return;
    }
    if (safety) {
        clearTimeout(safety);
        safety = null;
    }
    var restore = loadRecord();
    if (!restore) {
        if (done) {
            done();
        }
        return;
    }
    if (restoring) {
        if (done) {
            restoring.push(done);
        }
        return;
    }
    var changes = [];
    if (restore.manual !== undefined) {
        changes.push({action: 'set_on', args: {instanceId: MANUAL, on: !!restore.manual}});
    }
    if (restore.show !== undefined) {
        changes.push({action: 'set_value', args: {instanceId: SHOW, value: String(restore.show)}});
    }
    restoring = [];
    if (done) {
        restoring.push(done);
    }
    apply(changes, function (ok) {
        var tries = (restore.tries | 0) + 1;
        if (ok || tries >= RETRIES) {
            if (!ok) {
                log('gave up putting Quiet Time back after ' + tries + ' tries');
            }
            saveRecord(null);
        } else {
            restore.tries = tries;
            saveRecord(restore);
            safety = setTimeout(function () {
                safety = null;
                stop();
            }, RETRY_MS);
        }
        var list = restoring;
        restoring = null;
        list.forEach(function (fn) {
            fn();
        });
    });
}

exports.start = start;
exports.stop = stop;
exports.pending = function () {
    return !!loadRecord();
};
