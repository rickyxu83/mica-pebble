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

// This function is serialised into Clay's settings page. Keep it self-contained.
// IMPORTANT: in @rebble/clay 1.0.6 the custom function runs BEFORE build();
// every item lookup must happen inside the AFTER_BUILD handler.
module.exports = function(minified) {
    var clayConfig = this;

    clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
        var provider = clayConfig.getItemByMessageKey('LLM_PROVIDER');
        var service = clayConfig.getItemByMessageKey('OPENAI_SERVICE');
        var advanced = clayConfig.getItemById('SHOW_ADVANCED');
        var baseUrl = clayConfig.getItemByMessageKey('OPENAI_BASE_URL');
        var openaiModel = clayConfig.getItemByMessageKey('OPENAI_MODEL');

        var templates = {
            deepseek: {base: 'https://api.deepseek.com/v1', model: 'deepseek-chat'},
            openai: {base: 'https://api.openai.com/v1', model: 'gpt-4o-mini'},
            kimi: {base: 'https://api.moonshot.cn/v1', model: 'moonshot-v1-8k'},
            qwen: {base: 'https://dashscope.aliyuncs.com/compatible-mode/v1', model: 'qwen-plus'},
            openrouter: {base: 'https://openrouter.ai/api/v1', model: 'openai/gpt-4o-mini'},
            ollama: {base: 'http://<电脑IP>:11434/v1', model: 'llama3.1'},
            lmstudio: {base: 'http://<电脑IP>:1234/v1', model: 'local-model'},
            custom: {base: '', model: ''}
        };
        var knownBaseUrls = [''].concat(Object.keys(templates).map(function(key) {
            return templates[key].base;
        }));
        var knownModels = ['', 'gpt-4o-mini', 'deepseek-chat', 'moonshot-v1-8k', 'qwen-plus', 'llama3.1', 'local-model', 'openai/gpt-4o-mini'];

        function show(item, visible) {
            if (item) {
                if (visible) {
                    item.show();
                } else {
                    item.hide();
                }
            }
        }

        function byKeyOrId(key) {
            return clayConfig.getItemByMessageKey(key) || clayConfig.getItemById(key);
        }

        function applyTemplate() {
            var template = templates[service.get()] || templates.custom;
            var currentBase = String(baseUrl.get() || '');
            var currentModel = String(openaiModel.get() || '');
            if (!currentBase || knownBaseUrls.indexOf(currentBase) >= 0) {
                baseUrl.set(template.base);
            }
            if (!currentModel || knownModels.indexOf(currentModel) >= 0) {
                openaiModel.set(template.model);
            }
        }

        function updateVisibility() {
            var currentProvider = provider.get();
            var customEndpoint = service.get() === 'custom';
            var showAdvanced = advanced.get();

            show(byKeyOrId('GEMINI_API_KEY'), currentProvider === 'gemini');
            show(byKeyOrId('CLAUDE_API_KEY'), currentProvider === 'claude');
            show(byKeyOrId('OPENAI_API_KEY'), currentProvider === 'openai');
            show(service, currentProvider === 'openai');

            show(baseUrl, currentProvider === 'openai' && (customEndpoint || showAdvanced));
            show(openaiModel, currentProvider === 'openai' && (customEndpoint || showAdvanced));
            show(byKeyOrId('GEMINI_MODEL'), currentProvider === 'gemini' && showAdvanced);
            show(byKeyOrId('CLAUDE_MODEL'), currentProvider === 'claude' && showAdvanced);

            [
                'ASSISTANT_RUNTIME',
                'GEMINI_MONTHLY_BUDGET_USD',
                'GITHUB_ISSUES_URL',
                'FEEDBACK_POST_URL',
                'NOMINATIM_BASE_URL',
                'STATIC_MAP_BASE_URL',
                'USER_PROFILE_CONTEXT'
            ].forEach(function(key) {
                show(byKeyOrId(key), showAdvanced);
            });
        }

        applyTemplate();
        updateVisibility();
        provider.on('change', updateVisibility);
        service.on('change', function() {
            applyTemplate();
            updateVisibility();
        });
        advanced.on('change', updateVisibility);
    });
};
