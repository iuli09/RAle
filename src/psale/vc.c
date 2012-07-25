#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <cairo.h>
#include <gtk/gtkadjustment.h>

#include "db.h"
#include "vc.h"

static void
reseteazaIndicatorSoricel(VizualizatorCartulie *vc) {
  gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(vc->frm)), gdk_cursor_new(GDK_ARROW));
}

static void 
vc_destroy_frm(GtkWidget *window, gpointer data) {
  gtk_widget_hide(window);
}

static gboolean 
vc_delete_event_frm(GtkWidget *window, GdkEvent *event, gpointer data) {
  gtk_widget_hide(window);
  return TRUE;
}

static gboolean 
pdfviz_mouse_click_event(GtkWidget *widget, GdkEventButton *e, VizualizatorCartulie *vc) {
  switch(e->button) {
  case 1: /* butonul din stânga a șoricelului a fost apăsat */
    if(NULL != vc->actiune_salt_proxim) {
      switch(vc->actiune_salt_proxim->type) {
      case POPPLER_ACTION_GOTO_DEST:
        vc_sari_la_pagina(vc, vc->actiune_salt_proxim->goto_dest.dest->page_num - 1);
        reseteazaIndicatorSoricel(vc);
        break;
      default:
        break;
      }
      
      vc->actiune_salt_proxim = NULL;
    }
    break;
  }
  
  return TRUE;
}

static gboolean 
pdfviz_mouse_misc_event(GtkWidget *widget, GdkEventMotion *e, VizualizatorCartulie *vc) {
  GList *l = NULL;
  gboolean actiuneGasita = FALSE;
  PopplerLinkMapping *lmapping = NULL;
  PopplerAction *action = NULL;
  gint x1Rendat, y1Rendat;
  gint x2Rendat, y2Rendat;
  
  if(NULL != vc && NULL != vc->salturi_curente) {
    for (l = vc->salturi_curente; l && actiuneGasita == FALSE; l = g_list_next(l)) {
      lmapping = (PopplerLinkMapping *)l->data;
      action = poppler_action_copy(lmapping->action);
      x1Rendat = vc->x_start_rendare + lmapping->area.x1 * vc->x_pag_scale;
      x2Rendat = vc->x_start_rendare + lmapping->area.x2 * vc->x_pag_scale;
      /* "pag_height - y[1,2]" este un artificiu tare ciudat, dar necesar dacă dorim ca coordonatele să fie bune */
      y1Rendat = vc->y_start_rendare + (vc->pag_height - lmapping->area.y1) * vc->y_pag_scale;
      y2Rendat = vc->y_start_rendare + (vc->pag_height - lmapping->area.y2) * vc->y_pag_scale;
      
      switch(action->type) {
      case POPPLER_ACTION_GOTO_DEST:
        if(e->x >= x1Rendat && e->y <= y1Rendat &&
          e->x <= x2Rendat && e->y >= y2Rendat &&
          vc->actiune_salt_proxim != action) {
          vc->actiune_salt_proxim = action;
          gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(vc->frm)), gdk_cursor_new(GDK_HAND2));
          actiuneGasita = TRUE;
        }
        break;
      default:
        break;
      }
    }
    
    if(actiuneGasita == FALSE && NULL != vc->actiune_salt_proxim) {
      /* regiunea prosimă a fost părăsită, resetăm cursorul șoricelului */
      vc->actiune_salt_proxim = NULL;
      reseteazaIndicatorSoricel(vc);
    }
  }
  
  return TRUE;
}

static gboolean 
pdfviz_vscrol_modif_event(GtkWidget *widget, GdkEventScroll  *e, VizualizatorCartulie *vc) {
  gdouble pozCurentaVScrol = gtk_adjustment_get_value(vc->pdfScrolBVertical);
  gdouble dimPagina = gtk_adjustment_get_page_size(vc->pdfScrolBVertical);
  gdouble inaltimePdfReala = vc->x_pag_scale * vc->pag_height;
  gdouble navIncrement = (inaltimePdfReala - dimPagina) / SCROLURI_PER_PAG;
  
  if((e->state & GDK_CONTROL_MASK) != 0) {
    /* "CTRL" a fost apasat deci se dorește o scalare a documentului */
    switch(e->direction) {
    case GDK_SCROLL_UP:
      vc->x_pag_scale += INCREMENT_MARIRE;
      vc->y_pag_scale += INCREMENT_MARIRE;
      break;
    case GDK_SCROLL_DOWN:
      vc->x_pag_scale -= INCREMENT_MARIRE;
      vc->y_pag_scale -= INCREMENT_MARIRE;
      break;
    default:
      break;
    }
    /* redesenează pagina */
    vc_sari_la_pagina(vc, vc->nr_pag_curenta);
  } else {
    switch(e->direction) {
    case GDK_SCROLL_UP:
      if(pozCurentaVScrol < 1e-7) {
        if(vc->nr_pag_curenta != 0) {
          vc_sari_la_pagina(vc, vc->nr_pag_curenta - 1);
          gtk_adjustment_set_value(vc->pdfScrolBVertical, inaltimePdfReala - dimPagina);
        }
      } else {
        gtk_adjustment_set_value(vc->pdfScrolBVertical, pozCurentaVScrol - navIncrement);
      }
      break;
    case GDK_SCROLL_DOWN:
      if(dimPagina + pozCurentaVScrol >= inaltimePdfReala) {
        vc_sari_la_pagina(vc, vc->nr_pag_curenta + 1);
        gtk_adjustment_set_value(vc->pdfScrolBVertical, 0);
      } else {
        gtk_adjustment_set_value(vc->pdfScrolBVertical, pozCurentaVScrol + navIncrement);
      }
      break;
    default:
      break;
    }
    gtk_widget_queue_draw(widget);
  }
  
  return TRUE;
}

static void 
pdfviz_expose_event(GtkWidget *widget, GdkEventExpose *event, VizualizatorCartulie *vc) {
  gint w, h;
  cairo_t *cr = NULL;
  
  gtk_window_get_size(GTK_WINDOW(vc->frm), &w, &h);
  gdk_window_clear(vc->pdfviz->window);
  cr = gdk_cairo_create(vc->pdfviz->window);

  vc->x_start_rendare = (w - vc->pag_width * vc->x_pag_scale) / 2;
  vc->y_start_rendare = (h - vc->pag_height * vc->y_pag_scale) / 2;
  
  if(vc->x_start_rendare < 0) vc->x_start_rendare = 0;
  if(vc->y_start_rendare < 0) vc->y_start_rendare = 0;
  
  cairo_set_source_surface(cr, vc->suprafataPag, vc->x_start_rendare, vc->y_start_rendare);
  cairo_paint(cr);
  cairo_destroy(cr);
}

static void 
btPrecPag_clicked(GtkToolItem *tooleditcut, VizualizatorCartulie *vc) {
  vc_sari_la_pagina(vc, vc->nr_pag_curenta - 1);
  gtk_adjustment_set_value(vc->pdfScrolBVertical, 0);
}

static void 
btUrmPag_clicked(GtkToolItem *tooleditcopy, VizualizatorCartulie *vc) {
  vc_sari_la_pagina(vc, vc->nr_pag_curenta + 1);
  gtk_adjustment_set_value(vc->pdfScrolBVertical, 0);
}

static void 
btCuprins_clicked(GtkToolItem *tooleditpaste, VizualizatorCartulie *vc) {
  vc_sari_la_pagina(vc, PAGINA_CUPRINS);
}

VizualizatorCartulie * 
vc_initializeaza() {
  VizualizatorCartulie *deRet = NULL;
  GtkWidget *frmVbox = NULL;
  GtkWidget *pdfHVbox = NULL;
  GtkWidget *cadruPdfViz = NULL;
  GtkWidget *btPrecPag = NULL;
  GtkWidget *btUrmPag = NULL;
  GtkWidget *btCuprins = NULL;
  
  /* inițializăm structura principală a vizualizatorului */
  deRet = g_new(VizualizatorCartulie, 1);
  deRet->doc = poppler_document_new_from_file(g_file_get_uri(g_file_new_for_commandline_arg("./cartulie_fis_test.pdf")), NULL, NULL);
  if(NULL == deRet->doc) {
    g_free(deRet);
    return NULL;
  }
  deRet->nr_pag_totale = poppler_document_get_n_pages(deRet->doc);
  
  /* inițializează formularul principal */
  deRet->frm = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(deRet->frm), "~electro~ cărțulia");
  gtk_window_set_default_size(GTK_WINDOW(deRet->frm), 800, 600);
  gtk_window_maximize(GTK_WINDOW(deRet->frm));
  
  frmVbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(deRet->frm), frmVbox);

  btPrecPag = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(btPrecPag), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click(GTK_BUTTON(btPrecPag), FALSE);
  GdkPixbuf *imgPagPrecPixBuf = db_obtine_imagine_media_scalata(DB_IMG_PAG_PREC, 28, 28);
  GtkWidget *imgPagPrec = gtk_image_new_from_pixbuf(imgPagPrecPixBuf);
  gtk_button_set_image(GTK_BUTTON(btPrecPag), imgPagPrec);
  
  btUrmPag = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(btUrmPag), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click(GTK_BUTTON(btUrmPag), FALSE);
  GdkPixbuf *imgPagUrmPixBuf = db_obtine_imagine_media_scalata(DB_IMG_PAG_URM, 28, 28);
  GtkWidget *imgPagUrm = gtk_image_new_from_pixbuf(imgPagUrmPixBuf);
  gtk_button_set_image(GTK_BUTTON(btUrmPag), imgPagUrm);
  
  btCuprins = gtk_button_new_with_label("C");

  pdfHVbox = gtk_hbox_new(FALSE, 0);
  cadruPdfViz = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cadruPdfViz),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  deRet->pdfScrolBVertical = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(cadruPdfViz));
  deRet->pdfviz = gtk_drawing_area_new();
  gtk_widget_add_events(deRet->pdfviz, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_box_pack_start(GTK_BOX(pdfHVbox), btPrecPag, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(pdfHVbox), deRet->pdfviz, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(pdfHVbox), btUrmPag, FALSE, FALSE, 0);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(cadruPdfViz), pdfHVbox);
  gtk_box_pack_start(GTK_BOX(frmVbox), cadruPdfViz, TRUE, TRUE, 0);

  /* atașăm evenimentele */
  g_signal_connect(G_OBJECT(deRet->frm), "destroy", G_CALLBACK(vc_destroy_frm), NULL);
  g_signal_connect(G_OBJECT(deRet->frm), "delete_event", G_CALLBACK(vc_delete_event_frm), NULL);
  g_signal_connect(G_OBJECT(btPrecPag), "clicked", G_CALLBACK(btPrecPag_clicked), (gpointer)deRet);
  g_signal_connect(G_OBJECT(btUrmPag), "clicked", G_CALLBACK(btUrmPag_clicked), (gpointer)deRet);
  g_signal_connect(G_OBJECT(btCuprins), "clicked", G_CALLBACK(btCuprins_clicked), (gpointer)deRet);
  g_signal_connect(G_OBJECT(deRet->pdfviz), "scroll-event", G_CALLBACK(pdfviz_vscrol_modif_event), (gpointer)deRet);
  g_signal_connect(G_OBJECT(deRet->pdfviz), "motion-notify-event", G_CALLBACK(pdfviz_mouse_misc_event), (gpointer)deRet);
  g_signal_connect(G_OBJECT(deRet->pdfviz), "button-press-event", G_CALLBACK(pdfviz_mouse_click_event), (gpointer)deRet);
  g_signal_connect(G_OBJECT(deRet->pdfviz), "expose_event", G_CALLBACK(pdfviz_expose_event), (gpointer)deRet);
  
  /* finalizăm inițializarea structurii principale */
  deRet->suprafataPag = NULL;
  deRet->salturi_curente = NULL;
  deRet->actiune_salt_proxim = NULL;
  deRet->x_pag_scale = deRet->y_pag_scale = 1.3;
  
  return deRet;
}

void 
vc_sari_la_pagina(VizualizatorCartulie *vc, int nrPag)  {
  PopplerPage *ppage = NULL;
  int w, h;
  cairo_t *cr = NULL;

  /* validăm parametrii */
  if(nrPag < 0 || nrPag > vc->nr_pag_totale - 1) return;
  else vc->nr_pag_curenta = nrPag;
  
  /* accesăm pagina dorită + dimensiunile ei */
  ppage = poppler_document_get_page(vc->doc, nrPag);
  poppler_page_get_size(ppage, &vc->pag_width, &vc->pag_height);
  w = (int)ceil(vc->pag_width) * vc->x_pag_scale;
  h = (int)ceil(vc->pag_height) * vc->y_pag_scale;
  
  /* navigăm la pagina dorită */
  if(NULL != vc->suprafataPag) cairo_surface_destroy(vc->suprafataPag);
  vc->suprafataPag = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  cr = cairo_create(vc->suprafataPag);
  cairo_scale(cr, vc->x_pag_scale, vc->y_pag_scale);
  poppler_page_render(ppage, cr);
  cairo_destroy(cr);
  gtk_widget_set_size_request(vc->pdfviz, w, h);
  gtk_widget_queue_draw(vc->pdfviz);
  
  /* obținem lista de salturi(linkuri) de pe pagina curentă */
  if(NULL != vc->salturi_curente) poppler_page_free_link_mapping(vc->salturi_curente);
  vc->salturi_curente = poppler_page_get_link_mapping(ppage);
}
