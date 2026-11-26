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

var location = require('./location');
var session = require('./session');
var quota = require('./quota');
var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var customConfigFunction = require('./custom_config');
var config = require('./config');
var reminders = require('./reminders');
var feedback = require('./lib/feedback');
var quiet = require('./lib/quiet');
var package_json = require('package.json');
var runtimeRouter = require('./agent/runtime_router');
var diagnostics = require('./agent/diagnostics');


// The diagnostics report reaches the settings page through Clay's userData
// channel: Clay re-reads it every time the page is generated, so refreshing
// the object before Clay's own showConfiguration handler runs (this
// listener is registered first) keeps the page current. The report itself
// never becomes a setting, so it is never sent to the watch.
var diagnosticsUserData = {diagnosticsReport: 'No diagnostics recorded yet.'};

function refreshDiagnosticsReport() {
    try {
        diagnosticsUserData.diagnosticsReport = diagnostics.formatReport();
    } catch (e) {
        // Diagnostics must never break the app.
    }
}

Pebble.addEventListener('showConfiguration', refreshDiagnosticsReport);

var clay = new Clay(clayConfig, customConfigFunction, {userData: diagnosticsUserData});

// Registered after Clay's own webviewclosed handler, so Clay has already
// saved the settings to localStorage when this runs.
Pebble.addEventListener('webviewclosed', function(e) {
    if (!e || !e.response) {
        return;
    }
    try {
        if (config.getSetting('CLEAR_DIAGNOSTICS', false)) {
            diagnostics.clear();
            config.setSetting('CLEAR_DIAGNOSTICS', false);
            diagnostics.record('diagnostics', 'log cleared from settings');
            refreshDiagnosticsReport();
        }
    } catch (err) {
        // Diagnostics must never break the app.
    }
});

function main() {
    // Put back any Quiet Time change a previous run left behind (the watch
    // app was closed mid-dictation, the phone restarted, ...). No-op when
    // there is nothing to restore.
    quiet.stop();
    location.update();
    Pebble.addEventListener('appmessage', handleAppMessage);
}

function runAiTest() {
    var agentProviders = require('./agent/providers');
    var startedAt = Date.now();
    agentProviders.generate(
        [{type: 'user_input', content: [{type: 'text', text: 'ping'}]}],
        {systemInstruction: 'You are Mica. Reply with exactly: OK', tools: undefined, enableSearchGrounding: false},
        function(err, response) {
            var providerName = agentProviders.currentProviderName();
            var seconds = ((Date.now() - startedAt) / 1000).toFixed(1);
            if (err) {
                Pebble.showSimpleNotificationOnPebble('Mica 连接失败',
                    providerName + ' · ' + seconds + 's\n' + err.message);
                return;
            }
            Pebble.showSimpleNotificationOnPebble('Mica 连接成功',
                providerName + ' · ' + seconds + 's\n回复: ' +
                String(response && response.text ? response.text : '(空)').substring(0, 80));
        }
    );
}

function handleAppMessage(e) {
    try {
        handleAppMessageInner(e);
    } catch (err) {
        diagnostics.record('exception', 'handleAppMessage: ' + (err && err.message ? err.message : err));
        throw err;
    }
}

function handleAppMessageInner(e) {
    console.log("Inbound app message!");
    console.log(JSON.stringify(e));
    var data = e.payload;
    if (data.ANDROID_COMPANION_READY) {
        runtimeRouter.recordAndroidCompanionSeen(data.ANDROID_REQUEST_ID);
        return;
    }
    if ('QUIET_START' in data) {
        // The watch wants to dictate: engage Quiet Time (with notifications
        // hidden) first, then let it start. When the setting is off, or the
        // plugin API is unavailable, quiet.start() resolves immediately, so
        // the watch is never kept waiting; it also has its own timeout.
        if (config.getSetting('QUIET_LISTEN', true)) {
            quiet.start(function() {
                Pebble.sendAppMessage({QUIET_READY: 1});
            });
        } else {
            Pebble.sendAppMessage({QUIET_READY: 1});
        }
        return;
    }
    if ('QUIET_STOP' in data) {
        quiet.stop();
        return;
    }
    if (data.AI_TEST_REQUEST) {
        runAiTest();
        return;
    }
    if (data.PROMPT) {
        console.log("Starting a new Session...");
        diagnostics.record('session_start', 'thread=' + (data.THREAD_ID || 'new'));
        diagnostics.record('prompt', String(data.PROMPT).substring(0, 200));
        var s = new session.Session(data.PROMPT, data.THREAD_ID, data.ANDROID_REQUEST_ID);
        s.run();
        return;
    }

    if (reminders.handleReminderMessage(data)) {
        return;
    }

    if (data.QUOTA_REQUEST) {
        console.log("Requesting quota...");
        quota.handleQuotaRequest();
    }
    if ('LOCATION_ENABLED' in data) {
        config.setSetting("LOCATION_ENABLED", !!data.LOCATION_ENABLED);
        console.log("Location enabled: " + config.isLocationEnabled());
        // We need to confirm that we received this for the watch to proceed.
        Pebble.sendAppMessage({
            LOCATION_ENABLED: data.LOCATION_ENABLED,
        });
    }
    if ('FEEDBACK_TEXT' in data) {
        console.log("Handling feedback...");
        feedback.handleFeedbackRequest(data);
    }
    if ('REPORT_THREAD_UUID' in data) {
        console.log("Handling report...");
        feedback.handleReportRequest(data);
    }
}

function doCobbleWarning() {
    if (window.cobble) {
        console.log("WARNING: Running Mica on Cobble is not supported, and has multiple known issues.");
        Pebble.sendAppMessage({COBBLE_WARNING: 1});
    }
}

Pebble.addEventListener("ready",
    function(e) {
        // This happens before anything else because I don't trust Cobble to get through the normal flow,
        // given how many things bizarrely don't work.
        doCobbleWarning();
        console.log("Mica " + package_json['version']);
        if (Pebble.platform === 'pypkjs') {
            console.log("Entering emulator mode.");
            var emulator_main = require('./emulator/emulator_main');
            emulator_main.main();
            return;
        }
        Pebble.getTimelineToken(function(token) {
            console.log("Entering real mode.");
            session.userToken = token;
            main();
        }, function(e) {
            console.log("Get timeline token failed???", e);
        })
    }
);
