# Mica 参考项目与发布合规审查

审查日期：2026-11-20。范围是本地三个参考仓库、当前 fork 的源码/资源、打包依赖及代码中直接调用的数据服务。本文是工程风险排查，不是正式法律意见；商业发布或上架前仍建议由合格律师复核。

## 结论

**三个参考项目都明确允许使用、修改和再发布，许可证之间没有阻止当前 Apache 2.0 fork 的冲突。** 当前主要风险不在“能不能 fork”，而在分发义务、素材权属证明、第三方服务条款和隐私告知。私人自用风险较低；公开分发 PBW、上架或商业化前必须完成下列事项。

## 审查对象与许可证

| 项目 | 本地审查版本 | 许可证 | 修改/再发布 | 当前使用方式与风险 |
|---|---|---|---|---|
| pebble-dev/bobby-assistant | `142d92b53876b6704b391b788154d4cba4a6fb52` | Apache 2.0 | 允许，含专利授权 | Billy 的上游；原版权头必须保留。低风险，无上游 NOTICE 文件 |
| TomBolger/Billy | `c949df4df3c2d28b6ad91dffd907a361d8b61b82` | Apache 2.0 | 允许，含专利授权 | Mica 直接代码底座；须标修改、保留版权、随分发附许可证。美术署名必须保留 |
| Toyontaker/pebble-ai-companion | `a7a1e3c3de50acab12a94b95d1046b0ccebc5654` | MIT，Copyright 2026 Toyontaker | 允许 | 当前作为圆屏和 provider 设计参考；若复制代码或实质内容，副本必须含 MIT 版权及许可全文。现已预防性随包保留 |

Apache 2.0 与 MIT 都允许商业使用和闭源衍生，但 Mica 选择继续以 Apache 2.0 发布也可以。MIT 不强制衍生项目整体改为 MIT；Apache 2.0 的修改声明、版权保留和许可证分发义务仍要满足。

## 已完成的整改

1. 根目录保留完整 Apache 2.0 `LICENSE`，新增 `NOTICE` 和 `THIRD_PARTY_NOTICES.md`，记录 Billy/Bobby 来源、Mica 修改、PAC/依赖及素材署名。
2. 修改过的上游 C/JS 文件添加 `Modified in 2026 for Mica`；不清除 Google、Steffan Robert William Donal 等原有文件头。
3. `app/package.json` 保留 Thomas Bolger 作者署名，同时加入 Mica contributors；App 名称改为 Mica，避免暗示上游原版。
4. 完整许可证文本复制到 `app/resources/text/licenses/` 并作为 raw resources 打进 PBW：Apache 2.0、PAC MIT、Clay MIT、pebble-events MIT、字体 OFL 1.1。
5. About 页面保留 Thomas Bolger、Katharine Berry、Sarah Bolger、Stasia Michalska 和 AmberW 署名。
6. 新增 `PRIVACY.md`，更新 AI、位置、反馈文本，使其符合当前多 provider 实现，不再声称只会把数据发给 Gemini。
7. 新 SDK 的权限清单明确声明 Internet、LocalNetwork、Location、Timeline、ShowNotifications 和 LegacyPebbleKit；BYOA 自定义公网域名需要通用 Internet 权限，本地模型另需 LocalNetwork。
8. 天气与地图组件增加可见的 Open-Meteo / OpenStreetMap 署名；法律页提供数据许可入口。新增共享 Nominatim 客户端，实现全应用 1.1 秒请求间隔、50 条会话缓存、应用身份 User-Agent 和可配置切换端点，并通过 Node 测试。

## 剩余风险与发布门槛

### 高：公共地理/天气服务不是无条件免费 API

- 代码使用 `api.open-meteo.com`。官方条款要求署名；免费端点仅限非商业用途，并有限流。Mica 若收费、投广告或作为商业产品，需使用付费 customer endpoint 或更换服务。
- 公共 Nominatim 政策要求限流、可识别 User-Agent、缓存、署名和不靠 App 更新切换服务。代码已完成这些工程整改；真机仍需确认宿主 WebView 不拦截自定义 User-Agent，若拦截则必须改用自有/兼容的地理编码端点。
- `staticmap.openstreetmap.de` 是第三方公共服务，不是无限容量的自有地图服务器。已允许配置替代端点，但仍需核查其当前服务条款，并为限流/停用准备实际可用的替代服务。
- OpenStreetMap 数据是 ODbL；地图组件现已显示 `© OpenStreetMap contributors`，法律页提供 `https://www.openstreetmap.org/copyright`。后续改 UI 时不得移除该署名。

### 高：公开收集/传输数据前的隐私与商店合规

已补隐私政策和应用内告知，但公开上架还需填写商店隐私信息并确保与真实行为一致。自定义 endpoint 可能收到提示词、个人记忆和位置；用户选择非 HTTPS 公网端点会有泄露风险，应限制或明确警告。可选 Android companion 的 Google OAuth scope、验证、Play 政策及数据处理声明需要独立审查，不能由这份 PBW 审查覆盖。

### 中：美术素材的授权链证据不如代码完整

Billy 根许可证是 Apache 2.0，README 明确署名 Sarah Bolger、Katherine/Katharine Berry，About 还列 Stasia Michalska；但仓库未找到独立素材许可证或授权记录。`plan.md` 还记录了从作者本地 Google Drive 转换山羊 PNG 的过程。合理工程解释是素材作为 Billy 仓库一部分按 Apache 2.0 提供，但若要商业上架，最好让上游确认素材授权或换成 Mica 原创图标和角色。当前 goat 图未见明显第三方角色标识，仍不能据此证明权属。

### 中：商标与名称

Apache/MIT 不授予 Pebble、Rebble、Google、Bobby 或 Billy 商标权。Mica 可以描述“兼容 Pebble”及说明 fork 来源，但不能暗示官方出品或背书。`Mica` 本身尚未完成商标、应用商店和域名清查，正式公开发布前需另做名称检索。

### 低：贡献流程

上游带 DCO 要求的 `CONTRIBUTING.md` 已保留。后续公开协作时提交应带匹配作者邮箱的 `Signed-off-by`，并记录复制 PAC 代码的文件与版本，方便持续审计。

## 发布前最小检查单

- [x] 修复 Nominatim User-Agent、全局限流、缓存和可配置切换；staticmap 替代端点已可配置，服务政策仍待核查
- [x] 在天气卡片和地图附近补 Open-Meteo / OSM 可见署名与许可入口
- [ ] 确认只作非商业用途，或为 Open-Meteo 商业使用付费/换源
- [ ] 确认美术授权，或替换为 Mica 原创素材
- [ ] 完成 Mica 名称与商标检索
- [ ] 商店隐私表单、Privacy Policy、实际网络行为一致
- [ ] Android companion 单独完成 Google OAuth / Play / 隐私审查
- [ ] 解压最终 PBW，确认五份完整许可证和署名文本仍在

## 官方依据

- Apache 2.0 正文：本地三个仓库的 `LICENSE`；尤其第 4 条再分发义务和第 6 条商标限制
- PAC MIT：`reference/pebble-ai-companion/LICENSE`
- 字体 OFL：`app/resources/fonts/LICENSE`
- Open-Meteo 许可与条款：<https://open-meteo.com/en/license>、<https://open-meteo.com/en/terms>
- Nominatim 政策：<https://operations.osmfoundation.org/policies/nominatim/>
- OSM 署名指南：<https://osmfoundation.org/wiki/Licence/Attribution_Guidelines>
- Pebble manifest 权限：<https://developer.repebble.com/guides/plugins/preview/>
