project(FIT)

include_directories(${CMAKE_PREFIX_PATH}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB FIT_SRC *.cpp)
file(GLOB FIT_HEADERS *.h)

add_library(fit ${FIT_SRC} ${FIT_HEADERS})

install(FILES ${FIT_HEADERS} DESTINATION include/fit)
install(TARGETS fit ARCHIVE DESTINATION lib)
