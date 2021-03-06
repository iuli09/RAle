/*
 * acad - An avrdude distro adapted for the Ale packet
 * Copyright (C) 2012 Victor ADĂSCĂLIȚEI <admin@tuscale.ro>
 *
 * !!! Original avrdude 5.11 main.c notice follows !!!
 * Copyright (C) 2000-2005  Brian S. Dean <bsd@bsdhome.com>
 * Copyright 2007-2009 Joerg Wunsch <j@uriah.heep.sax.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* $Id: main.c 988 2011-08-26 20:30:26Z joerg_wunsch $ */

#include "ac_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "avr.h"
#include "config.h"
#include "confwin.h"
#include "fileio.h"
#include "lists.h"
#include "pindefs.h"
#include "safemode.h"
#include "update.h"
#include "usbtiny.h"

/* Set 'main' return value possibilities */
enum {
  ACAD_OP_OK = 0,               /* The operation was carried out succesfully */
  ACAD_MEMORY_FAILED_ERR,	/* A memory op failed */
  ACAD_CONFIG_FILE_PARSE_ERR,	/* There was an error in parsing the ".conf" file */
  ACAD_COULDNT_OPEN_PGM_ERR,	/* Couldn't make link with 'usbtiny'. Is it connected ? */
  ACAD_PGM_INIT_ERR,		/* Something went wrong with the initialization of the programmer. Double check connections and try again ... */
  ACAD_SIG_READ_ERR,            /* There was an error in reading the MCU's signature stored value */
  ACAD_SIG_BAD_VAL_ERR,		/* An error while reading the signature value occured. The resulting signature has been corupted. */
  ACAD_SIG_MISSMATCH_ERR,	/* The expected signature doesn't match the real one */
  ACAD_UPDATE_ACT_ERR,		/* There was an error in the ubpdate processing stage */
  ACAD_ERASE_ACT_ERR,		/* Signifies the fact that the erase procedure had a problem and didn't ended succesfully */
  ACAD_CYCLCNT_UPD_ERR,		/* There was an error in updating the EEPROM programming cycle count value */
  ACAD_MCU_NOT_FOUND_ERR	/* The requested MCU was not found */
};

/* Get VERSION from ac_cfg.h */
const char *version = PACKAGE_VERSION;
const char *progname = PACKAGE;
char   progbuf[PATH_MAX]; /* temporary buffer of spaces the same
                             length as progname; used for lining up
                             multiline messages */

struct list_walk_cookie {
    FILE *f;
    const char *prefix;
};
static LISTID updates;
static PROGRAMMER * pgm;

/*
 * global options
 */
int    do_cycles;   /* track erase-rewrite cycles */
int    verbose;     /* verbose output */
int    quell_progress; /* un-verebose output */
int    ovsigck;     /* 1=override sig check, 0=don't */

char    sys_config[PATH_MAX]; /* system wide config file */
struct avrpart * p;

/*
 * usage message
 */
static void usage(void)
{
  fprintf(stderr,
 "Usage: %s [options]\n"
 "Options:\n"
 "  -p <partno>                Required. Specify AVR device.\n"
 "  -B <bitclock>              Specify JTAG/STK500v2 bit clock period (us).\n"
 "  -D                         Disable auto erase for flash memory\n"
 "  -P <port>                  Specify connection port.\n"
 "  -F                         Override invalid signature check.\n"
 "  -e                         Perform a chip erase.\n"
 "  -U <memtype>:r|w|v:<filename>[:format]\n"
 "                             Memory operation specification.\n"
 "                             Multiple -U options are allowed, each request\n"
 "                             is performed in the order specified.\n"
 "  -n                         Do not write anything to the device.\n"
 "  -V                         Do not verify.\n"
 "  -u                         Disable safemode, default when running from a script.\n"
 "  -s                         Silent safemode operation, will not ask you if\n"
 "                             fuses should be changed back.\n"
 "  -y                         Count # erase cycles in EEPROM.\n"
 "  -Y <number>                Initialize erase cycle # in EEPROM.\n"
 "  -q                         Quell progress output. -q -q for less.\n"
 "  -?                         Display this usage.\n"
 "\nacad version %s, URL: <http://savannah.nongnu.org/projects/avrdude/>\n"
          ,progname, version);
}

static void list_avrparts_callback(const char *name, const char *desc,
                                   const char *cfgname, int cfglineno,
                                   void *cookie) {
  struct list_walk_cookie *c = (struct list_walk_cookie *)cookie;

  fprintf(c->f, "%s%-4s = %-15s [%s:%d]\n",
          c->prefix, name, desc, cfgname, cfglineno);
}

static void list_parts(FILE * f, const char *prefix, LISTID avrparts) {
    struct list_walk_cookie c;

    c.f = f;
    c.prefix = prefix;

    walk_avrparts(avrparts, list_avrparts_callback, &c);
}

static void exithook(void) {
  if (pgm->teardown)
    pgm->teardown(pgm);
}

extern int 
acad_init() {
  int rc = -1;
  int i = 0;

  init_config();

  updates = lcreat(NULL, 0);
  if (updates == NULL) {
    fprintf(stderr, "%s: cannot initialize updater list\n", progname);
    return ACAD_MEMORY_FAILED_ERR;
  }

#if defined(WIN32NATIVE)
  win_sys_config_set(sys_config);
#else
  /* Setting the config file variable */
  strcpy(sys_config, "./");
  i = strlen(sys_config);
  if (i && (sys_config[i-1] != '/'))
    strcat(sys_config, "/");
  strcat(sys_config, "acad.conf");
#endif

  rc = read_config(sys_config);
  if (rc) {
    return ACAD_CONFIG_FILE_PARSE_ERR;
  }

  pgm = usbtiny_initpgm(); 
  if (pgm->setup) {
    pgm->setup(pgm);
  }
  if (pgm->teardown) {
    atexit(exithook);
  }

  return ACAD_OP_OK;
}

extern int 
acad_erase_chip(void) {
  int rc = -1;

  rc = avr_chip_erase(pgm, p);
  if(rc) {
    return ACAD_ERASE_ACT_ERR;
  }

  return ACAD_OP_OK;
}

extern void 
acad_set_port(const char *portName) {
  
}

extern int 
acad_load_mcu(const char *partName) {
  p = locate_part(part_list, partdesc);
  if (p == NULL) {
    /*fprintf(stderr,
            "%s: AVR Part \"%s\" not found.\n\n",
            progname, partdesc);
    fprintf(stderr,"Valid parts are:\n");
    list_parts(stderr, "  ", part_list);
    fprintf(stderr, "\n");*/
    return ACAD_MCU_NOT_FOUND_ERR;
  }

  if(p->flags & (AVRPART_HAS_PDI | AVRPART_HAS_TPI)) {
    safemode = 0;
  }

  return ACAD_OP_OK;
}

extern int 
acad_read_prgcycle(void) {
  int rc = -1;
  int cycles = 0;

  rc = avr_get_cycle_count(pgm, p, &cycles);
  if(rc)
    return -1;

  return cycles;
}

extern int 
acad_set_prgcycle(int newCycleCnt) {
  int rc = -1;

  rc = avr_put_cycle_count(pgm, p, newCycleCnt);
  if(rc)
    return ACAD_CYCLCNT_UPD_ERR;

  return ACAD_OP_OK;
}

extern int 
acad_push_memory_action(DEVICE_OP_TYPE op, char *memtype, FILEFMT filefmt, char *filename) {
  UPDATE *upd = NULL;

  upd = new_update(op, memtype, filefmt, filename);
  ladd(updates, upd);
  
  if (upd->op == DEVICE_WRITE) {
    upd = dup_update(upd);
    upd->op = DEVICE_VERIFY;
    ladd(updates, upd);
  }

  return ACAD_OP_OK;
}

/*
 * main routine
 */
int main(int argc, char * argv [])
{
  int              rc;          /* general return code checking */ 
  int              exitrc;      /* exit code for main() */
  int              i;           /* general loop counter */
  int              ch;          /* options flag */
  int              len;         /* length for various strings */
  AVRMEM         * sig;         /* signature data */
  UPDATE         * upd;
  LNODEID        * ln;


  /* options / operating mode variables */
  int     erase;       /* 1=erase chip, 0=don't */
  int     auto_erase;  /* 0=never erase unless explicity told to do
                          so, 1=erase if we are going to program flash */
  char  * port;        /* device port (/dev/xxx) */
  int     nowrite;     /* don't actually write anything to the chip */
  int     verify;      /* perform a verify operation */
  char  * partdesc;    /* part id */
  int     cycles;      /* erase-rewrite cycles */
  int     set_cycles;  /* value to set the erase-rewrite cycles to */
  char  * e;           /* for strtol() error checking */
  double  bitclock;    /* Specify programmer bit clock (JTAG ICE) */
  int     safemode;    /* Enable safemode, 1=safemode on, 0=normal */
  int     init_ok;     /* Device initialization worked well */
  int     is_open;     /* Device open succeeded */
  unsigned char safemode_lfuse = 0xff;
  unsigned char safemode_hfuse = 0xff;
  unsigned char safemode_efuse = 0xff;
  unsigned char safemode_fuse = 0xff;

  char * safemode_response;
  int fuses_specified = 0;
  int fuses_updated = 0;

  /*default_parallel[0] = 0;
  default_serial[0]   = 0;
  default_bitclock    = 0.0;*/

  rc = acad_init();
  if(ACAD_OP_OK != rc)
    exit(rc);

  default_parallel[0] = 0;
  default_serial[0]   = 0;
  default_bitclock    = 0.0;

  partdesc      = NULL;
  port          = default_parallel;
  erase         = 0;
  auto_erase    = 1;
  p             = NULL;
  ovsigck       = 0;
  nowrite       = 0;
  verify        = 1;        /* on by default */
  quell_progress = 0;
  verbose       = 0;
  do_cycles     = 0;
  set_cycles    = -1;
  bitclock      = 0.0;
  safemode      = 1;       /* Safemode on by default */
  is_open       = 0;

  /* set bitclock from configuration files unless changed by command line */
  if (default_bitclock > 0 && bitclock == 0.0) {
    bitclock = default_bitclock;
  }

  if (isatty(STDIN_FILENO) == 0)
      safemode  = 0;       /* Turn off safemode if this isn't a terminal */

  len = strlen(progname) + 2;
  for (i=0; i<len; i++)
    progbuf[i] = ' ';
  progbuf[i] = 0;

  /* Process command line arguments */
  while ((ch = getopt(argc,argv,"?B:C:De:F:np:P:qstU:uV:yY:")) != -1) {
    switch (ch) {
      case 'B':	/* clock sck period for usbtiny */
	bitclock = strtod(optarg, &e);
	if ((e == optarg) || (*e != 0) || bitclock == 0.0) {
	  fprintf(stderr, "%s: invalid bit clock period specified '%s'\n",
                  progname, optarg);
          exit(1);
        }
        break;

      case 'D': /* disable auto erase */
        auto_erase = 0;
        break;

      case 'e': /* perform a chip erase */
        erase = 1;
        break;

      case 'F': /* override invalid signature check */
        ovsigck = 1;
        break;

      case 'n':
        nowrite = 1;
        break;

      case 'p' : /* specify AVR part */
        partdesc = optarg;
        break;

      case 'P':
        port = optarg;
        break;

      case 'q' : /* Quell progress output */
        quell_progress++ ;
        break;

      case 's' : /* Silent safemode */
        safemode = 1;
        break;

      case 'u' : /* Disable safemode */
        safemode = 0;
        break;

      case 'U':
        upd = parse_op(optarg);
        if (upd == NULL) {
          fprintf(stderr, "%s: error parsing update operation '%s'\n",
                  progname, optarg);
          exit(1);
        }
        ladd(updates, upd);

        if (verify && upd->op == DEVICE_WRITE) {
          upd = dup_update(upd);
          upd->op = DEVICE_VERIFY;
          ladd(updates, upd);
        }
        break;

      case 'V':
        verify = 0;
        break;

      case 'y':
        do_cycles = 1;
        break;

      case 'Y':
        set_cycles = strtol(optarg, &e, 0);
        if ((e == optarg) || (*e != 0)) {
          fprintf(stderr, "%s: invalid cycle count '%s'\n",
                  progname, optarg);
          exit(1);
        }
        do_cycles = 1;
        break;

      case '?': /* help */
        usage();
        exit(0);
        break;

      default:
        fprintf(stderr, "%s: invalid option -%c\n\n", progname, ch);
        usage();
        exit(1);
        break;
    }
  }

  rc = acad_load_mcu("t25");
  if(ACAD_OP_OK != rc) {
    exit(rc);
  }
  /*
   * set up seperate instances of the avr part, one for use in
   * programming, one for use in verifying.  These are separate
   * because they need separate flash and eeprom buffer space
   */
  p = avr_dup_part(p);

  /*
   * open the programmer
   */
  rc = pgm->open(pgm, port);
  if (rc < 0) {
    exitrc = ACAD_COULDNT_OPEN_PGM_ERR;
    pgm->ppidata = 0; /* clear all bits at exit */
    goto main_exit;
  }

  is_open = 1;
  exitrc = 0;

  /*
   * enable the programmer : isn't required by usbtiny (check usbtiny.c)
   */
  //pgm->enable(pgm);

  /*
   * initialize the chip in preperation for accepting commands
   */
  init_ok = (rc = pgm->initialize(pgm, p)) >= 0;
  if (!init_ok) {
    exitrc = ACAD_PGM_INIT_ERR;
    goto main_exit;
  }

  /* indicate ready */
  /*pgm->rdy_led(pgm, ON);*/

  /*
   * Let's read the signature bytes to make sure there is at least a
   * chip on the other end that is responding correctly.  A check
   * against 0xffffff / 0x000000 should ensure that the signature bytes
   * are valid.
   */
  if(!ovsigck) {
    rc = avr_signature(pgm, p);
    if (rc != 0) {
      exitrc = ACAD_SIG_READ_ERR;
      goto main_exit;
    }
  }
  
  sig = avr_locate_mem(p, "signature");  
  if (sig != NULL) {
    int ff, zz;
    
    /* Check signature-read validity */
    ff = zz = 1;
    for (i=0; i<sig->size; i++) {
      if (sig->buf[i] != 0xff)
	ff = 0;
      if (sig->buf[i] != 0x00)
	zz = 0;
    }
    
    if (ff || zz) {
      exitrc = ACAD_SIG_BAD_VAL_ERR;
      goto main_exit;
    }
    
    if ((sig->size != 3 ||
	 sig->buf[0] != p->signature[0] ||
	 sig->buf[1] != p->signature[1] ||
	 sig->buf[2] != p->signature[2]) &&
	!ovsigck) {
      exitrc = ACAD_SIG_MISSMATCH_ERR;
      goto main_exit;
    }
  }

  if (safemode == 1) {
    /* If safemode is enabled, go ahead and read the current low, high,
       and extended fuse bytes as needed */
    rc = safemode_readfuses(&safemode_lfuse, &safemode_hfuse,
			    &safemode_efuse, &safemode_fuse, pgm, p, verbose);
    if (rc == 0) {
      /* Save the fuses as default */
      safemode_memfuses(1, &safemode_lfuse, &safemode_hfuse, &safemode_efuse, &safemode_fuse);
    }
  }

  /*if ((erase == 0) && (auto_erase == 1)) {
    AVRMEM * m;
    for (ln=lfirst(updates); ln; ln=lnext(ln)) {
      upd = ldata(ln);
      m = avr_locate_mem(p, upd->memtype);
      if (m == NULL)
        continue;
      if ((strcasecmp(m->desc, "flash") == 0) && (upd->op == DEVICE_WRITE)) {
        erase = 1;
        break;
      }
      }
      }*/

  /* Cycle count read/update logic */
  
  if (erase && 
      acad_erase_chip() != ACAD_OP_OK)
    goto main_exit;

  /* Perform the update actions demanded through the '-U' param list */
  for (ln=lfirst(updates); ln; ln=lnext(ln)) {
    upd = ldata(ln);
    rc = do_op(pgm, p, upd, nowrite, verify);
    if (rc) {
      exitrc = ACAD_UPDATE_ACT_ERR;
      break;
    }
  }

  /* Right before we exit programming mode, which will make the fuse
     bits active, check to make sure they are still correct */
  if (safemode == 1) {
    /* If safemode is enabled, go ahead and read the current low,
     * high, and extended fuse bytes as needed */
    unsigned char safemodeafter_lfuse = 0xff;
    unsigned char safemodeafter_hfuse = 0xff;
    unsigned char safemodeafter_efuse = 0xff;
    unsigned char safemodeafter_fuse  = 0xff;
    unsigned char failures = 0;
    char yes[1] = {'y'};

    /* Restore the default fuse values */
    safemode_memfuses(0, &safemode_lfuse, &safemode_hfuse, &safemode_efuse, &safemode_fuse);

    /* Try reading back fuses, make sure they are reliable to read back */
    if (safemode_readfuses(&safemodeafter_lfuse, &safemodeafter_hfuse,
                           &safemodeafter_efuse, &safemodeafter_fuse, pgm, p, verbose) != 0) {
      /* Uh-oh.. try once more to read back fuses */
      if (safemode_readfuses(&safemodeafter_lfuse, &safemodeafter_hfuse,
                             &safemodeafter_efuse, &safemodeafter_fuse, pgm, p, verbose) != 0) { 
        exitrc = ACAD_SIG_READ_ERR;
        goto main_exit;		  
      }
    }
    
    /* Now check what fuses are against what they should be */
    if (safemodeafter_fuse != safemode_fuse) {
      fuses_updated = 1;
      
      /* TODO: Ask the user - should we change them */
      safemode_response = yes;

      if (tolower((int)(safemode_response[0])) == 'y') {
	/* Enough chit-chat, time to program some fuses and check them */
	if (safemode_writefuse (safemode_fuse, "fuse", pgm, p,
				10, verbose) != 0) {
	  failures++;
	}
      }
    }
    
    /* Now check what fuses are against what they should be */
    if (safemodeafter_lfuse != safemode_lfuse) {
      fuses_updated = 1;
      
      /* TODO: Ask user - should we change them */
      safemode_response = yes;

      if (tolower((int)(safemode_response[0])) == 'y') {        
	/* Enough chit-chat, time to program some fuses and check them */
	if (safemode_writefuse (safemode_lfuse, "lfuse", pgm, p,
				10, verbose) != 0) {
	  failures++;
	}
      }
    }

    /* Now check what fuses are against what they should be */
    if (safemodeafter_hfuse != safemode_hfuse) {
      fuses_updated = 1;
              
      /* TODO: Ask user - should we change them */
      safemode_response = yes;
      if (tolower((int)(safemode_response[0])) == 'y') {
	/* Enough chit-chat, time to program some fuses and check them */
	if (safemode_writefuse(safemode_hfuse, "hfuse", pgm, p,
			       10, verbose) != 0) {
	  failures++;
	}
      }
    }
    
    /* Now check what fuses are against what they should be */
    if (safemodeafter_efuse != safemode_efuse) {
      fuses_updated = 1;
      
      /* TODO: Ask user - should we change them */
      safemode_response = yes;

      if (tolower((int)(safemode_response[0])) == 'y') {
	/* Enough chit-chat, time to program some fuses and check them */
	if (safemode_writefuse (safemode_efuse, "efuse", pgm, p,
				10, verbose) != 0) {
	  failures++;
	}
      }
    }
    
    /* TODO: check failures here and take apropriate actions */

    if (fuses_updated && fuses_specified) {
      exitrc = 1;
    }
  }

main_exit:

  /*
   * program complete
   */

  if (is_open) {
    pgm->powerdown(pgm);

    pgm->disable(pgm);

    pgm->rdy_led(pgm, OFF);
  }

  pgm->close(pgm);

  return exitrc;
}
