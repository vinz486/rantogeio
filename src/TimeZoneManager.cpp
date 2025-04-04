#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "TimeZoneManager.h"
#include "settings.h"

// Definizione delle array di stringhe per i fusi orari
const char TIME_ZONE_NAMES[] = "Antarctica/Troll\0Africa/Casablanca\0Africa/El_Aaiun\0Etc/GMT-1\0Etc/GMT-2\0Antarctica/Syowa\0Asia/Aden\0Asia/Amman\0Asia/Baghdad\0Asia/Bahrain\0Asia/Damascus\0Asia/Kuwait\0Asia/Qatar\0Asia/Riyadh\0Europe/Istanbul\0Europe/Minsk\0Etc/GMT-3\0Asia/Tehran\0Asia/Baku\0Asia/Dubai\0Asia/Muscat\0Asia/Tbilisi\0Asia/Yerevan\0Europe/Astrakhan\0Europe/Samara\0Europe/Saratov\0Europe/Ulyanovsk\0Indian/Mahe\0Indian/Mauritius\0Indian/Reunion\0Etc/GMT-4\0Asia/Kabul\0Antarctica/Mawson\0Antarctica/Vostok\0Asia/Almaty\0Asia/Aqtau\0Asia/Aqtobe\0Asia/Ashgabat\0Asia/Atyrau\0Asia/Dushanbe\0Asia/Oral\0Asia/Qyzylorda\0Asia/Samarkand\0Asia/Tashkent\0Asia/Yekaterinburg\0Indian/Kerguelen\0Indian/Maldives\0Etc/GMT-5\0Asia/Colombo\0Asia/Kathmandu\0Asia/Bishkek\0Asia/Dhaka\0Asia/Omsk\0Asia/Thimphu\0Asia/Urumqi\0Indian/Chagos\0Etc/GMT-6\0Asia/Yangon\0Indian/Cocos\0Antarctica/Davis\0Asia/Bangkok\0Asia/Barnaul\0Asia/Ho_Chi_Minh\0Asia/Hovd\0Asia/Krasnoyarsk\0Asia/Novokuznetsk\0Asia/Novosibirsk\0Asia/Phnom_Penh\0Asia/Tomsk\0Asia/Vientiane\0Indian/Christmas\0Etc/GMT-7\0Antarctica/Casey\0Asia/Brunei\0Asia/Choibalsan\0Asia/Irkutsk\0Asia/Kuala_Lumpur\0Asia/Kuching\0Asia/Singapore\0Asia/Ulaanbaatar\0Etc/GMT-8\0Australia/Eucla\0Asia/Chita\0Asia/Dili\0Asia/Khandyga\0Asia/Yakutsk\0Pacific/Palau\0Etc/GMT-9\0Antarctica/DumontDUrville\0Asia/Ust-Nera\0Asia/Vladivostok\0Pacific/Chuuk\0Pacific/Port_Moresby\0Etc/GMT-10\0Australia/Lord_Howe\0Asia/Magadan\0Asia/Sakhalin\0Asia/Srednekolymsk\0Pacific/Bougainville\0Pacific/Efate\0Pacific/Guadalcanal\0Pacific/Kosrae\0Pacific/Noumea\0Pacific/Pohnpei\0Etc/GMT-11\0Pacific/Norfolk\0Asia/Anadyr\0Asia/Kamchatka\0Pacific/Fiji\0Pacific/Funafuti\0Pacific/Kwajalein\0Pacific/Majuro\0Pacific/Nauru\0Pacific/Tarawa\0Pacific/Wake\0Pacific/Wallis\0Etc/GMT-12\0Pacific/Chatham\0Pacific/Apia\0Pacific/Enderbury\0Pacific/Fakaofo\0Pacific/Tongatapu\0Etc/GMT-13\0Pacific/Kiritimati\0Etc/GMT-14\0Atlantic/Cape_Verde\0Etc/GMT+1\0Atlantic/Azores\0America/Noronha\0Atlantic/South_Georgia\0Etc/GMT+2\0America/Godthab\0America/Nuuk\0America/Scoresbysund\0America/Araguaina\0America/Argentina/Buenos_Aires\0America/Argentina/Catamarca\0America/Argentina/Cordoba\0America/Argentina/Jujuy\0America/Argentina/La_Rioja\0America/Argentina/Mendoza\0America/Argentina/Rio_Gallegos\0America/Argentina/Salta\0America/Argentina/San_Juan\0America/Argentina/San_Luis\0America/Argentina/Tucuman\0America/Argentina/Ushuaia\0America/Bahia\0America/Belem\0America/Cayenne\0America/Fortaleza\0America/Maceio\0America/Montevideo\0America/Paramaribo\0America/Punta_Arenas\0America/Recife\0America/Santarem\0America/Sao_Paulo\0Antarctica/Palmer\0Antarctica/Rothera\0Atlantic/Stanley\0Etc/GMT+3\0America/Miquelon\0America/Boa_Vista\0America/Campo_Grande\0America/Caracas\0America/Cuiaba\0America/Guyana\0America/La_Paz\0America/Manaus\0America/Porto_Velho\0Etc/GMT+4\0America/Asuncion\0America/Santiago\0America/Bogota\0America/Eirunepe\0America/Guayaquil\0America/Lima\0America/Rio_Branco\0Etc/GMT+5\0Pacific/Galapagos\0Etc/GMT+6\0Pacific/Easter\0Etc/GMT+7\0Pacific/Pitcairn\0Etc/GMT+8\0Pacific/Gambier\0Etc/GMT+9\0Pacific/Marquesas\0Pacific/Rarotonga\0Pacific/Tahiti\0Etc/GMT+10\0Pacific/Niue\0Etc/GMT+11\0Etc/GMT+12\0Australia/Darwin\0Australia/Adelaide\0Australia/Broken_Hill\0Australia/Brisbane\0Australia/Lindeman\0Antarctica/Macquarie\0Australia/Currie\0Australia/Hobart\0Australia/Melbourne\0Australia/Sydney\0America/Anchorage\0America/Juneau\0America/Metlakatla\0America/Nome\0America/Sitka\0America/Yakutat\0America/Anguilla\0America/Antigua\0America/Aruba\0America/Barbados\0America/Blanc-Sablon\0America/Curacao\0America/Dominica\0America/Grenada\0America/Guadeloupe\0America/Kralendijk\0America/Lower_Princes\0America/Marigot\0America/Martinique\0America/Montserrat\0America/Port_of_Spain\0America/Puerto_Rico\0America/Santo_Domingo\0America/St_Barthelemy\0America/St_Kitts\0America/St_Lucia\0America/St_Thomas\0America/St_Vincent\0America/Tortola\0America/Glace_Bay\0America/Goose_Bay\0America/Halifax\0America/Moncton\0America/Thule\0Atlantic/Bermuda\0Australia/Perth\0Africa/Blantyre\0Africa/Bujumbura\0Africa/Gaborone\0Africa/Harare\0Africa/Juba\0Africa/Khartoum\0Africa/Kigali\0Africa/Lubumbashi\0Africa/Lusaka\0Africa/Maputo\0Africa/Windhoek\0Africa/Algiers\0Africa/Tunis\0Africa/Ceuta\0Arctic/Longyearbyen\0Europe/Amsterdam\0Europe/Andorra\0Europe/Belgrade\0Europe/Berlin\0Europe/Bratislava\0Europe/Brussels\0Europe/Budapest\0Europe/Busingen\0Europe/Copenhagen\0Europe/Gibraltar\0Europe/Ljubljana\0Europe/Luxembourg\0Europe/Madrid\0Europe/Malta\0Europe/Monaco\0Europe/Oslo\0Europe/Paris\0Europe/Podgorica\0Europe/Prague\0Europe/Rome\0Europe/San_Marino\0Europe/Sarajevo\0Europe/Skopje\0Europe/Stockholm\0Europe/Tirane\0Europe/Vaduz\0Europe/Vatican\0Europe/Vienna\0Europe/Warsaw\0Europe/Zagreb\0Europe/Zurich\0Pacific/Guam\0Pacific/Saipan\0America/Havana\0America/Bahia_Banderas\0America/Belize\0America/Chihuahua\0America/Costa_Rica\0America/El_Salvador\0America/Guatemala\0America/Managua\0America/Merida\0America/Mexico_City\0America/Monterrey\0America/Regina\0America/Swift_Current\0America/Tegucigalpa\0America/Chicago\0America/Indiana/Knox\0America/Indiana/Tell_City\0America/Matamoros\0America/Menominee\0America/North_Dakota/Beulah\0America/North_Dakota/Center\0America/North_Dakota/New_Salem\0America/Ojinaga\0America/Rainy_River\0America/Rankin_Inlet\0America/Resolute\0America/Winnipeg\0Asia/Macau\0Asia/Shanghai\0Asia/Taipei\0Africa/Addis_Ababa\0Africa/Asmara\0Africa/Dar_es_Salaam\0Africa/Djibouti\0Africa/Kampala\0Africa/Mogadishu\0Africa/Nairobi\0Indian/Antananarivo\0Indian/Comoro\0Indian/Mayotte\0Africa/Tripoli\0Europe/Kaliningrad\0Asia/Gaza\0Asia/Hebron\0Europe/Chisinau\0Asia/Beirut\0Asia/Famagusta\0Asia/Nicosia\0Europe/Athens\0Europe/Bucharest\0Europe/Helsinki\0Europe/Kiev\0Europe/Mariehamn\0Europe/Riga\0Europe/Sofia\0Europe/Tallinn\0Europe/Uzhgorod\0Europe/Vilnius\0Europe/Zaporozhye\0Africa/Cairo\0America/Atikokan\0America/Cancun\0America/Cayman\0America/Jamaica\0America/Panama\0America/Detroit\0America/Grand_Turk\0America/Indiana/Indianapolis\0America/Indiana/Marengo\0America/Indiana/Petersburg\0America/Indiana/Vevay\0America/Indiana/Vincennes\0America/Indiana/Winamac\0America/Iqaluit\0America/Kentucky/Louisville\0America/Kentucky/Monticello\0America/Montreal\0America/Nassau\0America/New_York\0America/Nipigon\0America/Pangnirtung\0America/Port-au-Prince\0America/Thunder_Bay\0America/Toronto\0Africa/Abidjan\0Africa/Accra\0Africa/Bamako\0Africa/Banjul\0Africa/Bissau\0Africa/Conakry\0Africa/Dakar\0Africa/Freetown\0Africa/Lome\0Africa/Monrovia\0Africa/Nouakchott\0Africa/Ouagadougou\0Africa/Sao_Tome\0America/Danmarkshavn\0Atlantic/Reykjavik\0Atlantic/St_Helena\0Etc/GMT\0Etc/GMT-0\0Etc/GMT0\0Etc/GMT+0\0Etc/Greenwich\0Europe/Guernsey\0Europe/Isle_of_Man\0Europe/Jersey\0Europe/London\0Asia/Hong_Kong\0Pacific/Honolulu\0America/Adak\0Europe/Dublin\0Asia/Jerusalem\0Asia/Kolkata\0Asia/Tokyo\0Asia/Pyongyang\0Asia/Seoul\0Europe/Kirov\0Europe/Moscow\0Europe/Simferopol\0Europe/Volgograd\0America/Creston\0America/Dawson\0America/Dawson_Creek\0America/Fort_Nelson\0America/Hermosillo\0America/Mazatlan\0America/Phoenix\0America/Whitehorse\0America/Boise\0America/Cambridge_Bay\0America/Denver\0America/Edmonton\0America/Inuvik\0America/Yellowknife\0America/St_Johns\0Antarctica/McMurdo\0Pacific/Auckland\0Asia/Karachi\0Asia/Manila\0America/Los_Angeles\0America/Tijuana\0America/Vancouver\0Africa/Johannesburg\0Africa/Maseru\0Africa/Mbabane\0Pacific/Midway\0Pacific/Pago_Pago\0Etc/UCT\0Etc/UTC\0Etc/Universal\0Etc/Zulu\0Africa/Bangui\0Africa/Brazzaville\0Africa/Douala\0Africa/Kinshasa\0Africa/Lagos\0Africa/Libreville\0Africa/Luanda\0Africa/Malabo\0Africa/Ndjamena\0Africa/Niamey\0Africa/Porto-Novo\0Atlantic/Canary\0Atlantic/Faroe\0Atlantic/Madeira\0Europe/Lisbon\0Asia/Jakarta\0Asia/Pontianak\0Asia/Jayapura\0Asia/Makassar\0";

const char TIME_ZONE_STRINGS[] = "<+00>0<+02>-2,M3.5.0/1,M10.5.0/3\0~\0~\0<+01>-1\0<+02>-2\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<+03>-3\0<+0330>-3:30\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<+04>-4\0<+0430>-4:30\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<+05>-5\0<+0530>-5:30\0<+0545>-5:45\0~\0~\0~\0~\0~\0~\0<+06>-6\0~\0<+0630>-6:30\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<+07>-7\0~\0~\0~\0~\0~\0~\0~\0~\0<+08>-8\0<+0845>-8:45\0~\0~\0~\0~\0~\0<+09>-9\0~\0~\0~\0~\0~\0<+10>-10\0<+1030>-10:30<+11>-11,M10.1.0,M4.1.0\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<+11>-11\0<+11>-11<+12>,M10.1.0,M4.1.0/3\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<+12>-12\0<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45\0~\0~\0~\0~\0<+13>-13\0~\0<+14>-14\0~\0<-01>1\0<-01>1<+00>,M3.5.0/0,M10.5.0/1\0~\0~\0<-02>2\0~\0~\0<-02>2<-01>,M3.5.0/-1,M10.5.0/0\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0<-03>3\0<-03>3<-02>,M3.2.0,M11.1.0\0~\0~\0~\0~\0~\0~\0~\0~\0<-04>4\0<-04>4<-03>,M10.1.0/0,M3.4.0/0\0<-04>4<-03>,M9.1.6/24,M4.1.6/24\0~\0~\0~\0~\0~\0<-05>5\0~\0<-06>6\0<-06>6<-05>,M9.1.6/22,M4.1.6/22\0<-07>7\0~\0<-08>8\0~\0<-09>9\0<-0930>9:30\0~\0~\0<-10>10\0~\0<-11>11\0<-12>12\0ACST-9:30\0~\0ACST-9:30ACDT,M10.1.0,M4.1.0/3\0~\0AEST-10\0~\0~\0~\0~\0AEST-10AEDT,M10.1.0,M4.1.0/3\0~\0~\0~\0~\0~\0AKST9AKDT,M3.2.0,M11.1.0\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0AST4\0~\0~\0~\0~\0~\0AST4ADT,M3.2.0,M11.1.0\0AWST-8\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0CAT-2\0~\0CET-1\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0CET-1CEST,M3.5.0,M10.5.0/3\0~\0ChST-10\0CST5CDT,M3.2.0/0,M11.1.0/1\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0CST6\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0CST6CDT,M3.2.0,M11.1.0\0~\0~\0CST-8\0~\0~\0~\0~\0~\0~\0~\0~\0~\0EAT-3\0~\0EET-2\0~\0EET-2EEST,M3.4.4/50,M10.4.4/50\0EET-2EEST,M3.5.0,M10.5.0/3\0EET-2EEST,M3.5.0/0,M10.5.0/0\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0EET-2EEST,M3.5.0/3,M10.5.0/4\0EET-2EEST,M4.5.5/0,M10.5.4/24\0~\0~\0~\0~\0EST5\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0EST5EDT,M3.2.0,M11.1.0\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0GMT0\0~\0~\0~\0GMT0BST,M3.5.0/1,M10.5.0\0HKT-8\0HST10\0HST10HDT,M3.2.0,M11.1.0\0IST-1GMT0,M10.5.0,M3.5.0/1\0IST-2IDT,M3.4.4/26,M10.5.0\0IST-5:30\0JST-9\0~\0KST-9\0~\0~\0~\0MSK-3\0~\0~\0~\0~\0~\0~\0~\0MST7\0~\0~\0~\0~\0~\0MST7MDT,M3.2.0,M11.1.0\0NST3:30NDT,M3.2.0,M11.1.0\0~\0NZST-12NZDT,M9.5.0,M4.1.0/3\0PKT-5\0PST-8\0~\0~\0PST8PDT,M3.2.0,M11.1.0\0~\0~\0SAST-2\0~\0SST11\0~\0~\0~\0UTC0\0~\0~\0~\0~\0~\0~\0~\0~\0~\0~\0WAT-1\0~\0~\0~\0WET0WEST,M3.5.0/1,M10.5.0\0~\0WIB-7\0WIT-9\0WITA-8\0";

// Variabile Preferences (assicurarsi che non ci sia conflitto con altre istanze)
Preferences preferences;

void TimeZoneManager::set_logger(logger_cb_t logger) {
  _logger = logger;
}


void TimeZoneManager::begin(ClockManager *clock) {
  _preferences.begin("time", false);
  String tz = _preferences.getString("tz", "");
  _preferences.end();

  if (tz != "") {
    setenv("TZ", tz.c_str(), 1);
    tzset();
    (*_logger)("Loaded TZ = %s", tz.c_str());
  } else {
    if (WiFi.status() == WL_CONNECTED) {
      set_from_api();
    }
  }

  clock->set_displayed_time_to_current();
}

void TimeZoneManager::set(String tz) {
  const char* posix = convert_name(tz.c_str());
  setenv("TZ", posix, 1);
  tzset();

  _preferences.begin("time", false);
  _preferences.putString("tz", String(posix));
  _preferences.end();

  (*_logger)("Set time zone to %s", tz);
  (*_logger)("TZ = %s", posix);
}

void TimeZoneManager::set_from_api() {
  #if ENABLE_WIFI == 1
  WiFiClient client;
  HTTPClient http;

  (*_logger)("Requesting time zone from ip-api.com");

  http.begin(client, "http://ip-api.com/line/?fields=256");
  int httpResponseCode = http.GET();

  String payload = "";
  if (httpResponseCode > 0) {
    payload = http.getString();
    payload.trim();
    set(payload);
  } else {
    (*_logger)("Error code: %s", httpResponseCode);
  }
  http.end();
  #endif
}

const char* TimeZoneManager::convert_name(const char* tzdata) {
  int offset = 0;
  const char* current = TIME_ZONE_NAMES;
  while (strlen(current) > 0) {
    if (strcmp(tzdata, current) == 0) {
      const char* posix = TIME_ZONE_STRINGS;
      while (offset > 0 || strcmp(posix, "~") == 0) {
        posix = posix + strlen(posix) + 1;
        offset--;
      }
      return posix;
    }

    current = current + strlen(current) + 1;
    offset++;
  }

  return "UTC0";
}