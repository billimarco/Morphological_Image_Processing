cmake_minimum_required(VERSION 3.10)

# Nome del progetto
project(Morphological_Image_Processing VERSION 0.1.0 LANGUAGES C CXX)

# Aggiungi l'eseguibile
add_executable(Morphological_Image_Processing main.cpp)

# Imposta i flag di compilazione per Debug e Release
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
set(CMAKE_EXE_LINKER_FLAGS "-static")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")  # Debug symbols
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")  # Ottimizzazione
endif()

# Stampa il tipo di build (Debug o Release)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

