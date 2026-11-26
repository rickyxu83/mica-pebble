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

exports.getSettings = function() {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
}

exports.getSetting = function(key, defaultValue) {
    var settings = exports.getSettings();
    if (settings[key] !== undefined) {
        return settings[key];
    }
    return defaultValue;
}

exports.setSetting = function(key, value) {
    var settings = exports.getSettings();
    settings[key] = value;
    localStorage.setItem('clay-settings', JSON.stringify(settings));
}

exports.isLocationEnabled = function() {
    return !!exports.getSettings()['LOCATION_ENABLED'];
}

exports.RUNTIME_AUTOMATIC = 'automatic';
exports.RUNTIME_COMPANIONLESS = 'companionless';
exports.RUNTIME_ANDROID = 'android';

exports.getAssistantRuntime = function() {
    return exports.getSetting('ASSISTANT_RUNTIME', exports.RUNTIME_AUTOMATIC);
}

exports.PROVIDER_GEMINI = 'gemini';
exports.PROVIDER_OPENAI = 'openai';
exports.PROVIDER_CLAUDE = 'claude';

exports.getProvider = function() {
    var provider = String(exports.getSetting('LLM_PROVIDER', exports.PROVIDER_GEMINI) || exports.PROVIDER_GEMINI);
    if ([exports.PROVIDER_GEMINI, exports.PROVIDER_OPENAI, exports.PROVIDER_CLAUDE].indexOf(provider) === -1) {
        return exports.PROVIDER_GEMINI;
    }
    return provider;
}

exports.getGeminiApiKey = function() {
    return String(exports.getSetting('GEMINI_API_KEY', '') || '').replace(/\s+/g, '');
}

exports.getOpenaiApiKey = function() {
    return String(exports.getSetting('OPENAI_API_KEY', '') || '').replace(/\s+/g, '');
}

exports.getOpenaiBaseUrl = function() {
    return String(exports.getSetting('OPENAI_BASE_URL', '') || '').replace(/^\s+|\s+$/g, '');
}

exports.getOpenaiModel = function() {
    var model = String(exports.getSetting('OPENAI_MODEL', 'gpt-4o-mini') || 'gpt-4o-mini').replace(/\s+/g, '');
    return model || 'gpt-4o-mini';
}

exports.getClaudeApiKey = function() {
    return String(exports.getSetting('CLAUDE_API_KEY', '') || '').replace(/\s+/g, '');
}

exports.getClaudeModel = function() {
    var model = String(exports.getSetting('CLAUDE_MODEL', 'claude-sonnet-4-5') || 'claude-sonnet-4-5').replace(/\s+/g, '');
    return model || 'claude-sonnet-4-5';
}

exports.getGeminiModel = function() {
    var model = String(exports.getSetting('GEMINI_MODEL', 'gemini-3.1-flash-lite') || 'gemini-3.1-flash-lite').replace(/\s+/g, '');
    if (!model || model === 'gemini-3.5-flash') {
        return 'gemini-3.1-flash-lite';
    }
    return model;
}

exports.getGeminiMonthlyBudgetUsd = function() {
    return exports.getSetting('GEMINI_MONTHLY_BUDGET_USD', '10');
}

exports.getGithubIssuesUrl = function() {
    var value = String(exports.getSetting('GITHUB_ISSUES_URL', '') || '').replace(/^\s+|\s+$/g, '');
    // Default: Mica's own issue tracker, so watch feedback opens a
    // prefilled issue the author actually sees (previously a blank
    // setting silently dropped the feedback). Users can override
    // this URL, or set a feedback POST endpoint, in advanced settings.
    return value || 'https://github.com/rickyxu83/mica-pebble/issues/new';
}

exports.getFeedbackPostUrl = function() {
    return String(exports.getSetting('FEEDBACK_POST_URL', '') || '').replace(/^\s+|\s+$/g, '');
}

exports.getNominatimBaseUrl = function() {
    var value = String(exports.getSetting('NOMINATIM_BASE_URL', '') || '').replace(/\s+$/g, '').replace(/\/+$/g, '');
    return value || 'https://nominatim.openstreetmap.org';
}

exports.getStaticMapBaseUrl = function() {
    var value = String(exports.getSetting('STATIC_MAP_BASE_URL', '') || '').replace(/\s+$/g, '').replace(/\/+$/g, '');
    return value || 'https://staticmap.openstreetmap.de/staticmap.php';
}

exports.getUserProfileContext = function() {
    return String(exports.getSetting('USER_PROFILE_CONTEXT', '') || '')
        .replace(/[\r\n]+/g, ' ')
        .replace(/\s+/g, ' ')
        .replace(/^\s+|\s+$/g, '')
        .substring(0, 1200);
}
