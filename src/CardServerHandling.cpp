#include "CardServerHandling.h"

#include "settings.h"

#ifdef CARD_SERVER_ENABLED

#include <Arduino.h>
#include <ArduinoHttpClient.h>
#include <vector>
#include <WiFi.h>
#include "ArduinoJson.h"
#include "Common.h"
#include "Log.h"
#include "Queues.h"
#include "Rfid.h"
#include "System.h"

// If PSRAM is available use it allocate memory for JSON-objects
struct SpiRamAllocator {
    void* allocate(size_t size) {
        return ps_malloc(size);

    }
    void deallocate(void* pointer) {
        free(pointer);
    }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;

// old:
// RAM:   [==        ]  19.0% (used 62396 bytes from 327680 bytes)
// Flash: [===       ]  31.6% (used 2070211 bytes from 6553600 bytes)

typedef struct {
    String server;
    uint16_t port;
    String username;
    String password;
} CardServerSettings_t;

typedef struct {
    uint16_t currentTrack;
    uint16_t trackCount;
    String destinationPath;
} DownloadInfo_t;

typedef struct {
    String name;
    String path;
} Track;

enum Types {
      Invalid
    , Command
    , Stream
    , AudioTracks
};

typedef struct {
    enum Types type;
    uint8_t playModeOrCommand; // see values.h for available modes, commands, ...
    std::vector<Track> tracks;
} CardInfo_t;

enum States {
      Idle
    , Login
    , CheckTag
    , GatherCardInfo
    , DownloadFiles
    , DownloadFilesFinished
};

const char apiLoginPath[] PROGMEM = "/api/v1/User/login";

static char CurrentRfidTagId[cardIdStringSize];
static String CurrentApiToken;

static enum States State = States::Idle;
static DownloadInfo_t DownloadInfo;
static CardInfo_t CardInfo;
static CardServerSettings_t CardServerSettings;
static WiFiClient CardServer_WifiClient;
static HttpClient CardServer_HttpClient = HttpClient(CardServer_WifiClient, "localhost", 80); // create a dummy since its not possible to have an invalid client until _init

static JsonObject parseObject(const String& str, bool &ok)
{
    #ifdef BOARD_HAS_PSRAM
        SpiRamJsonDocument doc(1000);
    #else
        StaticJsonDocument<1000> doc;
    #endif

    DeserializationError err = deserializeJson(doc, str);
    JsonObject object = doc.as<JsonObject>();

    if (err) {
        error(err);
        ok = false;
    } else {
        ok = true;
    }
    return object;
}

void CardServer_Init(void)
{
    // Get CardServer settings from NVS
    CardServerSettings.server   = gPrefsSettings.getString("CardServer_Server", "http://cardserver.local");
    CardServerSettings.port     = gPrefsSettings.getUInt("CardServer_Port", 80);
    CardServerSettings.username = gPrefsSettings.getString("CardServer_Username", "CardServerUserLogin");
    CardServerSettings.password = gPrefsSettings.getString("CardServer_Password", "CardServerPassword");

    CardInfo.type = Types::Invalid;
    DownloadInfo.currentTrack = 0;
    DownloadInfo.trackCount = 0;

    CardServer_HttpClient = HttpClient(CardServer_WifiClient, CardServerSettings.server, CardServerSettings.port);
}

static void error(const String& errorStr)
{
    Log_Println(errorStr.c_str(), LOGLEVEL_ERROR);
    System_IndicateError();
    State = States::Idle;
}

static bool login()
{
    String postData = "{\"login\":\"";
    postData += CardServerSettings.username + "\", \"password\":\"" + CardServerSettings.password + "\"}";

    CardServer_HttpClient.post(apiLoginPath);
    int statusCode = CardServer_HttpClient.responseStatusCode();
    if(statusCode != 200) {
        return false;
    }
    bool ok = true;
    const JsonObject response = parseObject(CardServer_HttpClient.responseBody(), ok);

    if(!ok || !response.containsKey("token")) {
        return false;
    }

    CurrentApiToken = response["token"].as<String>();
    return true;
}

static bool tagExists()
{
    assert(true);
    return false;
}

static void gatherCardInfo()
{
    assert(true);
}

static void downloadNextTrack()
{
    assert(true);
}

static void assignCardAsCommand()
{
    char rfidString[12];
    snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s0%s0%s%u%s0", stringDelimiter, stringDelimiter, stringDelimiter, CardInfo.playModeOrCommand, stringDelimiter);
    gPrefsRfid.putString(CurrentRfidTagId, rfidString);
}

static void assignCardAsAudio()
{
    char rfidString[MAX_FILEPATH_LENTGH + 10];
    snprintf(rfidString, sizeof(rfidString) / sizeof(rfidString[0]), "%s%s%s0%s%u%s0", stringDelimiter, DownloadInfo.destinationPath, stringDelimiter, stringDelimiter, CardInfo.playModeOrCommand, stringDelimiter);
    gPrefsRfid.putString(CurrentRfidTagId, rfidString);
    Serial.println(CurrentRfidTagId);
    Serial.println(rfidString);
}

void CardServer_Cyclic(void)
{
    // always just handle one state once, to give others cycle time as well
    switch(State) {
        case States::Idle:
            if(xQueueReceive(gCheckCardServerRfidQueue, &CurrentRfidTagId, 0) == pdTRUE) {
                State = States::Login;
            }
            break;
        case States::Login:
            if (!login()) {
                error("Could not login to card server");
            } else {
                State = States::CheckTag;
            }
            break;
        case States::CheckTag:
            if (!tagExists()) {
                error("Card does not exist on card server");
            } else {
                State = States::GatherCardInfo;
            }
            break;
        case States::GatherCardInfo:
            gatherCardInfo();
            switch (CardInfo.type) {
                case Types::Command:
                    assignCardAsCommand();
                    State = States::Idle;
                    break;
                case Types::Stream:
                    assignCardAsAudio();
                    State = States::Idle;
                    break;
                case Types::AudioTracks:
                    State = States::DownloadFiles;
                    break;
                default:
                    error("Card type not supported");
            }
            break;
        case States::DownloadFiles:
            if(DownloadInfo.currentTrack == DownloadInfo.trackCount) {
                State = States::DownloadFilesFinished;
            } else {
                downloadNextTrack();
            }
            break;
        case States::DownloadFilesFinished:
            assignCardAsAudio();
            State = States::Idle;
            break;
    }
}

#endif
