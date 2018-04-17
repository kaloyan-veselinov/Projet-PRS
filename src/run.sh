#!/usr/bin/env bash

ipaddr=$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1 -d'/')
gnome-terminal -e "../bin/serveur1-PerformancesRadicalementSuperieures 8080"
../bin/client1 $ipaddr 8080 pic.png
