/*
 * Copyright 2026 Mica contributors.
 * Licensed under the Apache License, Version 2.0.
 */
#include "i18n.h"
#include "../settings/settings.h"

static int s_language = MICA_LANGUAGE_ZH;

static const char *const s_en[STR_COUNT] = {
  [STR_ALARMS] = "Alarms",
  [STR_TIMERS] = "Timers",
  [STR_REMINDERS] = "Reminders",
  [STR_QUOTA] = "Usage",
  [STR_ABOUT] = "About",
  [STR_LEGAL] = "Legal",
  [STR_GOOD_MORNING] = "Good morning!",
  [STR_GOOD_AFTERNOON] = "Good afternoon!",
  [STR_GOOD_EVENING] = "Good evening!",
  [STR_GOOD_NIGHT] = "Hey there, night owl!",
  [STR_HOLD_SELECT] = "Hold Select to ask Mica.",
  [STR_YOU] = "You",
  [STR_DICTATE] = "Dictate...",
  [STR_NO_TIMERS] = "No timers set. Ask Mica to set some.",
  [STR_NO_ALARMS] = "No alarms set. Ask Mica to set some.",
  [STR_NO_REMINDERS] = "No reminders. Ask Mica to set some.",
  [STR_ALLOW] = "Allow",
  [STR_DENY] = "Deny",
  [STR_DELETE] = "Delete",
  [STR_SENT] = "Sent",
  [STR_THANK_YOU] = "Thank you!",
  [STR_ERROR_RETRY] = "There was a problem. Please try again.",
  [STR_SNOOZED] = "Snoozed",
  [STR_FAILED] = "Failed",
  [STR_IMPORTANT] = "Important",
  [STR_PRIVACY] = "Privacy",
  [STR_LOCATION] = "Location",
  [STR_TODAY] = "Today",
  [STR_TOMORROW] = "Tomorrow",
  [STR_TIMES_UP] = "Time's up!",
  [STR_ALARM] = "Alarm!",
  [STR_NO_SUBSCRIPTION] = "Mica has no subscription. Add your AI provider API key in the Mica settings on your phone.",
  [STR_TEST_CONNECTION] = "Connection test",
};

static const char *const s_zh[STR_COUNT] = {
  [STR_ALARMS] = "闹钟",
  [STR_TIMERS] = "定时器",
  [STR_REMINDERS] = "提醒",
  [STR_QUOTA] = "用量",
  [STR_ABOUT] = "关于",
  [STR_LEGAL] = "法律信息",
  [STR_GOOD_MORNING] = "早上好！",
  [STR_GOOD_AFTERNOON] = "下午好！",
  [STR_GOOD_EVENING] = "晚上好！",
  [STR_GOOD_NIGHT] = "夜猫子，你好！",
  [STR_HOLD_SELECT] = "按住确认键，向 Mica 提问",
  [STR_YOU] = "你",
  [STR_DICTATE] = "语音输入…",
  [STR_NO_TIMERS] = "还没有定时器\n让 Mica 帮你设置吧",
  [STR_NO_ALARMS] = "还没有闹钟\n让 Mica 帮你设置吧",
  [STR_NO_REMINDERS] = "还没有提醒\n让 Mica 帮你设置吧",
  [STR_ALLOW] = "允许",
  [STR_DENY] = "拒绝",
  [STR_DELETE] = "删除",
  [STR_SENT] = "已发送",
  [STR_THANK_YOU] = "谢谢你的反馈！",
  [STR_ERROR_RETRY] = "出了点问题，请重试。",
  [STR_SNOOZED] = "已稍后提醒",
  [STR_FAILED] = "失败",
  [STR_IMPORTANT] = "重要提示",
  [STR_PRIVACY] = "隐私",
  [STR_LOCATION] = "位置",
  [STR_TODAY] = "今天",
  [STR_TOMORROW] = "明天",
  [STR_TIMES_UP] = "时间到！",
  [STR_ALARM] = "闹钟！",
  [STR_NO_SUBSCRIPTION] = "Mica 不收取订阅费。请在手机上的 Mica 设置中填写你的 AI Provider API Key。",
  [STR_TEST_CONNECTION] = "连接测试",
};

void i18n_init(void) {
  s_language = settings_get_ui_language();
}

void i18n_set_language(int language) {
  s_language = language == MICA_LANGUAGE_EN ? MICA_LANGUAGE_EN : MICA_LANGUAGE_ZH;
}

int i18n_get_language(void) {
  return s_language;
}

bool i18n_is_chinese(void) {
  return s_language == MICA_LANGUAGE_ZH;
}

const char *S(MicaStringId id) {
  if (id >= STR_COUNT) {
    return "";
  }
  const char *value = i18n_is_chinese() ? s_zh[id] : s_en[id];
  return value ? value : "";
}
