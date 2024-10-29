#include <argparse/argparse.hpp>
#include <sqlite/sqlite3.h>

#include <fit_sdk/fit_decode.hpp>
#include <fit_sdk/fit_mesg_broadcaster.hpp>

#define FMT_HEADER_ONLY
#include "fmt/format.h"

#include <filesystem>
namespace fs = std::filesystem;
#include <string>
#include <regex>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef VERSION
const std::string VERSION_STR = VERSION;
#else
const std::string VERSION_STR = "0.0";
#endif

const double DEG_TO_SEMICIRCLE = (double)(1L << 31) / 180.0;
const FIT_UINT32 FIT_EPOCH_DELTA = 631065600;  // epoch starts 1990 (vs 1970 for unix epoch)

struct LocationData {
    FIT_DATE_TIME timestamp;
    FIT_SINT32 lat;
    FIT_SINT32 lng;
    FIT_FLOAT32 distance;
    FIT_FLOAT32 altitude;
    FIT_FLOAT32 speed;
    FIT_SINT8 temperature;
};

struct SessionData {
    FIT_DATE_TIME startTime;
    FIT_SINT32 necLat;
    FIT_SINT32 necLng;
    FIT_SINT32 swcLat;
    FIT_SINT32 swcLng;
    FIT_FLOAT32 totalDistance;
    FIT_FLOAT32 totalTime;
};

class FitListener : public fit::SessionMesgListener,
                    public fit::RecordMesgListener,
                    public fit::LapMesgListener,
                    public fit::DeveloperFieldDescriptionListener {
public:
    int numRecs = 0;
    LocationData records[100000];
    bool foundSession = false;
    SessionData session = {};

    FIT_SINT32 necLat = std::numeric_limits<int32_t>::min();
    FIT_SINT32 necLng = std::numeric_limits<int32_t>::min();
    FIT_SINT32 swcLat = std::numeric_limits<int32_t>::max();
    FIT_SINT32 swcLng = std::numeric_limits<int32_t>::max();

    void OnMesg(fit::RecordMesg& record) override {
        // fmt::print( "Record:\n" );
        if (!record.IsPositionLatValid() || !record.IsPositionLongValid()) {
            return;
        }

        LocationData& rec = records[numRecs];
        rec.timestamp = record.GetTimestamp();
        rec.lat = record.GetPositionLat();
        rec.lng = record.GetPositionLong();
        rec.distance = record.GetDistance();
        rec.temperature = record.GetTemperature();

        if (record.IsAltitudeValid()) {
            rec.altitude = record.GetAltitude();
        } else if (record.IsEnhancedAltitudeValid()) {
            rec.altitude = record.GetEnhancedAltitude();
        } else {
            rec.altitude = 0;
        }

        if (record.IsSpeedValid()) {
            rec.speed = record.GetSpeed();
        } else if (record.IsEnhancedSpeedValid()) {
            rec.speed = record.GetEnhancedSpeed();
        } else {
            rec.speed = 0;
        }

        if (rec.lat < swcLat) {
            swcLat = rec.lat;
        }
        if (rec.lng < swcLng) {
            swcLng = rec.lng;
        }
        if (rec.lat > necLat) {
            necLat = rec.lat;
        }
        if (rec.lng > necLng) {
            necLng = rec.lng;
        }

        numRecs++;
    }

    void OnMesg(fit::LapMesg& record) override {
        if (foundSession) {
            return;
        }
        //some fits don't have session, so also grab lap data

        fmt::print("Records: {}\n", numRecs);

        session.startTime = record.GetStartTime();
        session.totalDistance = record.GetTotalDistance();
        session.totalTime = record.GetTotalElapsedTime();

        fmt::print("Lap:\n");
        fmt::print("	total distance: {}\n", record.GetTotalDistance());
        fmt::print("	start time: {}\n", record.GetStartTime());
        fmt::print("	total elapsed time: {}\n", record.GetTotalElapsedTime());
        fmt::print("	total timer time: {}\n", record.GetTotalTimerTime());
    }

    void OnMesg(fit::SessionMesg& record) override {
        fmt::print("Records: {}\n", numRecs);

        session.startTime = record.GetStartTime();
        session.totalDistance = record.GetTotalDistance();
        session.totalTime = record.GetTotalElapsedTime();
        session.necLat = record.GetNecLat();
        session.necLng = record.GetNecLong();
        session.swcLat = record.GetSwcLat();
        session.swcLng = record.GetSwcLong();
        foundSession = true;

        fmt::print("Session:\n");
        fmt::print("	total distance: {}\n", record.GetTotalDistance());
        fmt::print("	total elapsed time: {}\n", record.GetTotalElapsedTime());
        fmt::print("	total timer time: {}\n", record.GetTotalTimerTime());
        fmt::print("	avg speed: {}\n", record.GetAvgSpeed());
        fmt::print("	max speed: {}\n", record.GetMaxSpeed());
        fmt::print("	ascent: {}\n", record.GetTotalAscent());
        fmt::print("	decent: {}\n", record.GetTotalDescent());

        fmt::print("	start time: {}\n", record.GetStartTime());
        fmt::print("	end time: {}\n", record.GetTimestamp());
        //get these from first and last record?
        // fmt::print( "	start lat: {}\n", record.GetStartPositionLat());
        // fmt::print( "	start lng: {}\n", record.GetStartPositionLong());
        // fmt::print( "	end lat: {}\n", record.GetEndPositionLat());
        // fmt::print( "	end lng: {}\n", record.GetEndPositionLong());
        fmt::print("	nec lat: {}\n", record.GetNecLat());
        fmt::print("	nec lng: {}\n", record.GetNecLong());
        fmt::print("	swc lat: {}\n", record.GetSwcLat());
        fmt::print("	swc lng: {}\n", record.GetSwcLong());
    }

    void OnDeveloperFieldDescription(const fit::DeveloperFieldDescription& desc) override {}
};

static int dbCallback(void* _, int argc, char** argv, char** azColName) {
    int i;
    for (i = 0; i < argc; i++) {
        fmt::print("{} = {}\n", azColName[i], (argv[i] ? argv[i] : "NULL"));
    }
    return 0;
}

static void importFitFile(sqlite3* db, const std::string& filePath, const long long garminActivity) {
    fit::Decode decode;
    fit::MesgBroadcaster mesgBroadcaster;
    FitListener listener;
    std::fstream file;
    int rc;
    sqlite3_stmt* res;

    file.open(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        fmt::print(stderr, "Failed to open: {}\n", filePath);
        std::exit(1);
    }

    const std::string activityExistsParam = R"sql(
select 1 from activity where garminActivity=@garminActivity;
)sql";
    sqlite3_prepare_v2(db, activityExistsParam.c_str(), -1, &res, 0);
    sqlite3_bind_int64(res, sqlite3_bind_parameter_index(res, "@garminActivity"), garminActivity);
    rc = sqlite3_step(res);
    sqlite3_reset(res);
    sqlite3_finalize(res);
    if (rc == SQLITE_ROW) {
        fmt::print("Skipping {} (exists in db)\n", garminActivity);
        return;
    }

    mesgBroadcaster.AddListener((fit::RecordMesgListener&)listener);
    mesgBroadcaster.AddListener((fit::SessionMesgListener&)listener);
    mesgBroadcaster.AddListener((fit::LapMesgListener&)listener);

    fmt::print("Importing {}\n", filePath);

    try {
        decode.Read(&file, &mesgBroadcaster, &mesgBroadcaster, &listener);
    } catch (const fit::RuntimeException& e) {
        fmt::print(stderr, "Exception decoding file: {}\n", filePath);
        fmt::print(stderr, "{}\n", e.what());
        std::exit(1);
    } catch (...) {
        fmt::print(stderr, "Exception decoding file: {}\n", filePath);
        std::exit(1);
    }

    if (listener.numRecs == 0) {
        fmt::print("Skipping due to no GPS data\n");
        return;
    }

    char* zErrMsg = 0;
    std::string query = fmt::format(
        R"sql(
insert into activity (necLat, necLng, swcLat, swcLng, garminActivity, startTime, totalTime, totalDistance)
values ({}, {}, {}, {}, {}, {}, {}, {});
)sql",
        listener.necLat,
        listener.necLng,
        listener.swcLat,
        listener.swcLng,
        garminActivity,
        listener.session.startTime,
        listener.session.totalTime,
        listener.session.totalDistance
    );
    rc = sqlite3_exec(db, query.c_str(), dbCallback, 0, &zErrMsg);
    fmt::print("query: {}\n", query);
    if (rc != SQLITE_OK) {
        fmt::print(stderr, "query error: {}\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        std::exit(1);
    }
    sqlite3_int64 activityId = sqlite3_last_insert_rowid(db);

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &zErrMsg);

    const std::string insertLocationParam = R"sql(
insert into location (minLat, maxLat, minLng, maxLng, activityId, altitude, speed)
values (@minLat, @maxLat, @minLng, @maxLng, @activityId, @altitude, @speed);
)sql";

    rc = sqlite3_prepare_v2(db, insertLocationParam.c_str(), -1, &res, 0);
    if (rc != SQLITE_OK) {
        fmt::print(stderr, "error: {}\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        std::exit(1);
    }

    for (int i = 0; i < listener.numRecs; i++) {
        const LocationData& loc = listener.records[i];

        sqlite3_bind_int(res, sqlite3_bind_parameter_index(res, "@minLat"), loc.lat);
        sqlite3_bind_int(res, sqlite3_bind_parameter_index(res, "@maxLat"), loc.lat + 1);
        sqlite3_bind_int(res, sqlite3_bind_parameter_index(res, "@minLng"), loc.lng);
        sqlite3_bind_int(res, sqlite3_bind_parameter_index(res, "@maxLng"), loc.lng + 1);
        sqlite3_bind_int64(res, sqlite3_bind_parameter_index(res, "@activityId"), activityId);
        sqlite3_bind_double(res, sqlite3_bind_parameter_index(res, "@altitude"), loc.altitude);
        sqlite3_bind_double(res, sqlite3_bind_parameter_index(res, "@speed"), loc.speed);

        int step = sqlite3_step(res);
        sqlite3_reset(res);
    }
    sqlite3_finalize(res);

    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, &zErrMsg);
}

static void importFitFolder(sqlite3* db, const std::string& importFolder) {
    char* zErrMsg = 0;

    fmt::print("Starting import of {}\n", importFolder);

    const std::string createTableActivity = R"sql(
create virtual table if not exists
activity using rtree_i32(
	id,
	swcLat, necLat,
	swcLng, necLng,
	+garminActivity number,
	+startTime number,
	+totalTime number,
	+totalDistance number
);
)sql";

    int rc = sqlite3_exec(db, createTableActivity.c_str(), dbCallback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fmt::print(stderr, "query error: {}\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        std::exit(1);
    }

    const std::string createTableLocation = R"sql(
create virtual table if not exists
location using rtree_i32(
	id,
	minLat, maxLat,
	minLng, maxLng,
	+activityId int,
	+altitude number,
	+speed number,
);
)sql";
    //	foreign key (activityId) references activity(id)

    rc = sqlite3_exec(db, createTableLocation.c_str(), dbCallback, 0, &zErrMsg);
    if (rc) {
        fmt::print(stderr, "query error: {}\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        std::exit(1);
    }

    const std::regex garminActivityRegex(".*_([0-9]+)[^0-9]*\\.fit");
    std::smatch match;
    for (auto const& dirEntry : fs::directory_iterator{importFolder}) {
        if (dirEntry.is_regular_file()) {
            std::string fileName = dirEntry.path().filename();
            if (std::regex_match(fileName, match, garminActivityRegex)) {
                if (match.size() == 2) {
                    std::ssub_match base_sub_match = match[1];
                    std::string garminActivity = base_sub_match.str();
                    fmt::print("Found: [{}]\n", garminActivity);
                    importFitFile(db, dirEntry.path(), std::stoll(garminActivity));
                }
            } else {
                fmt::print("No garmin activity in filename: {}\n", fileName);
            }
        } else {
            fmt::print("Skipping: {}\n", dirEntry.path().c_str());
        }
    }
    sqlite3_close(db);
}

static void queryForActivities(
    sqlite3* db,
    const std::vector<double>& nec,
    const std::vector<double>& swc,
    int limit,
    const std::string fromTime,
    const std::string toTime
) {
    std::string fromTimeFit;
    if (fromTime.size()) {
        std::tm t = {};
        std::istringstream ss(fromTime);
        ss >> std::get_time(&t, "%Y-%m-{}");
        std::ostringstream tt;
        tt << " and startTime>=" << (std::mktime(&t) - FIT_EPOCH_DELTA);
        fromTimeFit = tt.str();
    }

    std::string toTimeFit;
    if (toTime.size()) {
        std::tm t = {};
        std::istringstream ss(toTime);
        ss >> std::get_time(&t, "%Y-%m-{}");
        std::ostringstream tt;
        tt << " and startTime<=" << (std::mktime(&t) - FIT_EPOCH_DELTA);
        toTimeFit = tt.str();
    }

    char* zErrMsg = 0;
    std::string query = fmt::format(
        R"sql(
select distinct(a.garminActivity) from activity a, location l on a.id=l.activityId
where minlat>{} and maxLat<{} and minLng>{} and maxLng<{}{}{}
order by a.garminActivity desc
limit {};
)sql",
        swc[0] * DEG_TO_SEMICIRCLE,
        nec[0] * DEG_TO_SEMICIRCLE,
        swc[1] * DEG_TO_SEMICIRCLE,
        nec[1] * DEG_TO_SEMICIRCLE,
        fromTimeFit,
        toTimeFit,
        limit
    );
    fmt::print("query: {}\n", query);
    fmt::print("Results:\n");
    int rc = sqlite3_exec(
        db,
        query.c_str(),
        [](void* _, int argc, char** argv, char** azColName) -> int {
            fmt::print("https://connect.garmin.com/modern/activity/{}\n", argv[0]);
            return 0;
        },
        0,
        &zErrMsg
    );
    // int rc = sqlite3_exec(db, query.c_str(), dbCallback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fmt::print(stderr, "query error: {}\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        std::exit(1);
    }
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser cli(argv[0], VERSION_STR);

    std::string importFolder("fit_data");
    cli.add_argument("-i", "--import")
        .help("Folder that contains fit files")
        .default_value(importFolder)
        .store_into(importFolder);

    std::string dbFileName("fitdb.sqlite");
    cli.add_argument("-d", "--database")
        .help("sqlite database file to use")
        .default_value(dbFileName)
        .store_into(dbFileName);

    std::vector<double> nec{0.0, 0.0};
    cli.add_argument("--nec").help("north east corner").nargs(2).scan<'g', double>();

    std::vector<double> swc{0.0, 0.0};
    cli.add_argument("--swc").help("south west corner").nargs(2).scan<'g', double>();

    std::string fromTime;
    cli.add_argument("-f", "--fromTime")
        .help("include activities >= fromTime (yyyy-mm-dd)")
        .default_value(fromTime)
        .store_into(fromTime);

    std::string toTime;
    cli.add_argument("-t", "--toTime")
        .help("include activities <= toTime (yyyy-mm-dd)")
        .default_value(toTime)
        .store_into(toTime);

    int limit = 100;
    cli.add_argument("-l", "--limit").help("max number or records to return").default_value(limit).store_into(limit);

    try {
        cli.parse_args(argc, argv);

        sqlite3* db;
        char* zErrMsg = 0;

        if (cli.is_used("--nec") && cli.is_used("--swc")) {
            int rc = sqlite3_open_v2(dbFileName.c_str(), &db, SQLITE_OPEN_READONLY, 0);
            if (rc) {
                fmt::print(stderr, "Failed to open db: {}\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                std::exit(1);
            }

            nec = cli.get<std::vector<double>>("--nec");
            swc = cli.get<std::vector<double>>("--swc");

            queryForActivities(db, nec, swc, limit, fromTime, toTime);
        }
        if (cli.is_used("-i")) {
            if (!fs::exists(importFolder) || fs::status(importFolder).type() != fs::file_type::directory) {
                fmt::print(stderr, "Folder not found: {}\n", importFolder);
                std::exit(1);
            }

            int rc = sqlite3_open(dbFileName.c_str(), &db);
            if (rc) {
                fmt::print(stderr, "Failed to open db: {}\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                std::exit(1);
            }

            importFitFolder(db, importFolder);
        }

    } catch (const std::exception& err) {
        fmt::print(stderr, "{}\n", err.what());
        fmt::print(stderr, "{}\n", cli.help().str());
        std::exit(1);
    }

    return 0;
}
