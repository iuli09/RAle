# psAle - If Ale's board ever needed an app, then this would be IT!
# Copyright (c) 2012 by Victor ADĂSCĂLIȚEI at admin@tuscale.ro
# 
# This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License. 
# To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/3.0/ , read it from the 
# COPYING file that resides in the same directory as this file does or send a letter to 
# Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.

include ../Makefile.defs

PSALE_CALE_ARHIVA      := ./CD/pachete/psAle.zip
COMPILATOR_CALE_ARHIVA := ./CD/pachete/compilator.zip

SUBDIRECTOARE_PSALE := ./limbaje/ ./stiluri/ ./media/ 
PSALE_ALTE_DEP      := ./*.dll ./psale_icoana_ap.ico ./$(DIR_SURSE_MELE)/
COMPILATOR_ALTE_DEP := 

all : FISIERE_PACHET_PSALE := 
all :
	@echo "TODO: Make-ul general nu este implementat pentru directorul 'construit'."

win : FISIERE_PACHET_PSALE := ./$(PRIN_BIN_NAME).exe ./$(REIN_BIN_NAME).exe \
      ./$(CART_PDF_NAME) $(SQLITEDB_NAME) ./$(NUME_CERT_PEM) \
      $(SUBDIRECTOARE_PSALE) $(PSALE_ALTE_DEP)
win : FISIERE_PACHET_COMPILATOR := ./winavr
win :
	#Documentatia 7za se poate gasi in 'wtooluri/7-zip.chm'
	@wtooluri/7za.exe u $(PSALE_CALE_ARHIVA) $(FISIERE_PACHET_PSALE)
	@wtooluri/7za.exe u $(COMPILATOR_CALE_ARHIVA) $(FISIERE_PACHET_COMPILATOR)
	# Documentatie MakeNSIS se gaseste in User Manual-ul NSIS-ului
	@makensis.exe //V1 $(NSIS_NUME_INST)
	
.PHONY : clean
clean :
	@echo "Curat directorul curent ..."
	@rm -f $(PSALE_CALE_ARHIVA) $(COMPILATOR_CALE_ARHIVA)
