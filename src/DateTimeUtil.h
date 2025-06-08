#ifndef _DATETIMEUTIL_H_
#define _DATETIMEUTIL_H_

#include "time.h"
#include <M5Unified.h>

// DateTime_t → 文字列 (例: "2025-06-01")
// 修正版
String dateToString(const m5::rtc_date_t &date) {
    char buf[11];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", date.year, date.month, date.date);
    return String(buf);
}

String dateTimeToString(const m5::rtc_date_t &date, const m5::rtc_time_t &time) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d", date.month, date.date, time.hours, time.minutes);
    return String(buf);
}

// m5::rtc_date_t → tm
void convertDateFromRTC(const m5::rtc_date_t &rtcDate, tm *date) {
    date->tm_year = rtcDate.year - 1900;
    date->tm_mon  = rtcDate.month - 1;
    date->tm_mday = rtcDate.date;
    date->tm_wday = rtcDate.weekDay;
    date->tm_hour = 0;
    date->tm_min = 0;
    date->tm_sec = 0;
    date->tm_isdst = -1;
}

// tm → m5::rtc_date_t
void convertDateToRTC(m5::rtc_date_t *rtcDate, const tm &date) {
    rtcDate->year  = date.tm_year + 1900;
    rtcDate->month = date.tm_mon + 1;
    rtcDate->date  = date.tm_mday;
    rtcDate->weekDay  = date.tm_wday;
}

void convertTimeToRTC(m5::rtc_time_t *rtcTime, const tm &time) {
    rtcTime->hours   = time.tm_hour;
    rtcTime->minutes = time.tm_min;
    rtcTime->seconds = time.tm_sec;
}

// 日付の加算を行う
// offset = 1 の時、翌日を取得
void offsetDate(m5::rtc_date_t *rtcDate, int offset = 1) {
    // RTC_DateTypeDef -> tm -> time_t の順に変換、日付を加算し、逆の順序で戻す
    tm date;
    convertDateFromRTC(*rtcDate, &date);
    time_t timestamp = mktime(&date);
    timestamp += 86400 * offset; // 86400 = 60*60*24
    date = *(localtime(&timestamp));
    convertDateToRTC(rtcDate, date);
}

#endif // _DATETIMEUTIL_H_
