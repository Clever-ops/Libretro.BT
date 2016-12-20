/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <compat/strl.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../input/input_config.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

typedef struct autoconfig_disconnect
{
   unsigned idx;
   char msg[255];
} autoconfig_disconnect_t;

/* Adds an index for devices with the same name,
 * so they can be identified in the GUI. */
static void input_autoconfigure_joypad_reindex_devices(void)
{
   unsigned i;
   settings_t      *settings = config_get_ptr();

   for(i = 0; i < settings->input.max_users; i++)
      settings->input.device_name_index[i]=0;

   for(i = 0; i < settings->input.max_users; i++)
   {
      unsigned j;
      const char *tmp = settings->input.device_names[i];
      int k           = 1;

      for(j = 0; j < settings->input.max_users; j++)
      {
         if(string_is_equal(tmp, settings->input.device_names[j])
               && settings->input.device_name_index[i] == 0)
            settings->input.device_name_index[j]=k++;
      }
   }
}

static void input_autoconfigure_joypad_conf(config_file_t *conf,
      struct retro_keybind *binds)
{
   unsigned i;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_config_parse_joy_button(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
      input_config_parse_joy_axis(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
   }
}

static int input_autoconfigure_joypad_try_from_conf(config_file_t *conf,
      autoconfig_params_t *params)
{
   char ident[256];
   char input_driver[32];
   int tmp_int                = 0;
   int              input_vid = 0;
   int              input_pid = 0;
   int                  score = 0;

   ident[0] = input_driver[0] = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));

   if (config_get_int  (conf, "input_vendor_id", &tmp_int))
      input_vid = tmp_int;

   if (config_get_int  (conf, "input_product_id", &tmp_int))
      input_pid = tmp_int;

   /* Check for VID/PID */
   if (     (params->vid == input_vid)
         && (params->pid == input_pid)
         && (params->vid != 0)
         && (params->pid != 0)
         && (input_vid   != 0)
         && (input_pid   != 0))
      score += 3;

   /* Check for name match */
   if (string_is_equal(ident, params->name))
      score += 2;
   else
   {
      if (     !string_is_empty(ident) 
            &&  string_is_equal(params->name, ident))
         score += 1;
   }

   return score;
}

static void input_autoconfigure_joypad_add(config_file_t *conf,
      autoconfig_params_t *params, retro_task_t *task)
{
   char msg[128];
   char display_name[128];
   char device_type[128];
   bool block_osd_spam                = false;
   settings_t      *settings          = config_get_ptr();

   msg[0] = display_name[0] = device_type[0] = '\0';

   config_get_array(conf, "input_device_display_name",
         display_name, sizeof(display_name));
   config_get_array(conf, "input_device_type", device_type,
         sizeof(device_type));

   if (!settings)
      return;

   /* This will be the case if input driver is reinitialized.
    * No reason to spam autoconfigure messages every time. */
   block_osd_spam = settings->input.autoconfigured[params->idx]
      && !string_is_empty(params->name);

   settings->input.autoconfigured[params->idx] = true;
   input_autoconfigure_joypad_conf(conf,
         settings->input.autoconf_binds[params->idx]);

   if (string_is_equal(device_type, "remote"))
   {
      static bool remote_is_bound        = false;

      snprintf(msg, sizeof(msg), "%s configured.",
            string_is_empty(display_name) ? params->name : display_name);

      if(!remote_is_bound)
         task->title = strdup(msg);
      remote_is_bound = true;
   }
   else
   {
      snprintf(msg, sizeof(msg), "%s %s #%u.",
            string_is_empty(display_name) ? params->name : display_name,
            msg_hash_to_str(MSG_DEVICE_CONFIGURED_IN_PORT),
            params->idx);

      if (!block_osd_spam)
         task->title = strdup(msg);
   }

   if (!string_is_empty(params->name))
      strlcpy(settings->input.device_names[params->idx],
            params->name,
            sizeof(settings->input.device_names[params->idx]));
   settings->input.pid[params->idx] = params->pid;
   settings->input.vid[params->idx] = params->vid;

   input_autoconfigure_joypad_reindex_devices();
}

static int input_autoconfigure_joypad_from_conf(
      config_file_t *conf, autoconfig_params_t *params, retro_task_t *task)
{
   int ret = input_autoconfigure_joypad_try_from_conf(conf,
         params);

   if (ret)
      input_autoconfigure_joypad_add(conf, params, task);

   config_file_free(conf);

   return ret;
}

static bool input_autoconfigure_joypad_from_conf_dir(
      autoconfig_params_t *params, retro_task_t *task)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   int ret                    = 0;
   int index                  = -1;
   int current_best           = 0;
   config_file_t *conf        = NULL;
   struct string_list *list   = NULL;

   path[0]                    = '\0';

   fill_pathname_application_special(path, sizeof(path),
         APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG);

   list = dir_list_new_special(path, DIR_LIST_AUTOCONFIG, "cfg");

   if (!list || !list->size)
   {
      settings_t *settings = config_get_ptr();
      if (list)
         string_list_free(list);
      list = dir_list_new_special(settings->directory.autoconfig,
            DIR_LIST_AUTOCONFIG, "cfg");
   }

   if(!list)
      return false;

   RARCH_LOG("Autodetect: %d profiles found.\n", list->size);

   for (i = 0; i < list->size; i++)
   {
      conf = config_file_new(list->elems[i].data);

      if (conf)
         ret  = input_autoconfigure_joypad_try_from_conf(conf, params);

      if(ret >= current_best)
      {
         index = i;
         current_best = ret;
      }
      config_file_free(conf);
   }

   if(index >= 0 && current_best > 0)
   {
      conf = config_file_new(list->elems[index].data);

      if (conf)
      {
         char conf_path[PATH_MAX_LENGTH];

         conf_path[0] = '\0';

         config_get_config_path(conf, conf_path, sizeof(conf_path));

         RARCH_LOG("Autodetect: selected configuration: %s\n", conf_path);
         input_autoconfigure_joypad_add(conf, params, task);
         config_file_free(conf);
         ret = 1;
      }
   }
   else
      ret = 0;

   string_list_free(list);

   if (ret == 0)
      return false;
   return true;
}

static bool input_autoconfigure_joypad_from_conf_internal(
      autoconfig_params_t *params, retro_task_t *task)
{
   size_t i;
   settings_t *settings = config_get_ptr();

   /* Load internal autoconfig files  */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      config_file_t *conf = config_file_new_from_string(
            input_builtin_autoconfs[i]);
      if (conf && input_autoconfigure_joypad_from_conf(conf, params, task))
         return true;
   }

   if (string_is_empty(settings->directory.autoconfig))
      return true;
   return false;
}

static bool input_autoconfigure_joypad_init(autoconfig_params_t *params)
{
   size_t i;
   settings_t *settings = config_get_ptr();

   if (!settings || !settings->input.autodetect_enable)
      return false;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      settings->input.autoconf_binds[params->idx][i].joykey           = NO_BTN;
      settings->input.autoconf_binds[params->idx][i].joyaxis          = AXIS_NONE;
      settings->input.autoconf_binds[params->idx][i].joykey_label[0]  = '\0';
      settings->input.autoconf_binds[params->idx][i].joyaxis_label[0] = '\0';
   }
   settings->input.autoconfigured[params->idx] = false;

   return true;
}

static void input_autoconfigure_connect_handler(retro_task_t *task)
{
   autoconfig_params_t *params = (autoconfig_params_t*)task->state;

   if (!params || !input_autoconfigure_joypad_init(params) || string_is_empty(params->name))
   {
      free(params);
      task->finished = true;
      return;
   }

   if (     !input_autoconfigure_joypad_from_conf_dir(params, task)
         && !input_autoconfigure_joypad_from_conf_internal(params, task))
   {
      char msg[255];

      msg[0] = '\0';

      RARCH_LOG("Autodetect: no profiles found for %s (%d/%d).\n",
            params->name, params->vid, params->pid);

      snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
            params->name, (long)params->vid, (long)params->pid,
            msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED));
      task->title = strdup(msg);
   }

   free(params);
   task->finished = true;
}

static void input_autoconfigure_disconnect_handler(retro_task_t *task)
{
   autoconfig_disconnect_t *params = (autoconfig_disconnect_t*)task->state;
   settings_t            *settings = config_get_ptr();

   task->title    = strdup(params->msg);
   task->finished = true;

   settings->input.device_names[params->idx][0] = '\0';

   RARCH_LOG("%s: %s\n", msg_hash_to_str(MSG_AUTODETECT), params->msg);

   free(params);
}

bool input_autoconfigure_disconnect(unsigned i, const char *ident)
{
   char msg[255];
   retro_task_t         *task     = (retro_task_t*)calloc(1, sizeof(*task));
   autoconfig_disconnect_t *state = (autoconfig_disconnect_t*)calloc(1, sizeof(*state));

   msg[0]      = '\0';

   state->idx  = i;

   snprintf(msg, sizeof(msg), "%s #%u (%s).", 
         msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT),
         i, ident);

   if (!task || !state)
      goto error;

   strlcpy(state->msg, msg, sizeof(state->msg));

   task->state   = state;
   task->handler = input_autoconfigure_disconnect_handler;

   task_queue_ctl(TASK_QUEUE_CTL_PUSH, task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}

bool input_autoconfigure_connect(autoconfig_params_t *params)
{
   retro_task_t         *task = (retro_task_t*)calloc(1, sizeof(*task));
   autoconfig_params_t *state = (autoconfig_params_t*)calloc(1, sizeof(*state));
   settings_t       *settings = config_get_ptr();

   if (!task || !state || !settings->input.autodetect_enable)
      goto error;

   strlcpy(state->name, params->name, sizeof(state->name));

   state->idx    = params->idx;
   state->vid    = params->vid;
   state->pid    = params->pid;

   task->state   = state;
   task->handler = input_autoconfigure_connect_handler;

   task_queue_ctl(TASK_QUEUE_CTL_PUSH, task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
