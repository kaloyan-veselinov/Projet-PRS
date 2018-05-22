cmake_minimum_required(VERSION 3.10)
project(Projet_PRS C)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(serveur1-PerformancesRadicalementSuperieures
        src/serveur1-PerformancesRadicalementSuperieures.c
        src/socket.c
        src/socket.h)

add_executable(serveur2-PerformancesRadicalementSuperieures
        src/serveur2-PerformancesRadicalementSuperieures.c
        src/socket.c
        src/socket.h)

add_executable(serveur3-PerformancesRadicalementSuperieures
        src/serveur3-PerformancesRadicalementSuperieures.c
        src/socket.c
        src/socket.h)
