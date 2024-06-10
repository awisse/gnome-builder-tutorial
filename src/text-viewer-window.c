/* text-viewer-window.c
 *
 * Copyright 2024 Aurel
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "text-viewer-window.h"

struct _TextViewerWindow
{
	AdwApplicationWindow  parent_instance;

  GSettings           *settings;

	/* Template widgets */
	AdwHeaderBar        *header_bar;
	GtkTextView         *main_text_view;
  GtkButton           *open_button;
};

G_DEFINE_FINAL_TYPE (TextViewerWindow, text_viewer_window, ADW_TYPE_APPLICATION_WINDOW)

static void
text_viewer_window_class_init (TextViewerWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/com/example/TextViewer/text-viewer-window.ui");
	gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, header_bar);
	gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, main_text_view);
	gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, open_button);
}

static void
text_viewer_window__open_file_dialog (GAction             *action,
                                      GVariant            *parameter,
                                      TextViewerWindow    *self);

static void
text_viewer_window_init (TextViewerWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

  g_autoptr (GSimpleAction) open_action = g_simple_action_new ("open", NULL);
  g_signal_connect (open_action, "activate", 
      G_CALLBACK (text_viewer_window__open_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (open_action));
}

static void
on_open_response (GObject       *source,
                  GAsyncResult  *result,
                  gpointer      user_data);

static void
text_viewer_window__open_file_dialog (GAction             *action,
                                      GVariant            *parameter,
                                      TextViewerWindow    *self)
{
  g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        on_open_response,
                        self);
}

static void 
open_file (TextViewerWindow *self,
           GFile            *file);

static void
on_open_response (GObject       *source,
                  GAsyncResult  *result,
                  gpointer      user_data) 
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  TextViewerWindow *self = user_data;

  g_autoptr (GFile) file =
    gtk_file_dialog_open_finish (dialog, result, NULL);

  // If the user selected a file, open it
  if (file != NULL) {
    open_file (self, file);
  }
}

static void 
open_file_complete (GObject           *source_object,
                    GAsyncResult      *result,
                    TextViewerWindow  *self);

static void 
open_file (TextViewerWindow *self,
           GFile            *file)
{
  g_file_load_contents_async (file,
                              NULL,
                              (GAsyncReadyCallback) open_file_complete,
                              self);
}

static void 
open_file_complete (GObject           *source_object,
                    GAsyncResult      *result,
                    TextViewerWindow  *self)
{
  GFile *file = G_FILE (source_object);

  g_autofree char *contents = NULL;
  gsize length = 0;

  g_autoptr (GError) error = NULL;
    
  // Complete the asynchronous operation; this function will either
  // give you the contents of the file as a byte array, or will
  // set the error argument
  g_file_load_contents_finish (file,
                               result,
                               &contents,
                               &length,
                               NULL,
                               &error);
  
  // In case of error, print a warning to the standard error output
  if (error != NULL)
  {
    g_printerr ("Unable to open \"%s\": %s\n",
                g_file_peek_path (file),
                error->message);
    return;
  }

  // Ensure that the file is encoded with UTF-8
  if (!g_utf8_validate (contents, length, NULL))
  {
    g_printerr ("Unable to load the contents of \"%s\": "
                "the file is not encoded with UTF-8\n",
                g_file_peek_path (file));
    return;
  }

  // Query the display name of the file
  g_autofree char *display_name = NULL;
  g_autoptr (GFileInfo) info =
    g_file_query_info (file,
                       "standard::display-name",
                       G_FILE_QUERY_INFO_NONE,
                       NULL,
                       NULL);
  if (info != NULL)
  {
    display_name =
      g_strdup (g_file_info_get_attribute_string (info, "standard::display-name"));
  }
  else
  {
    display_name = g_file_get_basename (file);
  }

  // Retrieve the GtkTextBuffer instance that stores the
  // text displayed by the GtkTextView widget
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);

  // Set the text using the contents of the file
  gtk_text_buffer_set_text (buffer, contents, length);

  // Repositon the cursor so it's at the start of the text
  GtkTextIter start;
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_place_cursor (buffer, &start);

  // Set the title using the display name
  gtk_window_set_title (GTK_WINDOW (self), display_name);
}

// vim: ts=2:sts=2:expandtab 
