/*
 *  desktop-menu-dentry.[ch] - routines for gathering .desktop entry data
 *
 *  Copyright (C) 2004 Danny Milosavljevic <danny.milo@gmx.net>
 *                2004 Brian Tarricone <bjt23@cornell.edu>
 *                2004 Benedikt Meurer <benny@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/xfce-appmenuitem.h>
#include <libxfcegui4/icons.h>

#include "desktop-menu-dentry.h"
#include "desktop-menu-private.h"
#include "desktop-menu.h"
#include "desktop-menuspec.h"
#include "desktop-menu-cache.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define CATEGORIES_FILE "xfce-registered-categories.xml"

static void menu_dentry_legacy_init();
G_INLINE_FUNC gboolean menu_dentry_legacy_need_update(XfceDesktopMenu *desktop_menu);
static void menu_dentry_legacy_add_all(XfceDesktopMenu *desktop_menu,
        MenuPathType pathtype);

static const char *dentry_keywords [] = {
   "Name", "Comment", "Icon", "Hidden", "StartupNotify",
   "Categories", "OnlyShowIn", "Exec", "TryExec", "Terminal",
   "NoDisplay", "GenericName",
};

/* these .desktop files _should_ have an OnlyShowIn key, but don't.  i'm going
 * to match by the Exec field.  */
static char *blacklist_arr[] = {
    "gnome-control-center",
    "kmenuedit",
    "kcmshell",
    "kcontrol",
    "kpersonalizer",
    "kappfinder",
    "kfmclient",
    "*.kss",
    NULL
};
static GList *blacklist = NULL;
static gchar **legacy_dirs = NULL;
static GHashTable *dir_to_cat = NULL;

/* we don't want most command-line parameters if they're given. */
G_INLINE_FUNC gchar *
_sanitise_dentry_cmd(gchar *cmd)
{
    gchar *p;
    
    /* this is the naive approach: if there's a '%' character in there, we're
     * going to strip all parameters.  this may not be the best thing to do,
     * but anything smarter is non-trivial and slow. */
    if(cmd && strchr(cmd, '%') && (p=strchr(cmd, ' ')))
        *p = 0;
    
    return cmd;
}

G_INLINE_FUNC gint
_get_path_depth(const gchar *path)
{
    gchar *p;
    gint cnt = 0;
    //TRACE("dummy");
    for(p=strchr(path, '/'); p; p=strchr(p+1, '/'))
        cnt++;
    
    return cnt;
}

/* O(n^2).  dammit. */
static void
_prune_generic_paths(GPtrArray *paths)
{
    gint i, j;
    GPtrArray *arr = g_ptr_array_sized_new(5);
    //TRACE("dummy");
    for(i=0; i<paths->len; i++) {
        gchar *comp = g_ptr_array_index(paths, i);
        for(j=0; j<paths->len; j++) {
            if(i == j)
                continue;
            if(strstr(comp, g_ptr_array_index(paths, j)) == comp)
                g_ptr_array_add(arr, g_ptr_array_index(paths, j));
        }
    }
    
    for(i=0; i<arr->len; i++)
        g_ptr_array_remove(paths, g_ptr_array_index(arr, i));
}

static gchar *
_build_path(const gchar *basepath, const gchar *path, const gchar *name)
{
    gchar *newpath = NULL;
    DBG("%s, %s, %s", basepath, path, name);
    if(basepath && *basepath == '/')
        newpath = g_build_path("/", basepath, path, name, NULL);
    else if(basepath)
        newpath = g_build_path("/", "/", basepath, path, name, NULL);
    else if(path && *path == '/')
        newpath = g_build_path("/", path, name, NULL);
    else if(path)
        newpath = g_build_path("/", "/", path, name, NULL);
    else if(name && *name == '/')
        newpath = g_strdup(name);
    else if(name)
        newpath = g_strconcat("/", name, NULL);
    
    DBG("  newpath=%s", newpath);
    
    return newpath;
}

static gint
_menu_shell_insert_sorted(GtkMenuShell *menu_shell, GtkWidget *mi,
        const gchar *name)
{
    GList *items;
    gint i;
    gchar *cmpname;
    
    //TRACE("dummy");
    
    items = gtk_container_get_children(GTK_CONTAINER(menu_shell));
    for(i=0; items; items=items->next, i++)  {
        cmpname = (gchar *)g_object_get_data(G_OBJECT(items->data), "item-name");
        if(cmpname && g_ascii_strcasecmp(name, cmpname) < 0)
            break;
    }
    
    gtk_menu_shell_insert(menu_shell, mi, i);
    
    return i;
}

/* returns menu widget */
static GtkWidget *
_ensure_path(XfceDesktopMenu *desktop_menu, const gchar *path)
{
    GtkWidget *mi = NULL, *parent = NULL, *submenu, *img;
    GdkPixbuf *pix = NULL;
    gchar *tmppath, *p, *q;
    const gchar *icon = NULL;
    gint menu_pos;
    
    DBG("%s", path);
    
    if(desktop_menu->menu_branches && 
           (submenu = g_hash_table_lookup(desktop_menu->menu_branches, path)))
        return submenu;
    else {
        tmppath = g_strdup(path);
        p = g_strrstr(tmppath, "/");
        *p = 0;
        if(*tmppath)
            parent = _ensure_path(desktop_menu, tmppath);
        if(!parent)
            parent = desktop_menu->dentry_basemenu;
        DBG("  parent=%p", parent);
        g_free(tmppath);
    }
    
    if(!parent)
        return NULL;
    
    q = g_strrstr(path, "/");
    if(q)
        q++;
    else
        q = (gchar *)path;
    
    if(desktop_menu->use_menu_icons) {
        mi = gtk_image_menu_item_new_with_label(q);
        
        icon = desktop_menuspec_displayname_to_icon(q);
        if(icon) {
            pix = gdk_pixbuf_new_from_file_at_size(icon,
                                                   _xfce_desktop_menu_icon_size,
                                                   _xfce_desktop_menu_icon_size,
                                                   NULL);
            if(pix) {
                img = gtk_image_new_from_pixbuf(pix);
                gtk_widget_show(img);
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
                g_object_unref(G_OBJECT(pix));
            }
        }
        if(!pix) {
            icon = "applications-other";
            pix = xfce_themed_icon_load(icon, _xfce_desktop_menu_icon_size);
            if(!pix) {
                _desktop_menu_ensure_unknown_icon();
                icon = "XFDESKTOP_BUILTIN_UNKNOWN_ICON";
                if(gdk_pixbuf_get_width(unknown_icon) != _xfce_desktop_menu_icon_size) {
                    GdkPixbuf *tmp = gdk_pixbuf_scale_simple(pix,
                                                             _xfce_desktop_menu_icon_size,
                                                             _xfce_desktop_menu_icon_size,
                                                             GDK_INTERP_BILINEAR);
                    pix = tmp;
                } else {
                    pix = unknown_icon;
                    g_object_ref(G_OBJECT(pix));
                }
            }
            img = gtk_image_new_from_pixbuf(pix);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
            g_object_unref(G_OBJECT(pix));
        }
    } else
        mi = gtk_menu_item_new_with_label(q);
    
    g_object_set_data_full(G_OBJECT(mi), "item-name", g_strdup(q),
            (GDestroyNotify)g_free);
    
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), submenu);
    gtk_widget_show_all(mi);
    menu_pos = _menu_shell_insert_sorted(GTK_MENU_SHELL(parent), mi, q);
    g_hash_table_insert(desktop_menu->menu_branches, g_strdup(path), submenu);
    
    desktop_menu_cache_add_entry(DM_TYPE_MENU, q, NULL, icon,
            FALSE, FALSE, parent, menu_pos, submenu);
    
    DBG("for the hell of it: basepath=%s", desktop_menu->dentry_basepath);
    
    return submenu;
}

static void
menu_cleanup_executable(gchar *string)
{
    gchar *p;

    if(!string)
        return;

    if((p = strchr(string, ' ')))
        *p = 0;

    if(string[0] == '"') {
        int i;

        for(i = 1; string[i-1] != '\0'; ++i) {
            if(string[i] != '"')
                string[i-1] = string[i];
            else {
                string[i-1] = '\0';
                break;
            }
        }
    }
}

static gint list_find(gconstpointer a, gconstpointer b)
{
    gchar *glist_elmt = (gchar *) a;
    gchar *comp_elmt  = (gchar *) b;

    if (*glist_elmt == '*')
        return !g_str_has_suffix(comp_elmt, ++glist_elmt);
    return g_ascii_strncasecmp(comp_elmt, glist_elmt, strlen (glist_elmt));
}

static gboolean
menu_dentry_parse_dentry(XfceDesktopMenu *desktop_menu, XfceDesktopEntry *de,
        MenuPathType pathtype, gboolean is_legacy, const gchar *extra_cat)
{
    gchar *categories = NULL, *hidden = NULL, *onlyshowin = NULL;
    gchar *nodisplay = NULL, *tryexec = NULL;
    gchar *path = NULL, *exec = NULL, *p;
    GtkWidget *mi = NULL, *menu;
    gint i, menu_pos;
    GPtrArray *newpaths = NULL;
    const gchar *name;
    gchar tmppath[2048];
    gboolean ret = FALSE;
    
    xfce_desktop_entry_get_string(de, "OnlyShowIn", FALSE, &onlyshowin);
    /* each element needs to be ';'-terminated.  i'm not working around
     * broken files. */
    if(onlyshowin && !strstr(onlyshowin, "XFCE;"))
        goto cleanup;
    
    xfce_desktop_entry_get_string(de, "Hidden", FALSE, &hidden);
    if(hidden && !g_ascii_strcasecmp(hidden, "true"))
        goto cleanup;
    
    xfce_desktop_entry_get_string(de, "NoDisplay", FALSE, &nodisplay);
    if(nodisplay && !g_ascii_strcasecmp(nodisplay, "true"))
        goto cleanup;
    
    xfce_desktop_entry_get_string(de, "TryExec", FALSE, &tryexec);
    menu_cleanup_executable(tryexec);
    
    if(tryexec) {
        /* check for blacklisted item */
        if(blacklist && g_list_find_custom(blacklist, tryexec, list_find))
            goto cleanup;
    
        /* Check for "TryExec" command in path */
        p = g_find_program_in_path(tryexec);
        if(!p)
            goto cleanup;
        g_free(p);
    }
    
    xfce_desktop_entry_get_string(de, "Exec", FALSE, &exec);
    if(!exec)
        goto cleanup;
    
    /* filter out quotes around the command (yeah, people do that!) */
    menu_cleanup_executable(exec);
    
    /* Check for "Exec" command in path, too */
    p = g_find_program_in_path(exec);
    if(!p)
        goto cleanup;
    g_free(p);
    
    /* check for blacklisted item */
    if(blacklist && g_list_find_custom(blacklist, exec, list_find))
        goto cleanup;
    
    xfce_desktop_entry_get_string(de, "Categories", TRUE, &categories);    
    if(categories || !is_legacy) {
        /* hack: leave out items that look like they are KDE control panels */
        if(categories && strstr(categories, ";X-KDE-"))
            goto cleanup;
        
        if(pathtype == MPATH_SIMPLE || pathtype == MPATH_SIMPLE_UNIQUE)
            newpaths = desktop_menuspec_get_path_simple(categories);
        else if(pathtype == MPATH_MULTI || pathtype == MPATH_MULTI_UNIQUE)
            newpaths = desktop_menuspec_get_path_multilevel(categories);
        
        if(!newpaths)
            goto cleanup;
    } else if(is_legacy) {
        newpaths = g_ptr_array_new();
        g_ptr_array_add(newpaths, g_strdup(extra_cat));
    }
    
    if(pathtype == MPATH_SIMPLE_UNIQUE) {
        /* grab first of the most general */
        DBG("before ensuring - basepath=%s", desktop_menu->dentry_basepath);
        path = _build_path(desktop_menu->dentry_basepath,
                g_ptr_array_index(newpaths, 0), NULL);
        menu = _ensure_path(desktop_menu, path);
        mi = xfce_app_menu_item_new_from_desktop_entry(de,
                desktop_menu->use_menu_icons);
        if(!mi)
            goto cleanup;
        name = xfce_app_menu_item_get_name(XFCE_APP_MENU_ITEM(mi));
        g_snprintf(tmppath, 2048, "%s/%s", path, name);
        if(desktop_menu->menu_entry_hash && 
                g_hash_table_lookup(desktop_menu->menu_entry_hash, tmppath))
        {
            gtk_widget_destroy(mi);
            goto cleanup;
        }
        if(desktop_menu->use_menu_icons
                && !XFCE_APP_MENU_ITEM(mi)->image_menu_item.image)
        {
            GtkWidget *image = gtk_image_new_from_pixbuf(dummy_icon);
            xfce_app_menu_item_set_image(XFCE_APP_MENU_ITEM(mi), image);
        }
        g_object_set_data(G_OBJECT(mi), "item-name", (gpointer)name);
        gtk_widget_show(mi);
        menu_pos = _menu_shell_insert_sorted(GTK_MENU_SHELL(menu), mi, name);
        DBG("before hashtable: path=%s, name=%s", path, name);
        g_hash_table_insert(desktop_menu->menu_entry_hash, _build_path(NULL,
                path, name), GINT_TO_POINTER(1));
        desktop_menu_cache_add_entry(DM_TYPE_APP, name,
                xfce_app_menu_item_get_command(XFCE_APP_MENU_ITEM(mi)),
                xfce_app_menu_item_get_icon_name(XFCE_APP_MENU_ITEM(mi)),
                xfce_app_menu_item_get_needs_term(XFCE_APP_MENU_ITEM(mi)),
                xfce_app_menu_item_get_startup_notification(XFCE_APP_MENU_ITEM(mi)),
                menu, menu_pos, NULL);
        ret = TRUE;
    } else if(pathtype == MPATH_MULTI_UNIQUE) {
        /* grab most specific */
        path = _build_path(desktop_menu->dentry_basepath,
                g_ptr_array_index(newpaths, newpaths->len-1), NULL);
        menu = _ensure_path(desktop_menu, path);
        mi = xfce_app_menu_item_new_from_desktop_entry(de,
                desktop_menu->use_menu_icons);
        if(!mi)
            goto cleanup;
        name = xfce_app_menu_item_get_name(XFCE_APP_MENU_ITEM(mi));
        g_snprintf(tmppath, 2048, "%s/%s", path, name);
        if(desktop_menu->menu_entry_hash && 
                g_hash_table_lookup(desktop_menu->menu_entry_hash, tmppath))
        {
            gtk_widget_destroy(mi);
            goto cleanup;
        }
        if(desktop_menu->use_menu_icons
                && !XFCE_APP_MENU_ITEM(mi)->image_menu_item.image)
        {
            GtkWidget *image = gtk_image_new_from_pixbuf(dummy_icon);
            xfce_app_menu_item_set_image(XFCE_APP_MENU_ITEM(mi), image);
        }
        g_object_set_data(G_OBJECT(mi), "item-name", (gpointer)name);
        gtk_widget_show(mi);
        menu_pos = _menu_shell_insert_sorted(GTK_MENU_SHELL(menu), mi, name);
        g_hash_table_insert(desktop_menu->menu_entry_hash, _build_path(NULL,
                path, name), GINT_TO_POINTER(1));
        desktop_menu_cache_add_entry(DM_TYPE_APP, name,
                xfce_app_menu_item_get_command(XFCE_APP_MENU_ITEM(mi)),
                xfce_app_menu_item_get_icon_name(XFCE_APP_MENU_ITEM(mi)),
                xfce_app_menu_item_get_needs_term(XFCE_APP_MENU_ITEM(mi)),
                xfce_app_menu_item_get_startup_notification(XFCE_APP_MENU_ITEM(mi)),
                menu, menu_pos, NULL);
        ret = TRUE;
    } else {
        if(pathtype == MPATH_MULTI)
            _prune_generic_paths(newpaths);
        for(i=0; i < newpaths->len; i++) {
            path = _build_path(desktop_menu->dentry_basepath,
                    g_ptr_array_index(newpaths, i), NULL);
            menu = _ensure_path(desktop_menu, path);
            mi = xfce_app_menu_item_new_from_desktop_entry(de,
                    desktop_menu->use_menu_icons);
            if(!mi)
                goto cleanup;
            name = xfce_app_menu_item_get_name(XFCE_APP_MENU_ITEM(mi));
            g_snprintf(tmppath, 2048, "%s/%s", path, name);
            if(desktop_menu->menu_entry_hash && 
                    g_hash_table_lookup(desktop_menu->menu_entry_hash, tmppath))
            {
                gtk_widget_destroy(mi);
                g_free(path);
                path = NULL;
                continue;
            }
            if(desktop_menu->use_menu_icons
                && !XFCE_APP_MENU_ITEM(mi)->image_menu_item.image)
            {
                GtkWidget *image = gtk_image_new_from_pixbuf(dummy_icon);
                xfce_app_menu_item_set_image(XFCE_APP_MENU_ITEM(mi), image);
            }
            g_object_set_data(G_OBJECT(mi), "item-name", (gpointer)name);
            gtk_widget_show(mi);
            menu_pos = _menu_shell_insert_sorted(GTK_MENU_SHELL(menu), mi, name);
            g_hash_table_insert(desktop_menu->menu_entry_hash, _build_path(NULL,
                    path, name), GINT_TO_POINTER(1));
            desktop_menu_cache_add_entry(DM_TYPE_APP, name,
                    xfce_app_menu_item_get_command(XFCE_APP_MENU_ITEM(mi)),
                    xfce_app_menu_item_get_icon_name(XFCE_APP_MENU_ITEM(mi)),
                    xfce_app_menu_item_get_needs_term(XFCE_APP_MENU_ITEM(mi)),
                    xfce_app_menu_item_get_startup_notification(XFCE_APP_MENU_ITEM(mi)),
                    menu, menu_pos, NULL);
            ret = TRUE;
            g_free(path);
        }
        path = NULL;
    }
    
    cleanup:
    
    if(newpaths)
        desktop_menuspec_path_free(newpaths);
    g_free(onlyshowin);
    g_free(nodisplay);
    g_free(hidden);
    g_free(categories);
    g_free(tryexec);
    g_free(exec);
    g_free(path);

    return ret;
}

static gboolean
menu_dentry_parse_dentry_file(XfceDesktopMenu *desktop_menu,
        const gchar *filename, MenuPathType pathtype) 
{
    gboolean ret = FALSE;
    XfceDesktopEntry *dentry;
    
    //TRACE("dummy");

    dentry = xfce_desktop_entry_new(filename, dentry_keywords,
            G_N_ELEMENTS(dentry_keywords));
    if(dentry) {
        ret = menu_dentry_parse_dentry(desktop_menu, dentry,
                pathtype, FALSE, NULL);
        g_object_unref(G_OBJECT(dentry));
        return ret;
    }
    
    return ret;
}

static gint
dentry_recurse_dir(GDir *dir, const gchar *path, XfceDesktopMenu *desktop_menu,
        MenuPathType pathtype)
{
    const gchar *file;
    gchar fullpath[PATH_MAX];
    GDir *d1;
    gint ndirs = 1;
    struct stat st;
    
    while((file=g_dir_read_name(dir))) {
        if(g_str_has_suffix(file, ".desktop")) {
            if(!g_hash_table_lookup(desktop_menu->menu_entry_hash, file)) {
                g_snprintf(fullpath, PATH_MAX, "%s" G_DIR_SEPARATOR_S "%s",
                        path, file);
                if(menu_dentry_parse_dentry_file(desktop_menu, fullpath,
                        pathtype))
                {
                    g_hash_table_insert(desktop_menu->menu_entry_hash,
                            g_strdup(file), GINT_TO_POINTER(1));
                }
            }
        } else {
            g_snprintf(fullpath, PATH_MAX, "%s" G_DIR_SEPARATOR_S "%s",
                    path, file);
            if((d1=g_dir_open(fullpath, 0, NULL))) {
                if(!stat(fullpath, &st)) {
                    g_hash_table_insert(desktop_menu->dentrydir_mtimes,
                            g_strdup(fullpath), GUINT_TO_POINTER(st.st_mtime));
                }
                ndirs += dentry_recurse_dir(d1, fullpath, desktop_menu, pathtype);
                g_dir_close(d1);
            }
        }
    }
    
    desktop_menu_cache_add_dentrydir(path);
    
    return ndirs;
}

static gchar *
desktop_menu_dentry_get_catfile()
{
    XfceKiosk *kiosk;
    gboolean user_menu;
    gchar filename[PATH_MAX], searchpath[PATH_MAX*3+2], **all_dirs;
    gint i;

    kiosk = xfce_kiosk_new("xfdesktop");
    user_menu = xfce_kiosk_query(kiosk, "UserMenu");
    xfce_kiosk_free(kiosk);
    
    if(!user_menu) {
        const gchar *userhome = xfce_get_homedir();
        all_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG,
                "xfce4/desktop/");
        
        for(i = 0; all_dirs[i]; i++) {
            if(strstr(all_dirs[i], userhome) != all_dirs[i]) {
                g_snprintf(searchpath, PATH_MAX*3+2,
                        "%s%%F.%%L:%s%%F.%%l:%s%%F",
                        all_dirs[i], all_dirs[i], all_dirs[i]);
                if(xfce_get_path_localized(filename, PATH_MAX, searchpath,
                        "xfce-registered-categories.xml", G_FILE_TEST_IS_REGULAR))
                {
                    g_strfreev(all_dirs);
                    return g_strdup(filename);
                }
            }            
        }
        g_strfreev(all_dirs);
    } else {
        gchar *cat_file = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                "xfce4/desktop/xfce-registered-categories.xml", FALSE);
        if(cat_file && g_file_test(cat_file, G_FILE_TEST_IS_REGULAR))
            return cat_file;
        else if(cat_file)
            g_free(cat_file);
        
        all_dirs = xfce_resource_lookup_all(XFCE_RESOURCE_CONFIG,
                "xfce4/desktop/");
        for(i = 0; all_dirs[i]; i++) {
            g_snprintf(searchpath, PATH_MAX*3+2,
                    "%s%%F.%%L:%s%%F.%%l:%s%%F",
                    all_dirs[i], all_dirs[i], all_dirs[i]);
            if(xfce_get_path_localized(filename, PATH_MAX, searchpath,
                    "xfce-registered-categories.xml", G_FILE_TEST_IS_REGULAR))
            {
                g_strfreev(all_dirs);
                return g_strdup(filename);
            }        
        }
        g_strfreev(all_dirs);
    }

    g_critical("%s: Could not locate a registered categories file", PACKAGE);

    return NULL;
}

void
desktop_menu_dentry_parse_files(XfceDesktopMenu *desktop_menu, 
        MenuPathType pathtype, gboolean do_legacy)
{
    gint i, totdirs = 0;
    gchar **dentry_paths, *catfile, *kdepath = NULL, *homepath;
    const gchar *pathd, *kdedir = g_getenv("KDEDIR");
    GDir *d;
    struct stat st;
    
    g_return_if_fail(desktop_menu != NULL);

    TRACE("base: %s", desktop_menu->dentry_basepath);
    
    catfile = desktop_menu_dentry_get_catfile();
    if(!catfile)
        return;
    if(!desktop_menuspec_parse_categories(catfile)) {
        g_critical("XfceDesktopMenu: Unable to find xfce-registered-categories.xml");
        g_free(catfile);
        return;
    }
    
    if(!blacklist) {
        for(i=0; blacklist_arr[i]; i++)
            blacklist = g_list_append(blacklist, blacklist_arr[i]);
    }
    
    /* lookup applications/ directories */
    homepath = xfce_get_homefile(".local", "share", NULL);
    if(kdedir) {
        kdepath = g_build_path(G_DIR_SEPARATOR_S, kdedir, "share", NULL);
        xfce_resource_push_path(XFCE_RESOURCE_DATA, kdepath);
    }
    xfce_resource_push_path(XFCE_RESOURCE_DATA, DATADIR);
    xfce_resource_push_path(XFCE_RESOURCE_DATA, homepath);
    dentry_paths = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "applications/");
    xfce_resource_pop_path(XFCE_RESOURCE_DATA);
    xfce_resource_pop_path(XFCE_RESOURCE_DATA);
    if(kdedir) {
        xfce_resource_pop_path(XFCE_RESOURCE_DATA);
        g_free(kdepath);
    }
    g_free(homepath);

    for(i = 0; dentry_paths[i]; i++) {
        pathd = dentry_paths[i];
        totdirs++;

        d = g_dir_open(pathd, 0, NULL);
        if(d) {
            if(!stat(pathd, &st)) {
                g_hash_table_insert(desktop_menu->dentrydir_mtimes,
                        g_strdup(pathd), GUINT_TO_POINTER(st.st_mtime));
            }
            totdirs += dentry_recurse_dir(d, pathd, desktop_menu, pathtype);
            g_dir_close(d);
        }
    }
    g_strfreev (dentry_paths);
    
    if(do_legacy) {
        menu_dentry_legacy_init();
        menu_dentry_legacy_add_all(desktop_menu, pathtype);
    }
    g_free(catfile);
    desktop_menuspec_free();
}

static void
dentry_need_update_check_ht(gpointer key, gpointer value, gpointer user_data)
{
    XfceDesktopMenu *desktop_menu = user_data;
    struct stat st;
    
    if(!stat((const char *)key, &st)) {
        if(st.st_mtime > GPOINTER_TO_UINT(value)) {
            g_hash_table_replace(desktop_menu->dentrydir_mtimes,
                    g_strdup((gchar *)key), GUINT_TO_POINTER(st.st_mtime));
            desktop_menu->modified = TRUE;
        }
    }
}

gboolean
desktop_menu_dentry_need_update(XfceDesktopMenu *desktop_menu)
{
    g_return_val_if_fail(desktop_menu != NULL, FALSE);
    
    TRACE("dummy");
    
    if(!desktop_menu->dentrydir_mtimes)
        return TRUE;
    
    desktop_menu->modified = FALSE;
    g_hash_table_foreach(desktop_menu->dentrydir_mtimes,
            dentry_need_update_check_ht, desktop_menu);
    
    TRACE("modified=%s", desktop_menu->modified?"TRUE":"FALSE");
    
    return desktop_menu->modified;
}

/*******************************************************************************
 * legacy dir support.  bleh.
 ******************************************************************************/

static gboolean
menu_dentry_legacy_parse_dentry_file(XfceDesktopMenu *desktop_menu,
        const gchar *filename, const gchar *catdir, MenuPathType pathtype)
{
    XfceDesktopEntry *dentry;
    gchar *category, *precat;

    /* check for a conversion into a freedeskop-compliant category */
    if (dir_to_cat)
                precat = g_hash_table_lookup(dir_to_cat, catdir);
    else
                precat = NULL;
        if(!precat)
        precat = (gchar *)catdir;
    /* check for a conversion into a user-defined display name */
    category = (gchar *)desktop_menuspec_cat_to_displayname(precat);
    if(!category)
        category = precat;

    dentry = xfce_desktop_entry_new(filename, dentry_keywords,
            G_N_ELEMENTS(dentry_keywords));
    if(dentry) {
        gboolean ret = menu_dentry_parse_dentry(desktop_menu, dentry,
                pathtype, TRUE, category);
        g_object_unref(G_OBJECT(dentry));
        return ret;
    }

    return FALSE;
}

static void
menu_dentry_legacy_process_dir(XfceDesktopMenu *desktop_menu,
        const gchar *basedir, const gchar *catdir, MenuPathType pathtype)
{
    GDir *dir = NULL;
    gchar const *file;
    gchar newbasedir[PATH_MAX], fullpath[PATH_MAX];
    struct stat st;
    
    if(!(dir = g_dir_open(basedir, 0, NULL)))
        return;
    
    while((file = g_dir_read_name(dir))) {
        g_snprintf(fullpath, PATH_MAX, "%s/%s", basedir, file);
        if(g_file_test(fullpath, G_FILE_TEST_IS_DIR)) {
            if(*file == '.' || strstr(file, "Settings")) /* FIXME: this is questionable */
                continue;
            /* i've made the decision to ignore categories with subdirectories.
             * that is, we're going to collapse them into their toplevel
             * category.  the subcategories i've seen are rather non-compliant,
             * and it's non-trivial and error-prone to convert them into
             * something compliant. */
            g_snprintf(newbasedir, PATH_MAX, "%s/%s", basedir, file);
            menu_dentry_legacy_process_dir(desktop_menu, newbasedir,
                    (catdir ? catdir : file), pathtype);
        } else if(catdir && g_str_has_suffix(file, ".desktop")) {
            /* we're also going to ignore category-less .desktop files. */
            if(!g_hash_table_lookup(desktop_menu->menu_entry_hash, file)) {
                if(menu_dentry_legacy_parse_dentry_file(desktop_menu,
                        fullpath, catdir, pathtype))
                {
                    g_hash_table_insert(desktop_menu->menu_entry_hash,
                            g_strdup(file), GINT_TO_POINTER(1));
                }
            }
        }
    }
    
    desktop_menu_cache_add_dentrydir(basedir);
    if(!stat(basedir, &st)) {
        g_hash_table_insert(desktop_menu->dentrydir_mtimes, g_strdup(basedir),
                GINT_TO_POINTER(st.st_mtime));
    }
    
    g_dir_close(dir);
}

static void
menu_dentry_legacy_add_all(XfceDesktopMenu *desktop_menu, MenuPathType pathtype)
{
    gint i, totdirs = 0;
    const gchar *kdedir = g_getenv("KDEDIR");
    gchar extradir[PATH_MAX];
    
    for(i=0; legacy_dirs[i]; i++) {
        totdirs++;
        menu_dentry_legacy_process_dir(desktop_menu, legacy_dirs[i],
                NULL, pathtype);
    }
    
    if(kdedir && strcmp(kdedir, "/usr")) {
        g_snprintf(extradir, PATH_MAX, "%s/share/applnk", kdedir);
        totdirs++;
        menu_dentry_legacy_process_dir(desktop_menu, extradir, NULL, pathtype);
    }
}

static void
menu_dentry_legacy_init()
{
    static gboolean is_inited = FALSE;
    gchar **apps, **applnk;
    gint napps, napplnk;
    gint i, n;
    
    if(is_inited)
        return;

    apps = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "gnome/apps/");
    for(napps = 0; apps[napps] != NULL; ++napps);

    applnk = xfce_resource_lookup_all(XFCE_RESOURCE_DATA, "applnk/");
    for(napplnk = 0; applnk[napplnk] != NULL; ++napplnk);

    legacy_dirs = g_new0(gchar *, napps + napplnk + 3);

    i = 0;

    legacy_dirs[i++] = xfce_get_homefile(".kde", "share", "apps", NULL);
    legacy_dirs[i++] = xfce_get_homefile(".kde", "share", "applnk", NULL);

    for(n = 0; n < napps; ++n, ++i)
        legacy_dirs[i] = apps[n];
    for(n = 0; n < napplnk; ++n, ++i)
        legacy_dirs[i] = applnk[n];

    g_free(applnk);
    g_free(apps);
    
    dir_to_cat = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(dir_to_cat, "Internet", "Network");
    g_hash_table_insert(dir_to_cat, "OpenOffice.org", "Office");
    g_hash_table_insert(dir_to_cat, "Utilities", "Utility");
    g_hash_table_insert(dir_to_cat, "Toys", "Utility");
    g_hash_table_insert(dir_to_cat, "Multimedia", "AudioVideo");
    g_hash_table_insert(dir_to_cat, "Applications", "Core");
    
    /* we'll keep this stuff around for the lifetime of xfdesktop.  it'll
     * give us a nice performance boost during regenerations, and the memory
     * requirements should be of minimal impact. */
    is_inited = TRUE;
}