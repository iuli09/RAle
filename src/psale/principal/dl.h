/*
 * psAle - If Ale's board ever needed an app, then this would be IT!
 * Copyright (c) 2012 by Victor ADĂSCĂLIȚEI at admin@tuscale.ro
 * 
 * This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License. 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/3.0/ , read it from the 
 * COPYING file that resides in the same directory as this file does or send a letter to 
 * Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
 */

#ifndef _DL_H_
#define _DL_H_

#include <gtk/gtk.h>

#define DL_TEMP_ARHIVA     "temp.zip"
#define DL_CURL_TIMEOUT    5

typedef struct {
  char *adrPachetNou;
  double vers;
  char *mesajModificari;
} IntrareActualizare;

extern gboolean 
dl_initializeaza(const char *adresa);

extern gboolean 
dl_exista_versiune_mai_buna_decat(double versCurentaLocal);

extern void
dl_curata();

extern const IntrareActualizare *
dl_obtine_ultima_intrare_actualizare();

extern void
dl_seteaza_ultima_intrare_actualizare(IntrareActualizare *iac);

extern double
dl_obtine_vers_curenta_server();

extern gboolean 
dl_actualizeaza_aplicatia();

#endif
