cmake_minimum_required(VERSION 3.10)
project(Projet_PRS C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(Projet_PRS
        src/serveur1-PerformancesRadicalementSuperieures.c
        src/serveur1-PerformancesRadicalementSuperieures.h
        src/socket.c
        src/socket.h)
