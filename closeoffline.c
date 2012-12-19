/*
 * CloseOffline - Add a entry into the right click context
 * menu when clicking on conversation tabs to close all that
 * are offline
 * Copyright (C) 2012 Charlie Meyer <charlie@charliemeyer.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */
#define PURPLE_PLUGINS

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pidgin.h"
#include "version.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"

/*start taken from gtkconv.c*/

#define CLOSE_CONV_TIMEOUT_SECS  (10 * 60)

static PidginWindow *hidden_convwin = NULL;

static gboolean
close_this_sucker(gpointer data)
{
	PidginConversation *gtkconv = data;
	GList *list = g_list_copy(gtkconv->convs);
	g_list_foreach(list, (GFunc)purple_conversation_destroy, NULL);
	g_list_free(list);
	return FALSE;
}

static gboolean
close_already(gpointer data)
{
	purple_conversation_destroy(data);
	return FALSE;
}

static void
hide_conv(PidginConversation *gtkconv, gboolean closetimer)
{
	GList *list;

	purple_signal_emit(pidgin_conversations_get_handle(),
			"conversation-hiding", gtkconv);

	for (list = g_list_copy(gtkconv->convs); list; list = g_list_delete_link(list, list)) {
		PurpleConversation *conv = list->data;
		if (closetimer) {
			guint timer = GPOINTER_TO_INT(purple_conversation_get_data(conv, "close-timer"));
			if (timer)
				purple_timeout_remove(timer);
			timer = purple_timeout_add_seconds(CLOSE_CONV_TIMEOUT_SECS, close_already, conv);
			purple_conversation_set_data(conv, "close-timer", GINT_TO_POINTER(timer));
		}
#if 0
		/* I will miss you */
		purple_conversation_set_ui_ops(conv, NULL);
#else
		pidgin_conv_window_remove_gtkconv(gtkconv->win, gtkconv);
		pidgin_conv_window_add_gtkconv(hidden_convwin, gtkconv);
#endif
	}
}

static gboolean
close_conv_cb(GtkButton *button, PidginConversation *gtkconv)
{
	/* We are going to destroy the conversations immediately only if the 'close immediately'
	 * preference is selected. Otherwise, close the conversation after a reasonable timeout
	 * (I am going to consider 10 minutes as a 'reasonable timeout' here.
	 * For chats, close immediately if the chat is not in the buddylist, or if the chat is
	 * not marked 'Persistent' */
	PurpleConversation *conv = gtkconv->active_conv;
	PurpleAccount *account = purple_conversation_get_account(conv);
	const char *name = purple_conversation_get_name(conv);

	switch (purple_conversation_get_type(conv)) {
		case PURPLE_CONV_TYPE_IM:
		{
			if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/close_immediately"))
				close_this_sucker(gtkconv);
			else
				hide_conv(gtkconv, TRUE);
			break;
		}
		case PURPLE_CONV_TYPE_CHAT:
		{
			PurpleChat *chat = purple_blist_find_chat(account, name);
			if (!chat ||
					!purple_blist_node_get_bool(&chat->node, "gtk-persistent"))
				close_this_sucker(gtkconv);
			else
				hide_conv(gtkconv, FALSE);
			break;
		}
		default:
			;
	}

	return TRUE;
}

/*end taken from gtkconv.c*/

static void close_offline_tabs_cb(GtkWidget *w, GObject *menu)
{
    GList *iter;
    PidginConversation *gtkconv, *gconv;
    PidginWindow *win;
    PurpleConversation *purpconv;
    PurpleAccount *account;
    PurpleBuddy *buddy;

    gtkconv = g_object_get_data(menu, "clicked_tab");

    if (!gtkconv)
        return;

    win = pidgin_conv_get_window(gtkconv);

    for (iter = pidgin_conv_window_get_gtkconvs(win); iter; )
    {
        gconv = iter->data;
        iter = iter->next;

        purpconv = gconv->active_conv;
        account = purpconv->account;
        buddy = purple_find_buddy(account, purpconv->name);

        if(!PURPLE_BUDDY_IS_ONLINE(buddy)){
            close_conv_cb(NULL, gconv);
        }
    }
}

static void create_close_offline_menu_item_pidgin(PidginConversation *gtkconv)
{
    GtkWidget *item, *menu, *separator;
    PidginWindow *win;
    GtkNotebook *notebook;
   

    win = gtkconv->win;
    notebook = (GtkNotebook*)win->notebook;
    menu = notebook->menu;
    
    item = g_object_get_data(G_OBJECT(win->notebook), "close_offline");
    if (item != NULL){
        return;
    }

    separator = pidgin_separator(menu);
    item = gtk_menu_item_new_with_label("Close offline tabs");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(close_offline_tabs_cb), 
                     menu);
    gtk_widget_show(item);

    g_object_set_data(G_OBJECT(win->notebook), "close_offline",
                          item);
    g_object_set_data(G_OBJECT(win->notebook), "close_offline_separator",
                          separator);

}

static void remove_close_offline_menu_item_pidgin(PidginConversation *gtkconv)
{
    GtkWidget *item, *separator;
    PidginWindow *win;
   
    win = gtkconv->win;
    
    item = g_object_get_data(G_OBJECT(win->notebook), "close_offline");
    if (item == NULL){
        return;
    }
    separator = g_object_get_data(G_OBJECT(win->notebook), "close_offline_separator");
    if (item != NULL) {
        gtk_widget_destroy(item);
        g_object_set_data(G_OBJECT(win->notebook),
                          "close_offline", NULL);
        gtk_widget_destroy(separator);
        g_object_set_data(G_OBJECT(win->notebook),
                          "close_offline_separator", NULL);
    }
}

static void conversation_displayed_cb(PidginConversation *gtkconv)
{
	create_close_offline_menu_item_pidgin(gtkconv);
}

static gboolean plugin_load(PurplePlugin *plugin)
{
     
	GList *convs = purple_get_conversations();
	void *gtk_conv_handle = pidgin_conversations_get_handle();

	purple_signal_connect(gtk_conv_handle, "conversation-displayed", plugin,
	                      PURPLE_CALLBACK(conversation_displayed_cb), NULL);
	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Setup context menu action */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
			create_close_offline_menu_item_pidgin(PIDGIN_CONVERSATION(conv));
		}

		convs = convs->next;
	}
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Remove context menu item button */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
			remove_close_offline_menu_item_pidgin(PIDGIN_CONVERSATION(conv));
		}

		convs = convs->next;
	}
    
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,                           /**< major version */
	PURPLE_MINOR_VERSION,                           /**< minor version */
	PURPLE_PLUGIN_STANDARD,                         /**< type */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                              /**< flags */
	NULL,                                           /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority */

	"gtkcloseoffline",                              /**< id */
	"Close offline tabs",                       /**< name */
	"1.0",                                          /**< version */
	"Close offline tabs.",                      /**< summary */
	"Adds an entry to the context menu when "
	   "right clicking on tabs to close all tabs "
	   "that have buddies that are offline.",      /**< description */
	"Charlie Meyer <charlie@charliemeyer.net>",     /**< author */
	"http://charliemeyer.net",                      /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	NULL,                                           /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	NULL,                                           /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(closeoffline, init_plugin, info)
