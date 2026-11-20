# Handoff: Mica（云母，Pebble 三合一 AI 助手）

> 更新：2026-11-20。项目名已从暂定 Wrist 定为 **Mica**。下一步优先 **emery 真机安装 + DeepSeek 实测**，然后恢复 gabbro target 并做圆屏适配、中文化。

## 一句话背景

用户（称呼“佬”，全程中文）做 Pebble Time 2（emery）/ Pebble Round 2（gabbro）AI 语音助手，融合 Bobby、Billy、pebble-ai-companion。核心差异是 BYOA（自带 API key + 自定义 OpenAI 兼容 endpoint），零 Mica 服务器、零订阅。产品 Slogan：**Your AI. Your keys. Your wrist.**

## 项目位置与环境

- 项目根仍为 `~/projects/wrist`（故意保留目录名，勿迁到 oms）
- 本地 Git 仓库，无远端；`reference/` 已 gitignore，勿提交
- 已安装 `uv`、`dtc`、`sdl12-compat`
- 已安装 `pebble-tool 5.2.0`（Python 3.13）和 Pebble SDK `4.44`
- `pebble` 在 `~/.local/bin/pebble`，SDK 在 `~/.local/share/pebble-sdk/SDKs/4.44`

## 本轮完成

1. **品牌**：App/package、C UI、PKJS、设置页、提示词、反馈、示例文本、资源图标及 README 全部改为 Mica；内部 Billy 存储键保留兼容，历史 changelog 与上游署名不改。
2. **emery 已跑通**：
   - `cd app && pebble build` 成功，输出 `app/build/app.pbw`
   - `pebble install --emulator emery` 成功
   - 已完成首次启动三页 consent、位置 Allow、主界面与菜单验证
   - 截图在 `docs/screenshots/`，主图为 `mica-emery-main.png`
   - RAM 约 49 KB / 128 KB，资源约 55 KB / 256 KB
3. **真实构建 bug 修复**：`agent/providers/index.js` 相对 require 路径原来全错；新增 `test/test_provider_facade.js` 防回归。
4. **SDK 4.44**：补齐 manifest v2 的 Internet / LocalNetwork / Location / Timeline / ShowNotifications / LegacyPebbleKit。BYOA 公网自定义域名需要无 domains 限制的 Internet。
5. **构建入口**：上游 `CMakeLists.txt` 实际注释明确说仅供 CLion，pebble-tool 5.2 却会优先拿它构建并报 SDK 不支持。已改名 `app/CMakeLists.clion.txt`，实际继续走 `wscript`。
6. **合规整改**：新增 `NOTICE`、`THIRD_PARTY_NOTICES.md`、`PRIVACY.md`、`docs/LEGAL_REVIEW.md`；修改文件加 Mica modification notice；五份完整许可证（Apache、PAC MIT、Clay MIT、pebble-events MIT、字体 OFL）作为 raw resources 打入 PBW。
7. **第三方服务合规**：
   - 天气/地图组件显示 Open-Meteo / © OpenStreetMap contributors
   - 新增共享 `lib/nominatim.js`：1.1 秒全局限流、50 条缓存、User-Agent、可配置 endpoint
   - `NOMINATIM_BASE_URL`、`STATIC_MAP_BASE_URL` 可在高级设置切换
   - `test_nominatim.js` 验证限流、缓存、User-Agent、自定义端点

## 验证命令

```bash
node test/test_openai_provider.js
node test/test_anthropic_provider.js
node test/test_provider_facade.js
node test/test_nominatim.js
cd app && pebble build
pebble install --emulator emery
pebble screenshot --emulator emery --no-open --scale 3 ../docs/screenshots/mica-emery-main.png
```

四组 Node 测试均通过；最终 PBW 已重新安装到 emery 模拟器，五份许可证通过 `strings app/build/emery/app_resources.pbpack` 确认实际存在。

## 三个上游与法律结论

| 项目 | 许可证 | 本地参考 |
|---|---|---|
| pebble-dev/bobby-assistant | Apache 2.0 | `reference/bobby` |
| TomBolger/Billy | Apache 2.0，本项目直接底座 | `reference/billy` |
| Toyontaker/pebble-ai-companion | MIT，圆屏/provider 参考 | `reference/pebble-ai-companion` |

三者都允许修改和再发布。剩余发布风险主要是：Open-Meteo 免费端点仅非商业、staticmap 服务政策、真机 WebView 是否允许 Nominatim User-Agent、美术素材独立授权证据、Mica 商标/商店重名、商店隐私表单和 Android companion OAuth。详见 `docs/LEGAL_REVIEW.md`，不要对佬说“零法律风险”。

## 下一步顺序

1. **emery 真机**：`cd app && pebble install --phone <IP>`，在 Mica 设置中选 OpenAI-compatible，DeepSeek base URL `https://api.deepseek.com/v1`、model `deepseek-chat`，验证完整工具调用。
2. **gabbro**：当前 `package.json` 暂时只启用 emery。此前恢复 gabbro 后编译会因 `RESOURCE_ID_FENCE_PONY_BITMAP` 和 `RESOURCE_ID_FONT_GOTHIC_36_BOLD` 未定义而失败（资源只声明 emery/basalt，C 无平台保护）。先修资源/平台 guard，再参考 PAC 的 `PBL_IF_ROUND_ELSE` / `PBL_ROUND` 做圆屏 UI，完成后恢复 target。
3. **中文化**：手表 C UI、`agent/prompt.js`、字体/语言包。
4. **发布前**：处理 LEGAL_REVIEW 检查单；Mica 名称尚未做正式商标与商店检索。

## 关键代码认知

- PKJS 入口 `app/src/pkjs/index.js` → Clay 初始化/AppMessage → `agent/runtime_router.js` → `agent/companionless.js`
- Provider 门面 `agent/providers/index.js`；OpenAI `/v1/chat/completions` 和 Anthropic `/v1/messages` 共用中性历史契约
- 工具：`ui_tools.js`、`watch_tools.js`、`profile_tools.js`；工具名里的 `billy` 暂时是内部兼容标识，不是遗漏的 UI 品牌
- `lib/nominatim.js` 是天气与地图共享的唯一地理编码入口，勿再绕开它直接请求公共 Nominatim
- 手表 message key 消费：`src/c/settings/settings.c`、`converse/conversation_manager.c`

## 用户偏好

- 中文交流，称呼“佬”，热情专业；用户做产品决策，AI 全责开发
- 交付可验收成品（PBW、模拟器截图/演示），不要只讲实现
- 项目文件一律在 `~/projects/wrist`，勿污染 oms
