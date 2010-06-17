#include <string.h>

#include <gtk/gtk.h>
#include <glib.h>

static void on_entry_changed(GtkEntry *entry, GtkLabel *label)
{
	const char *text;

	text = gtk_entry_get_text(entry);
	if (strlen(text)) {
		char *markup;

		markup = g_markup_printf_escaped("<b>Hello %s!</b>", text);
		gtk_label_set_markup(label, markup);
		g_free(markup);
	} else {
		gtk_label_set_markup(label, "<b>Hello World!</b>");
	}
}

int main(int argc, char **argv)
{
	GtkWidget *win, *vbox, *img, *label, *entry;

	gtk_init(&argc, &argv);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(win, "delete-event", G_CALLBACK(gtk_main_quit), NULL);
	gtk_window_set_title(GTK_WINDOW(win), "Hello World!");
	gtk_window_set_icon_from_file(GTK_WINDOW(win),
			"/opt/example.com/helloworld-gui/share/Tux_small.png",
			NULL);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);

	img = gtk_image_new_from_file(
			"/opt/example.com/helloworld-gui/share/Tux.png");
	gtk_box_pack_start(GTK_BOX(vbox), img, TRUE, TRUE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), "<b>Hello World!</b>");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), "World");
	g_signal_connect(GTK_EDITABLE (entry), "changed",
			 G_CALLBACK(on_entry_changed), label);
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

	gtk_widget_show_all(win);
	gtk_main();
	return 0;
}
