/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "menu_entries.h"
#include "menu_action.h"
#include <file/file_list.h>
#include <file/file_path.h>
#include "../file_extract.h"
#include "../file_ops.h"
#include <file/dir_list.h>

int menu_entries_setting_set_flags(rarch_setting_t *setting)
{
   if (!setting)
      return 0;

   if (setting->flags & SD_FLAG_IS_DRIVER)
      return MENU_SETTING_DRIVER;

   switch (setting->type)
   {
      case ST_ACTION:
         return MENU_SETTING_ACTION;
      case ST_PATH:
         return MENU_FILE_PATH;
      case ST_GROUP:
         return MENU_SETTING_GROUP;
      case ST_SUB_GROUP:
         return MENU_SETTING_SUBGROUP;
      default:
         break;
   }

   return 0;
}

#ifdef HAVE_LIBRETRODB
int menu_entries_push_query(libretrodb_t *db,
   libretrodb_cursor_t *cur, file_list_t *list)
{
   unsigned i;
   struct rmsgpack_dom_value item;
    
   while (libretrodb_cursor_read_item(cur, &item) == 0)
   {
      if (item.type != RDT_MAP)
         continue;
        
      for (i = 0; i < item.map.len; i++)
      {
         struct rmsgpack_dom_value *key = &item.map.items[i].key;
         struct rmsgpack_dom_value *val = &item.map.items[i].value;
            
         if (!strcmp(key->string.buff, "name"))
         {
            menu_list_push(list, val->string.buff, db->path,
            MENU_FILE_RDB_ENTRY, 0);
            break;
         }
      }
   }
    
   return 0;
}
#endif

int menu_entries_push_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned type, unsigned setting_flags)
{
   rarch_setting_t *setting = NULL;
   
   settings_list_free(menu->list_settings);
   menu->list_settings = (rarch_setting_t *)setting_data_new(setting_flags);

   if (!(setting = (rarch_setting_t*)menu_action_find_setting(label)))
      return -1;

   menu_list_clear(list);

   /* Hack - should come up with something cleaner
    * here. */
   if (!strcmp(label, "Video Options"))
   {
#if defined(GEKKO) || defined(__CELLOS_LV2__)
      menu_list_push(list, "Screen Resolution", "",
            MENU_SETTINGS_VIDEO_RESOLUTION, 0);
#endif
      menu_list_push(list, "Custom Ratio", "",
            MENU_SETTINGS_CUSTOM_VIEWPORT, 0);
   }

   for (; setting->type != ST_END_GROUP; setting++)
   {
      if (
            setting->type == ST_GROUP ||
            setting->type == ST_SUB_GROUP ||
            setting->type == ST_END_SUB_GROUP
         )
         continue;

      menu_list_push(list, setting->short_description,
            setting->name, menu_entries_setting_set_flags(setting), 0);
   }

   if (driver.menu_ctx && driver.menu_ctx->populate_entries)
      driver.menu_ctx->populate_entries(menu, path, label, type);

   return 0;
}

static void menu_entries_content_list_push(file_list_t *list,
      const char* exts, const char* path)
{
   int num_items = 0, j;
   struct string_list *str_list = NULL;

   str_list = (struct string_list*)dir_list_new(path, exts, true);

   dir_list_sort(str_list, true);

   if (str_list)
      num_items = str_list->size;

   for (j = 0; j < num_items; j++)
   {
      if (str_list->elems[j].attr.i == RARCH_DIRECTORY)
         menu_entries_content_list_push(list, exts, str_list->elems[j].data);
      else
         menu_list_push(
               list,
               str_list->elems[j].data,
               "content_actions",
               MENU_FILE_CONTENTLIST_ENTRY,
               0);
   }

   string_list_free(str_list);
}

int menu_entries_push_horizontal_menu_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned menu_type)
{
   xmb_entry_t *entry = NULL;
   xmb_entry_list_t *entry_list = NULL;
   char content_dir[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];

   menu_list_clear(list);

   entry_list = (xmb_entry_list_t*)g_extern.xmb_entry;

   if (!entry_list)
      return -1;

   entry = (xmb_entry_t*)&entry_list->list[driver.menu->cat_selection_ptr - 1];

   if (!entry)
      return -1;

   if (entry->content_subdirectory)
      fill_pathname_join(content_dir, g_settings.content_directory,
            entry->content_subdirectory, sizeof(content_dir));

   fill_pathname_join(core_path,
         g_settings.libretro_directory, entry->core_name, sizeof(core_path));

   strlcpy(g_settings.libretro,
         core_path, sizeof(g_settings.libretro));

   core_info_t *info = find_core_info(g_extern.core_info, core_path);

   if (!info->supports_no_game)
      menu_entries_content_list_push(list, info->supported_extensions,
            content_dir);
   else
      menu_list_push(
            list,
            entry->display_name,
            "content_actions",
            MENU_FILE_CONTENTLIST_ENTRY,
            0);

   menu_list_populate_generic(menu, list, path, label, menu_type);

   return 0;
}

/**
 * menu_entries_parse_drive_list:
 * @list                     : File list handle.
 *
 * Generates default directory drive list.
 * Platform-specific.
 *
 **/
static void menu_entries_parse_drive_list(file_list_t *list)
{
   size_t i = 0;

   (void)i;

#if defined(GEKKO)
#ifdef HW_RVL
   menu_list_push(list,
         "sd:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "usb:/", "", MENU_FILE_DIRECTORY, 0);
#endif
   menu_list_push(list,
         "carda:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "cardb:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX1)
   menu_list_push(list,
         "C:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "D:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "E:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "F:", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "G:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_XBOX360)
   menu_list_push(list,
         "game:", "", MENU_FILE_DIRECTORY, 0);
#elif defined(_WIN32)
   unsigned drives = GetLogicalDrives();
   char drive[] = " :\\";
   for (i = 0; i < 32; i++)
   {
      drive[0] = 'A' + i;
      if (drives & (1 << i))
         menu_list_push(list,
               drive, "", MENU_FILE_DIRECTORY, 0);
   }
#elif defined(__CELLOS_LV2__)
   menu_list_push(list,
         "/app_home/",   "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_hdd0/",   "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_hdd1/",   "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/host_root/",  "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb000/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb001/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb002/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb003/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb004/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb005/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/dev_usb006/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(PSP)
   menu_list_push(list,
         "ms0:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "ef0:/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "host0:/", "", MENU_FILE_DIRECTORY, 0);
#elif defined(IOS)
   menu_list_push(list,
         "/var/mobile/Documents/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         "/var/mobile/", "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list,
         g_defaults.core_dir, "", MENU_FILE_DIRECTORY, 0);
   menu_list_push(list, "/", "",
         MENU_FILE_DIRECTORY, 0);
#else
   menu_list_push(list, "/", "",
         MENU_FILE_DIRECTORY, 0);
#endif
}


int menu_entries_parse_list(
      file_list_t *list, file_list_t *menu_list,
      const char *dir, const char *label, unsigned type,
      unsigned default_type_plain, const char *exts,
      rarch_setting_t *setting)
{
   size_t i, list_size;
   bool path_is_compressed, push_dir;
   int device = 0;
   struct string_list *str_list = NULL;

   (void)device;

   if (!list || !menu_list)
      return -1;

   menu_list_clear(list);

   if (!*dir)
   {
      menu_entries_parse_drive_list(list);
      if (driver.menu_ctx && driver.menu_ctx->populate_entries)
         driver.menu_ctx->populate_entries(driver.menu, dir, label, type);
      return 0;
   }
#if defined(GEKKO) && defined(HW_RVL)
   LWP_MutexLock(gx_device_mutex);
   device = gx_get_device_from_path(dir);

   if (device != -1 && !gx_devices[device].mounted &&
         gx_devices[device].interface->isInserted())
      fatMountSimple(gx_devices[device].name, gx_devices[device].interface);

   LWP_MutexUnlock(gx_device_mutex);
#endif

   path_is_compressed = path_is_compressed_file(dir);
   push_dir           = (setting && setting->browser_selection_type == ST_DIR);

   if (path_is_compressed)
      str_list = compressed_file_list_new(dir,exts);
   else
      str_list = dir_list_new(dir,
            g_settings.menu.navigation.browser.filter.supported_extensions_enable 
            ? exts : NULL, true);

   if (!str_list)
      return -1;

   dir_list_sort(str_list, true);

   if (push_dir)
      menu_list_push(list, "<Use this directory>", "",
            MENU_FILE_USE_DIRECTORY, 0);

   list_size = str_list->size;
   for (i = 0; i < str_list->size; i++)
   {
      bool is_dir;
      const char *path = NULL;
      menu_file_type_t file_type = MENU_FILE_NONE;

      switch (str_list->elems[i].attr.i)
      {
         case RARCH_DIRECTORY:
            file_type = MENU_FILE_DIRECTORY;
            break;
         case RARCH_COMPRESSED_ARCHIVE:
            file_type = MENU_FILE_CARCHIVE;
            break;
         case RARCH_COMPRESSED_FILE_IN_ARCHIVE:
            file_type = MENU_FILE_IN_CARCHIVE;
            break;
         case RARCH_PLAIN_FILE:
         default:
            if (!strcmp(label, "detect_core_list"))
            {
               if (path_is_compressed_file(str_list->elems[i].data))
               {
                  /* in case of deferred_core_list we have to interpret
                   * every archive as an archive to disallow instant loading
                   */
                  file_type = MENU_FILE_CARCHIVE;
                  break;
               }
            }
            file_type = (menu_file_type_t)default_type_plain;
            break;
      }

      is_dir = (file_type == MENU_FILE_DIRECTORY);

      if (push_dir && !is_dir)
         continue;

      /* Need to preserve slash first time. */
      path = str_list->elems[i].data;

      if (*dir && !path_is_compressed)
         path = path_basename(path);


#ifdef HAVE_LIBRETRO_MANAGEMENT
#ifdef RARCH_CONSOLE
      if (!strcmp(label, "core_list") && (is_dir ||
               strcasecmp(path, SALAMANDER_FILE) == 0))
         continue;
#endif
#endif

      /* Push type further down in the chain.
       * Needed for shader manager currently. */
      if (!strcmp(label, "core_list"))
      {
         /* Compressed cores are unsupported */
         if (file_type == MENU_FILE_CARCHIVE)
            continue;

         menu_list_push(list, path, "",
               is_dir ? MENU_FILE_DIRECTORY : MENU_FILE_CORE, 0);
      }
      else
      menu_list_push(list, path, "",
            file_type, 0);
   }

   string_list_free(str_list);

   if (!strcmp(label, "core_list"))
   {
      menu_list_get_last_stack(driver.menu->menu_list, &dir, NULL, NULL);
      list_size = file_list_get_size(list);

      for (i = 0; i < list_size; i++)
      {
         char core_path[PATH_MAX_LENGTH], display_name[PATH_MAX_LENGTH];
         const char *path = NULL;

         menu_list_get_at_offset(list, i, &path, NULL, &type);

         if (type != MENU_FILE_CORE)
            continue;

         fill_pathname_join(core_path, dir, path, sizeof(core_path));

         if (g_extern.core_info &&
               core_info_list_get_display_name(g_extern.core_info,
                  core_path, display_name, sizeof(display_name)))
            menu_list_set_alt_at_offset(list, i, display_name);
      }
      menu_list_sort_on_alt(list);
   }

   menu_list_populate_generic(driver.menu, list, dir, label, type);

   return 0;
}

int menu_entries_deferred_push(file_list_t *list, file_list_t *menu_list)
{
   unsigned type             = 0;
   const char *path          = NULL;
   const char *label         = NULL;
   menu_file_list_cbs_t *cbs = NULL;

   menu_list_get_last_stack(driver.menu->menu_list, &path, &label, &type);

   if (!strcmp(label, "Main Menu"))
      return menu_entries_push_list(driver.menu, list, path, label, type,
            SL_FLAG_MAIN_MENU);
   else if (!strcmp(label, "Horizontal Menu"))
      return menu_entries_push_horizontal_menu_list(driver.menu, list, path, label, type);

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_last_stack_actiondata(driver.menu->menu_list);

   if (!cbs->action_deferred_push)
      return 0;

   return cbs->action_deferred_push(list, menu_list, path, label, type);
}

/**
 * menu_entries_init:
 * @menu                     : Menu handle.
 *
 * Creates and initializes menu entries.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_entries_init(menu_handle_t *menu)
{
   if (!menu)
      return false;

   menu->list_settings = setting_data_new(SL_FLAG_ALL);

   menu_list_push_stack(menu->menu_list, "", "Main Menu", MENU_SETTINGS, 0);
   menu_navigation_clear(menu, true);
   menu_entries_push_list(menu, menu->menu_list->selection_buf,
         "", "Main Menu", 0, SL_FLAG_MAIN_MENU);

   return true;
}
