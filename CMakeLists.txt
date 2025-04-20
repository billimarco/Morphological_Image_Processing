cmake_minimum_required(VERSION 3.10)

# Nome del progetto
project(Morphological_Image_Processing VERSION 0.1.0 LANGUAGES C CXX)

# Imposta i flag di compilazione per Debug e Release
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(include include/nlohmann include/stb)

# Source files
set(SOURCES src/main.cpp)

# Aggiungi l'eseguibile
add_executable(${PROJECT_NAME} ${SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
set(CMAKE_EXE_LINKER_FLAGS "-static")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")  # Debug symbols
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")  # Ottimizzazione
endif()

file(COPY ${CMAKE_SOURCE_DIR}/settings DESTINATION ${CMAKE_BINARY_DIR})

# Stampa il tipo di build (Debug o Release)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

