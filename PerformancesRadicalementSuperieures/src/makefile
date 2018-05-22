NOMGRP = PerformancesRadicalementSuperieures
CFLAGS = -Wall -DDEBUG -g


all : serveur1 serveur2 serveur3
serveur1: ../bin/serveur1-$(NOMGRP)
serveur2: ../bin/serveur2-$(NOMGRP)
serveur3: ../bin/serveur3-$(NOMGRP)

../bin/serveur1-$(NOMGRP): serveur1-$(NOMGRP).o socket.o
	gcc $(CFLAGS) $^ -o $@ -lm -lpthread

../bin/serveur2-$(NOMGRP): serveur2-$(NOMGRP).o socket.o
	gcc $(CFLAGS) $^ -o $@ -lm -lpthread

../bin/serveur3-$(NOMGRP): serveur3-$(NOMGRP).o socket.o
	gcc $(CFLAGS) $^ -o $@ -lm -lpthread

socket.o: socket.c socket.h
	gcc $(CFLAGS) -c $< -o $@

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -rf ./*.o

mrproper: clean
	rm -rf ../bin/serveur1-$(NOMGRP) ../bin/serveur2-$(NOMGRP) ../bin/serveur3-$(NOMGRP) 

tar: clean
	cd .. && tar -czvf $(NOMGRP).tar.gz src/ bin/
