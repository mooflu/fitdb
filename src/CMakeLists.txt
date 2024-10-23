include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/fit_sdk)
include_directories(${PROJECT_SOURCE_DIR}/sqlite)

file(GLOB FITDB_SRC main.cpp)
file(GLOB FITDB_HEADERS *.hpp)

add_executable(fitdb WIN32 MACOSX_BUNDLE ${FITDB_SRC} ${FITDB_HEADERS})

target_link_libraries(fitdb fit sqlite3
${CMAKE_DL_LIBS}
)

