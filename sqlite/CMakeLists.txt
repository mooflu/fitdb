project(SQLITE3)

include_directories(${CMAKE_PREFIX_PATH}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB SQLITE3_SRC sqlite3.c)
file(GLOB SQLITE3_HEADERS *.h)

add_definitions(-DSQLITE_ENABLE_RTREE)

add_library(sqlite3 ${SQLITE3_SRC} ${SQLITE3_HEADERS})

install(FILES ${SQLITE3_HEADERS} DESTINATION include/sqlite3)
install(TARGETS sqlite3 ARCHIVE DESTINATION lib)
