#include <gtk/gtk.h>

#include "db.h"
#include "finfo.h"
#include "fcod.h"

static GtkWidget *dlgInfo = NULL;

static gboolean 
imgLogo_click(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
#ifdef G_OS_WIN32
  ShellExecute(NULL, "open", "http://tuscale.ro", NULL, NULL, SW_SHOWNORMAL);
#elif defined G_OS_UNIX
    system("xdg-open 'http://tuscale.ro' &");
#endif

  return TRUE;
}

static void 
cmbxCodNou_selectat(GtkComboBox *widget, gpointer user_data) {
  /* interpretăm selecția doar dacă ea este una validă (se dorește un limbaj valid) */
  if(gtk_combo_box_get_active(widget) != 0) {
    GtkWidget *dlgCod = NULL;
    
    switch(gtk_combo_box_get_active(widget)) {
    case 1: /* ASM */
      dlgCod = initializeaza_formular_cod(ASM, "", FALSE);
      break;
    case 2: /* C */
      dlgCod = initializeaza_formular_cod(C, "", FALSE);
      break;
    }

    /* repoziționăm textul afișat */
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);

    gtk_widget_show_all(dlgCod);
  }
}

static void 
cmbxCodCarte_selectat(GtkComboBox *widget, gpointer user_data) {
  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
}

static void 
btInfo_click(GtkWidget *widget, gpointer data) {
  if(dlgInfo == NULL)
    dlgInfo = initializeaza_formular_info();

  gtk_widget_show_all(dlgInfo);
}

static gboolean 
frmPrincipal_delev(GtkWidget *widget, GdkEvent *event, gpointer data) {

  return FALSE;
}

static void 
frmPrincipal_destroy(GtkWidget *widget, gpointer data) {
  gtk_main_quit();
}

int main(int argc, char *argv[]) {
  GtkWidget *cadruFormPrincipal = NULL;
  GtkWidget *formPrincipal = NULL;
  GtkWidget *cadruImgLogo = NULL;
  GtkWidget *imgLogo = NULL;
  GtkWidget *cmbxCodNou = NULL;
  GtkWidget *cmbxCodCarte = NULL;
  GtkWidget *btInfo = NULL;
  GtkWidget *btIesire = NULL;

  gtk_init (&argc, &argv);
  dl_initializeaza(db_obtine_adresa_actualizare());

  dl_actualizeaza_bd();

  /* inițializăm formularul principal */
  formPrincipal = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width(GTK_CONTAINER(formPrincipal), 8);
  gtk_window_set_resizable(GTK_WINDOW(formPrincipal), FALSE);
  gtk_window_set_title(GTK_WINDOW(formPrincipal), "psAle");
  gtk_window_set_position(GTK_WINDOW(formPrincipal), GTK_WIN_POS_CENTER);
  g_signal_connect(formPrincipal, "delete-event", G_CALLBACK(frmPrincipal_delev), NULL);
  g_signal_connect(formPrincipal, "destroy", G_CALLBACK(frmPrincipal_destroy), NULL);

  /* inițializăm cadrul formularului principal */
  cadruFormPrincipal = gtk_vbox_new(FALSE, 5);
  gtk_container_add(GTK_CONTAINER(formPrincipal), cadruFormPrincipal);

  /* inițializăm logoul aplicației */
  imgLogo = gtk_image_new_from_file("media/tuscale_small_logo_ale.png");

  /* inițializăm cadrul pentru logoul aplicației */
  cadruImgLogo = gtk_event_box_new();
  gtk_widget_set_events(cadruImgLogo, GDK_KEY_PRESS_MASK);
  gtk_container_add(GTK_CONTAINER(cadruImgLogo), imgLogo);
  gtk_container_add(GTK_CONTAINER(cadruFormPrincipal), cadruImgLogo);
  g_signal_connect(cadruImgLogo, "button-press-event", G_CALLBACK(imgLogo_click), FALSE);

  /* inițializăm meniul de selecție pentru programare brută */
  cmbxCodNou = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmbxCodNou), "Hai la programare ...");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmbxCodNou), "... în ASM");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmbxCodNou), "... în C");
  gtk_combo_box_set_active(GTK_COMBO_BOX(cmbxCodNou), 0);
  g_signal_connect(cmbxCodNou, "changed", G_CALLBACK(cmbxCodNou_selectat), NULL);
  gtk_container_add(GTK_CONTAINER(cadruFormPrincipal), cmbxCodNou);

  /* inițializăm meniul de selecție pentru exemplele de cod din carte */
  cmbxCodCarte = gtk_combo_box_text_new();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cmbxCodCarte), "Cod din cărțulie ...");
  db_incarca_exemple_carte(GTK_COMBO_BOX_TEXT(cmbxCodCarte));
  gtk_combo_box_set_active(GTK_COMBO_BOX(cmbxCodCarte), 0);
  g_signal_connect(cmbxCodCarte, "changed", G_CALLBACK(cmbxCodCarte_selectat), NULL);
  gtk_container_add(GTK_CONTAINER(cadruFormPrincipal), cmbxCodCarte);

  /* inițializăm butonul de informații */
  btInfo = gtk_button_new_with_label("Ce avem aici?");
  g_signal_connect(btInfo, "clicked", G_CALLBACK(btInfo_click), NULL);
  gtk_container_add(GTK_CONTAINER(cadruFormPrincipal), btInfo);

  /* inițializăm butonul de ieșire */
  btIesire = gtk_button_new_with_label("Gata, am ieșit!");
  g_signal_connect_swapped(btIesire, "clicked", G_CALLBACK(gtk_widget_destroy), formPrincipal);
  gtk_container_add(GTK_CONTAINER(cadruFormPrincipal), btIesire);

  gtk_widget_show_all(formPrincipal);

  /* permite butoanelor să afișeze atât imaginea cât și textul dorit */
  GtkSettings *default_settings = gtk_settings_get_default();
  g_object_set(default_settings, "gtk-button-images", TRUE, NULL); 
  gtk_main();
  dl_curata();

  return 0;
}
