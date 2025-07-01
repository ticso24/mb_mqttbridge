cmake_minimum_required(VERSION 3.31)

project("mb_mqttbridge"
	VERSION 0.9
)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(bwct 0.22 REQUIRED)
if(${bwct_FOUND})
    #message(STATUS "Found BWCT: ${bwct_PREFIX}")
endif()

find_package(bwctmb 0.21 REQUIRED)
if(${bwctmb_FOUND})
    #message(STATUS "Found BWCTMB: ${bwct_PREFIX}")
endif()

# workarround until the bwct cmake library support works
include_directories(/usr/local/include)
#target_link_libraries(${BWCT_LIBRARIES})

add_executable(${PROJECT_NAME})

# workarround until the bwct cmake library support works
target_link_libraries(${PROJECT_NAME} /usr/local/lib/libbwct.a)
target_link_libraries(${PROJECT_NAME} /usr/local/lib/libbwctmb.a)
target_link_libraries(${PROJECT_NAME} -lpthread)
target_link_libraries(${PROJECT_NAME} -lcrypt)
target_link_directories(${PROJECT_NAME} PRIVATE /usr/local/lib)
target_link_libraries(${PROJECT_NAME} -lmosquitto)

target_sources(${PROJECT_NAME}
	PRIVATE
		src/main.cc
		src/mqtt.cc
		src/vendor_bwct.cc
		src/vendor_eastron.cc
		src/vendor_epever.cc
		src/vendor_mru.cc
		src/vendor_trucki.cc
		src/vendor_zgej.cc
)

include(GNUInstallDirs)

# install the target and create export-set
install(TARGETS ${PROJECT_NAME}
    EXPORT "${PROJECT_NAME}Targets"
    # these get default values from GNUInstallDirs, no need to set them
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # bin
    #LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # lib
    #ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} # lib
    # except for public headers, as we want them to be inside a library folder
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME} # include/SomeLibrary
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # include
)

