# psAle - If Ale's board ever needed an app, then this would be IT!
# Copyright (c) 2012 by Victor ADĂSCĂLIȚEI at admin@tuscale.ro
# 
# This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License. 
# To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/3.0/ , read it from the 
# COPYING file that resides in the same directory as this file does or send a letter to 
# Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

include ../Makefile.defs

OBIECTE_PSALE := psale_db.o psale_dl.o
PSALE_CALE_SURSE := ../principal

CFLAGS := -g -Wall -I$(PSALE_CALE_SURSE) `pkg-config --cflags gtk+-2.0 sqlite3 libzip`
LDFLAGS := `pkg-config --libs gtk+-2.0 sqlite3 libcurl libzip`
OBJS = $(OBIECTE_PSALE)

TIPCONSTRUCTIE :=

.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Construiesc obiectul '$@' ..."

.PHONY : all
all: $(OBJS)
	@$(CC) $(TIPCONSTRUCTIE) $(CFLAGS) $(OBJS) main.c -o $(REIN_BIN_NAME) $(LDFLAGS)
	@echo "Asamblez aplicatia reinoitor ..."
	
win : TIPCONSTRUCTIE := -mwindows 
win: all

$(OBIECTE_PSALE): psale_%.o: $(PSALE_CALE_SURSE)/%.c
	@$(CC) $(CFLAGS) -I$(PSALE_CALE_SURSE) -c $< -o $@
	@echo "Construiesc obiectul '$@' ..."

clean :
	@rm -f $(REIN_BIN_NAME) $(OBJS) *~
	@echo "Curat directorul curent ..."
