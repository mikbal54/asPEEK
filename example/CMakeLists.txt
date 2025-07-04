cmake_minimum_required(VERSION 2.6) 
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE) 
cmake_policy(SET CMP0003 NEW) 
 
project(asPEEK_Test) 
SET(asPEEK_Test_EXE_PATH "bin/")
SET(EXECUTABLE_OUTPUT_PATH ${asPEEK_Test_EXE_PATH})

SET(ANGELSCRIPT_SDK "C:/OyunLibs2012/Angelscript/trunk/sdk")
SET(asPEEK_SDK "C:/OyunLibs2012/asPEEK")
SET(BOOST_SDK "C:/OyunLibs2012/boost")

# set library paths!
link_directories(
	${ANGELSCRIPT_SDK}/angelscript/lib
	${asPEEK_SDK}/lib
	${BOOST_SDK}/lib
)

#set include paths!
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${ANGELSCRIPT_SDK}/add_on
	${ANGELSCRIPT_SDK}/angelscript/include # angelscript
	${BOOST_SDK} # boost
	${asPEEK_SDK}/include
	${asPEEK_SDK}/include/websocketpp
)
 
#find all cpp files, these are linked 
file(GLOB Source
    "src/*.cpp"
	${ANGELSCRIPT_SDK}/add_on/scriptarray/*.cpp
	${ANGELSCRIPT_SDK}/add_on/scriptstdstring/*.cpp
)

file(GLOB ADDONS
	"${ANGELSCRIPT}/add_on/scriptarray/*.cpp"
	"${ANGELSCRIPT}/add_on/scriptstdstring/*.cpp"
)

#message(${ANGELSCRIPT})
#message(${ADDONS})

file(GLOB Headers
    "include/*.h"
	"../include/*.h"
) 

add_executable(asPEEK_Test
${Source}
${ADDONS}
${Headers}
)


set (CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_SOURCE_DIR}/bin/${CMAKE_PLATFORM}/$(ConfigurationName))


if(MSVC)

set(LINK_LIBRARY 
optimized angelscript.lib
optimized asPEEK.lib
debug angelscriptd.lib
debug asPEEKd.lib
)

endif(MSVC)

target_link_libraries(asPEEK_Test ${LINK_LIBRARY})

#TODO, libraries for other platforms