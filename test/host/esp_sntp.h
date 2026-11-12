#pragma once
void sntp_stop();
void sntp_init();
constexpr int SNTP_OPMODE_POLL = 0;
inline void sntp_setoperatingmode(int) {}
inline void sntp_setservername(int, const char*) {}
enum sntp_sync_status_t { SNTP_SYNC_STATUS_COMPLETED };
inline void sntp_set_sync_status(sntp_sync_status_t) {}
