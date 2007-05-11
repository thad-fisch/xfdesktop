/* $Id$ */
/*
 * Copyright (c) 2006 Jean-François Wauthy (pollux@xfce.org)
 * Copyright (c) 2006 Benedikt Meurer (benny@xfce.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>

#ifdef HAVE_LIBEXO
#include <exo/exo.h>
#endif

#include "menueditor-edit-dialog.h"

#define MENUEDITOR_EDIT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MENUEDITOR_TYPE_EDIT_DIALOG, MenuEditorEditDialogPrivate))

/* private struct */
typedef struct {
  GtkWidget *entry_name;
  GtkWidget *label_command;
  GtkWidget *entry_command;
  GtkWidget *hbox_command;
#ifdef HAVE_LIBEXO
  GtkWidget *button_icon;
#else
  GtkWidget *radio_button_themed_icon;
  GtkWidget *entry_themed_icon;
  GtkWidget *radio_button_icon;
  GtkWidget *chooser_icon;
#endif
  GtkWidget *check_button_snotify;
  GtkWidget *check_button_interm;
} MenuEditorEditDialogPrivate;

/* prototypes */
static void menueditor_edit_dialog_class_init (MenuEditorEditDialogClass *);
static void menueditor_edit_dialog_init (MenuEditorEditDialog *);

static void cb_browse_button_clicked (GtkButton *, MenuEditorEditDialog *);
#ifdef HAVE_LIBEXO
static void cb_icon_button_clicked (GtkWidget *, MenuEditorEditDialog *);
#else
static void cb_chooser_icon_update_preview (GtkFileChooser *, GtkImage *);
static void cb_radio_button_themed_icon_toggled (GtkToggleButton *, MenuEditorEditDialog *);
static void cb_radio_button_icon_toggled (GtkToggleButton *, MenuEditorEditDialog *);
#endif

/* globals */
static XfceTitledDialogClass *parent_class = NULL;

/******************************/
/* MenuEditorEditDialog class */
/******************************/
GtkType
menueditor_edit_dialog_get_type (void)
{
  static GtkType edit_dialog_type = 0;

  if (!edit_dialog_type) {
    static const GTypeInfo edit_dialog_info = {
      sizeof (MenuEditorEditDialogClass),
      NULL,
      NULL,
      (GClassInitFunc) menueditor_edit_dialog_class_init,
      NULL,
      NULL,
      sizeof (MenuEditorEditDialog),
      0,
      (GInstanceInitFunc) menueditor_edit_dialog_init
    };

    edit_dialog_type = g_type_register_static (XFCE_TYPE_TITLED_DIALOG, "MenuEditorEditDialog", &edit_dialog_info, 0);
  }

  return edit_dialog_type;
}

static void
menueditor_edit_dialog_class_init (MenuEditorEditDialogClass * klass)
{  
  g_type_class_add_private (klass, sizeof (MenuEditorEditDialogPrivate));

  parent_class = g_type_class_peek_parent (klass);
}

static void
menueditor_edit_dialog_init (MenuEditorEditDialog * dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);

#ifdef HAVE_LIBEXO
  GtkWidget *align;
#else
  GtkFileFilter *filter;
  GtkWidget *preview;
  GSList *radio_button_group = NULL;
#endif
  GtkWidget *table;
  GtkWidget *label;
  gchar *label_text = NULL;
  GtkWidget *button_browse;
  
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Edit menu entry"));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_EDIT);

  /* table */
  table = gtk_table_new (6, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table), 5);
  gtk_container_set_border_width (GTK_CONTAINER (table), 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);
  
  /* Name */
  label = gtk_label_new (_("Name:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Name:"));
  gtk_label_set_markup (GTK_LABEL (label), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  
  priv->entry_name = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (priv->entry_name), TRUE);
  gtk_widget_show (priv->entry_name);
  gtk_table_attach (GTK_TABLE (table), priv->entry_name, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 6);

  /* Command */
  priv->label_command = gtk_label_new (_("Command:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Command:"));
  gtk_label_set_markup (GTK_LABEL (priv->label_command), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (priv->label_command), 1.0f, 0.5f);
  gtk_widget_show (priv->label_command);
  gtk_table_attach (GTK_TABLE (table), priv->label_command, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);

  priv->hbox_command = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (priv->hbox_command);

  priv->entry_command = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (priv->entry_command), TRUE);
  gtk_widget_show (priv->entry_command);
  gtk_box_pack_start (GTK_BOX (priv->hbox_command), priv->entry_command, TRUE, TRUE, 0);
  
  button_browse = gtk_button_new_with_label ("...");
  g_signal_connect (G_OBJECT (button_browse), "clicked", G_CALLBACK (cb_browse_button_clicked), dialog);
  gtk_widget_show (button_browse);
  gtk_box_pack_start (GTK_BOX (priv->hbox_command), button_browse, FALSE, FALSE, 0);
  
  gtk_table_attach (GTK_TABLE (table), priv->hbox_command, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 6);
  
#ifdef HAVE_LIBEXO
  /* Icon chooser */
  label = gtk_label_new (_("Icon:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Icon:"));
  gtk_label_set_markup (GTK_LABEL (label), label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (label_text);

  align = gtk_alignment_new (0.0f, 0.5f, 0.0f, 0.0f);
  gtk_table_attach (GTK_TABLE (table), align, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (align);

  priv->button_icon = gtk_button_new_with_label (_("No icon"));
  gtk_container_add (GTK_CONTAINER (align), priv->button_icon);
  g_signal_connect (G_OBJECT (priv->button_icon), "clicked", G_CALLBACK (cb_icon_button_clicked), dialog);
  gtk_widget_show (priv->button_icon);
#else
  /* Themed icon */
  priv->radio_button_themed_icon = gtk_radio_button_new (NULL);
  radio_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (priv->radio_button_themed_icon));
  g_signal_connect (G_OBJECT (priv->radio_button_themed_icon), "toggled", G_CALLBACK (cb_radio_button_themed_icon_toggled), dialog);
  gtk_widget_show (priv->radio_button_themed_icon);
  gtk_table_attach (GTK_TABLE (table), priv->radio_button_themed_icon, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
  
  label = gtk_label_new (_("Themed icon:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Themed icon:"));
  gtk_label_set_markup (GTK_LABEL (label), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (priv->radio_button_themed_icon), label);
  
  priv->entry_themed_icon = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (priv->entry_themed_icon), TRUE);
  gtk_widget_show (priv->entry_themed_icon);
  gtk_table_attach (GTK_TABLE (table), priv->entry_themed_icon, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  
  /* Icon */
  priv->radio_button_icon = gtk_radio_button_new (radio_button_group);
  g_signal_connect (G_OBJECT (priv->radio_button_icon), "toggled", G_CALLBACK (cb_radio_button_icon_toggled), dialog);
  gtk_widget_show (priv->radio_button_icon);
  gtk_table_attach (GTK_TABLE (table), priv->radio_button_icon, 0, 1, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
  
  label = gtk_label_new (_("Icon:"));
  label_text = g_strdup_printf ("<span weight='bold'>%s</span>", _("Icon:"));
  gtk_label_set_markup (GTK_LABEL (label), label_text);
  g_free (label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0f, 0.5f);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (priv->radio_button_icon), label);
  
  priv->chooser_icon = gtk_file_chooser_button_new (_("Select icon"), GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_widget_set_sensitive (priv->chooser_icon, FALSE);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->chooser_icon), TRUE);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(priv->chooser_icon), filter);
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Image Files"));
  gtk_file_filter_add_pattern (filter, "*.png");
  gtk_file_filter_add_pattern (filter, "*.jpg");
  gtk_file_filter_add_pattern (filter, "*.bmp");
  gtk_file_filter_add_pattern (filter, "*.svg");
  gtk_file_filter_add_pattern (filter, "*.xpm");
  gtk_file_filter_add_pattern (filter, "*.gif");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->chooser_icon), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (priv->chooser_icon), filter);
  
  preview = gtk_image_new ();
  gtk_widget_show (preview);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (priv->chooser_icon), preview);
  gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER (priv->chooser_icon), TRUE);
  g_signal_connect (G_OBJECT (priv->chooser_icon), "update-preview", G_CALLBACK (cb_chooser_icon_update_preview), preview);
  
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->chooser_icon), DATADIR "/icons");
  gtk_widget_show (priv->chooser_icon);
  gtk_table_attach (GTK_TABLE (table), priv->chooser_icon, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 6);
#endif

  /* Start Notify check button */
  priv->check_button_snotify = gtk_check_button_new_with_mnemonic (_("Use startup _notification"));
  gtk_widget_show (priv->check_button_snotify);
  gtk_table_attach (GTK_TABLE (table), priv->check_button_snotify, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  /* Run in terminal check button */
  priv->check_button_interm = gtk_check_button_new_with_mnemonic (_("Run in _terminal"));
  gtk_widget_show (priv->check_button_interm);
  gtk_table_attach (GTK_TABLE (table), priv->check_button_interm, 1, 2, 6, 7, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

/*************/
/* internals */
/*************/
static void
cb_browse_button_clicked (GtkButton *button, MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  GtkWidget *chooser_dialog = NULL;
  GtkFileFilter *filter;
  const gchar *command;
  
  chooser_dialog = gtk_file_chooser_dialog_new (_("Select command"), GTK_WINDOW (dialog), 
                                                GTK_FILE_CHOOSER_ACTION_OPEN, 
                                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                GTK_STOCK_OPEN, GTK_RESPONSE_OK, 
                                                NULL);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (chooser_dialog), TRUE);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser_dialog), TRUE);
  
  /* add file chooser filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);
  
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);
    
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser_dialog), filter);
  
  command = gtk_entry_get_text (GTK_ENTRY (priv->entry_command));
  
  if (strlen (command) > 0) {
    if (g_path_is_absolute (command)) {
      gtk_file_chooser_select_filename (GTK_FILE_CHOOSER (chooser_dialog), command);
    } else {
      gchar *cmd_buf = NULL;
      gchar *cmd_tok = NULL;
      gchar *program_path = NULL;
      
      cmd_buf = g_strdup (command);
      cmd_tok = strtok (cmd_buf, " ");
      program_path = g_find_program_in_path (cmd_buf);
      if (program_path) 
        gtk_file_chooser_select_filename (GTK_FILE_CHOOSER (chooser_dialog), program_path);

      g_free (cmd_buf);
      g_free (program_path);
    }
  } else {
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser_dialog), BINDIR);
  }
  
  if (gtk_dialog_run (GTK_DIALOG (chooser_dialog)) == GTK_RESPONSE_OK) {
    gchar *command = NULL;

    command = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser_dialog));
    gtk_entry_set_text (GTK_ENTRY (priv->entry_command), command);
    
    g_free (command);
  }
  
  gtk_widget_destroy (chooser_dialog);
}

#ifdef HAVE_LIBEXO
static void
cb_icon_button_clicked (GtkWidget *button, MenuEditorEditDialog *dialog)
{
  GtkWidget *chooser;
  gchar *icon;
  gint response;
  
  /* allocate the chooser dialog */
  chooser = exo_icon_chooser_dialog_new (_("Select icon"), GTK_WINDOW (dialog),
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										 GTK_STOCK_CLEAR, GTK_RESPONSE_NO,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT, GTK_RESPONSE_CANCEL, -1);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);

  /* setup the currently selected icon */
  icon = menueditor_edit_dialog_get_entry_icon (dialog);
  if (G_LIKELY (icon != NULL)) {
    exo_icon_chooser_dialog_set_icon (EXO_ICON_CHOOSER_DIALOG (chooser), icon);
    g_free (icon);
  }

  /* run the icon chooser dialog */
  response = gtk_dialog_run (GTK_DIALOG (chooser));
  if (response == GTK_RESPONSE_ACCEPT) {
	/* remember the selected icon from the chooser */
	icon = exo_icon_chooser_dialog_get_icon (EXO_ICON_CHOOSER_DIALOG (chooser));
	menueditor_edit_dialog_set_entry_icon (dialog, icon);
	g_free (icon);
  } else if (response == GTK_RESPONSE_NO) {
	menueditor_edit_dialog_set_entry_icon (dialog, NULL);
  }

  /* destroy the chooser */
  gtk_widget_destroy (chooser);
}
#else
static void
cb_chooser_icon_update_preview (GtkFileChooser * chooser, GtkImage *preview)
{
  gchar *filename;
  GdkPixbuf *pix = NULL;

  filename = gtk_file_chooser_get_filename (chooser);
  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    pix = gdk_pixbuf_new_from_file_at_size (filename, 64, 64, NULL);
  g_free (filename);

  if (G_IS_OBJECT (pix)) {
    gtk_image_set_from_pixbuf (preview, pix);
    g_object_unref (G_OBJECT (pix));
  }
  
  gtk_file_chooser_set_preview_widget_active (chooser, G_IS_OBJECT (pix));
}

static void
cb_radio_button_themed_icon_toggled (GtkToggleButton *button, MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  if (gtk_toggle_button_get_active (button)) {
    gtk_widget_set_sensitive (priv->entry_themed_icon, TRUE);
    gtk_widget_set_sensitive (priv->chooser_icon, FALSE);
  }
}

static void
cb_radio_button_icon_toggled (GtkToggleButton *button, MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  if (gtk_toggle_button_get_active (button)) {
    gtk_widget_set_sensitive (priv->entry_themed_icon, FALSE);
    gtk_widget_set_sensitive (priv->chooser_icon, TRUE);
  }
}
#endif

static gchar *
unescape_text (const gchar *markup)
{
  gchar *text, *ptr, c;

  if (markup == NULL)
    return NULL;

  ptr = text = g_malloc (strlen (markup) + 1);

  while ((c = *markup++))
  {
    if (G_UNLIKELY (c == '&'))
    {
      if (!memcmp (markup, "amp;", 4)) {
        *ptr++ = '&';
        markup += 4;
      }
      else if (!memcmp (markup, "lt;", 3)) {
        *ptr++ = '<';
        markup += 3;
      }
      else if (!memcmp (markup, "gt;", 3)) {
        *ptr++ = '>';
        markup += 3;
      }
      else if (!memcmp (markup, "quot;", 5)) {
        *ptr++ = '"';
        markup += 5;
      }
      else if (!memcmp (markup, "apos;", 5)) {
        *ptr++ = '\'';
        markup += 5;
      }
    }
    else
      *ptr++ = c;
  }
  *ptr = 0;

  return text;
}


/******************/
/* public methods */
/******************/
GtkWidget *
menueditor_edit_dialog_new (GtkWindow *parent)
{
  GtkWidget *obj = NULL;
  MenuEditorEditDialogPrivate *priv;
  
  obj = g_object_new (menueditor_edit_dialog_get_type (), NULL);
  priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (obj);
  
  if (parent)
	gtk_window_set_transient_for (GTK_WINDOW (obj), parent);
	
  return obj;
}

void 
menueditor_edit_dialog_set_entry_type (MenuEditorEditDialog *dialog, EntryType type)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  if (type == MENU || type == TITLE || type == BUILTIN) {
    gtk_widget_hide (priv->label_command);
    gtk_widget_hide (priv->hbox_command);
    gtk_widget_hide (priv->check_button_interm);
    gtk_widget_hide (priv->check_button_snotify);
  }
}

void 
menueditor_edit_dialog_set_entry_name (MenuEditorEditDialog *dialog, const gchar *name)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  if (name) {
    char *temp = unescape_text (name);
    gtk_entry_set_text (GTK_ENTRY (priv->entry_name), temp);
    g_free (temp);
  }
}

void 
menueditor_edit_dialog_set_entry_command (MenuEditorEditDialog *dialog, const gchar *command)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);

  if (command) {
    char *temp = unescape_text (command);
    gtk_entry_set_text (GTK_ENTRY (priv->entry_command), temp);
    g_free (temp);
  }
}

void 
menueditor_edit_dialog_set_entry_icon (MenuEditorEditDialog *dialog, const gchar *icon)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
 
#ifdef HAVE_LIBEXO
  GdkPixbuf *pixbuf;
  GtkWidget *image;
  GtkWidget *label;

  /* drop the previous button child */
  if (GTK_BIN (priv->button_icon)->child != NULL)
    gtk_widget_destroy (GTK_BIN (priv->button_icon)->child);

  /* try to load the icon */
  if (G_LIKELY (icon != NULL && g_path_is_absolute (icon))) {
    /* setup the image from the file */
    pixbuf = exo_gdk_pixbuf_new_from_file_at_max_size (icon, 48, 48, TRUE, NULL);
    image = gtk_image_new_from_pixbuf (pixbuf);
    gtk_container_add (GTK_CONTAINER (priv->button_icon), image);
    g_object_unref (G_OBJECT (pixbuf));
    gtk_widget_show (image);
  }
  else if (icon != NULL && *icon != '\0') {
    /* setup a themed icon */
    image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_DIALOG);
    gtk_container_add (GTK_CONTAINER (priv->button_icon), image);
    gtk_widget_show (image);
  }
  else {
    /* setup a label to tell that no icon was selected */
    label = gtk_label_new (_("No icon"));
    gtk_container_add (GTK_CONTAINER (priv->button_icon), label);
    gtk_widget_show (label);
  }

  /* remember the icon */
  g_object_set_data_full (G_OBJECT (priv->button_icon), "menueditor-edit-dialog-icon", g_strdup (icon), g_free);
#else
  if (icon && strlen (icon) > 0) {
    if (icon[0] != '/') {
      /* themed icon */
      char *temp;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radio_button_themed_icon), TRUE);
      temp = unescape_text (icon);
      gtk_entry_set_text (GTK_ENTRY (priv->entry_themed_icon), temp);
      g_free (temp);
    } else {
      /* path to icon */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radio_button_icon), TRUE);
      gtk_file_chooser_select_filename (GTK_FILE_CHOOSER (priv->chooser_icon), icon);
    }
  }
#endif
}

void 
menueditor_edit_dialog_set_entry_startup_notification (MenuEditorEditDialog *dialog, gboolean snotify)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check_button_snotify), snotify);
}

void
menueditor_edit_dialog_set_entry_run_in_terminal (MenuEditorEditDialog *dialog, gboolean interm)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check_button_interm), interm);
}

gchar *
menueditor_edit_dialog_get_entry_name (MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry_name)));
}

gchar *
menueditor_edit_dialog_get_entry_command (MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry_command)));
}

gchar *
menueditor_edit_dialog_get_entry_icon (MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
#ifdef HAVE_LIBEXO
  return g_strdup (g_object_get_data (G_OBJECT (priv->button_icon), "menueditor-edit-dialog-icon"));
#else
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->radio_button_icon)))
    return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->chooser_icon));
  else
    return g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry_themed_icon)));
#endif
}

gboolean
menueditor_edit_dialog_get_entry_startup_notification (MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->check_button_snotify));
}

gboolean
menueditor_edit_dialog_get_entry_run_in_terminal (MenuEditorEditDialog *dialog)
{
  MenuEditorEditDialogPrivate *priv = MENUEDITOR_EDIT_DIALOG_GET_PRIVATE (dialog);
  
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->check_button_interm));
}
