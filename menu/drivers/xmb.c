/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "../menu.h"
#include "../../xmb_entry.h"
#include "../menu_input.h"
#include <file/file_path.h>
#include "../../gfx/gl_common.h"
#include "../../gfx/video_thread_wrapper.h"
#include <compat/posix_string.h>

#include "shared.h"
#include "../menu_animation.h"

#ifndef XMB_THEME
#define XMB_THEME "monochrome"
#endif

#ifndef XMB_DELAY
#define XMB_DELAY 0.02
#endif

typedef struct
{
   float alpha;
   float label_alpha;
   float zoom;
   float x;
   float y;
   GLuint icon;
   GLuint content_icon;
} xmb_node_t;

enum
{
   XMB_TEXTURE_BG = 0,
   XMB_TEXTURE_SETTINGS,
   XMB_TEXTURE_SETTING,
   XMB_TEXTURE_SUBSETTING,
   XMB_TEXTURE_ARROW,
   XMB_TEXTURE_RUN,
   XMB_TEXTURE_RESUME,
   XMB_TEXTURE_SAVESTATE,
   XMB_TEXTURE_LOADSTATE,
   XMB_TEXTURE_SCREENSHOT,
   XMB_TEXTURE_RELOAD,
   XMB_TEXTURE_FILE,
   XMB_TEXTURE_FOLDER,
   XMB_TEXTURE_ZIP,
   XMB_TEXTURE_CORE,
   XMB_TEXTURE_SWITCH_ON,
   XMB_TEXTURE_SWITCH_OFF,
   XMB_TEXTURE_LAST
};

struct xmb_texture_item
{
   GLuint id;
   char path[PATH_MAX_LENGTH];
};

typedef struct xmb_handle
{
   file_list_t *menu_stack_old;
   file_list_t *selection_buf_old;
   size_t cat_selection_ptr_old;
   size_t selection_ptr_old;
   int active_category;
   int active_category_old;
   int num_categories;
   int depth;
   int old_depth;
   char icon_dir[4];
   char box_message[PATH_MAX_LENGTH];
   char title[PATH_MAX_LENGTH];
   struct xmb_texture_item textures[XMB_TEXTURE_LAST];
   int icon_size;
   float x;
   float categories_x;
   float alpha;
   float arrow_alpha;
   float hspacing;
   float vspacing;
   float margin_left;
   float margin_top;
   float title_margin_left;
   float title_margin_top;
   float title_margin_bottom;
   float label_margin_left;
   float label_margin_top;
   float setting_margin_left;
   float above_item_offset;
   float active_item_factor;
   float under_item_offset;
   float above_subitem_offset;
   float c_active_zoom;
   float c_active_alpha;
   float i_active_zoom;
   float i_active_alpha;
   float c_passive_zoom;
   float c_passive_alpha;
   float i_passive_zoom;
   float i_passive_alpha;
   void *font;
   int font_size;
   xmb_node_t settings_node;
   bool prevent_populate;
} xmb_handle_t;

static const GLfloat rmb_vertex[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1,
};

static const GLfloat rmb_tex_coord[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0,
};

static int xmb_entry_iterate(unsigned action)
{
   const char *label = NULL;
   menu_file_list_cbs_t *cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(driver.menu->menu_list->selection_buf,
            driver.menu->selection_ptr);

   menu_list_get_last_stack(driver.menu->menu_list, NULL, &label, NULL);

   if (driver.video_data && driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture(driver.menu);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);
   
   return -1;
}

static void xmb_draw_icon(GLuint texture, float x, float y,
      float alpha, float rotation, float scale_factor)
{
   struct gl_coords coords;
   math_matrix_4x4 mymat, mrot, mscal;
   gl_t *gl;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   if (alpha == 0)
      return;

   gl = (gl_t*)video_driver_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb->icon_size/2 || x > gl->win_width
         || y < xmb->icon_size/2 || y > gl->win_height + xmb->icon_size)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
      1.0f, 1.0f, 1.0f, alpha,
   };

   if (gl->shader && gl->shader->use)
      gl->shader->use(gl, GL_SHADER_STOCK_BLEND);

   glViewport(x, gl->win_height - y, xmb->icon_size, xmb->icon_size);

   coords.vertices = 4;
   coords.vertex = rmb_vertex;
   coords.tex_coord = rmb_tex_coord;
   coords.lut_tex_coord = rmb_tex_coord;
   coords.color = color;
   glBindTexture(GL_TEXTURE_2D, texture);

   matrix_4x4_rotate_z(&mrot, rotation);
   matrix_4x4_multiply(&mymat, &mrot, &gl->mvp_no_rot);

   matrix_4x4_scale(&mscal, scale_factor, scale_factor, 1);
   matrix_4x4_multiply(&mymat, &mscal, &mymat);

   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &mymat);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);
}

static void xmb_draw_text(const char *str, float x,
      float y, float scale_factor, float alpha, bool align_right)
{
   gl_t *gl;
   uint8_t a8 = 0;
   struct font_params params = {0};
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;
   a8 = 255 * alpha;
   if (a8 == 0)
      return;

   gl = (gl_t*)video_driver_resolve(NULL);

   if (!gl)
      return;

   if (x < -xmb->icon_size || x > gl->win_width + xmb->icon_size
         || y < -xmb->icon_size || y > gl->win_height + xmb->icon_size)
      return;

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);

   params.x = x / gl->win_width;
   params.y = 1.0f - y / gl->win_height;

   params.scale = scale_factor;
   params.color = FONT_COLOR_RGBA(255, 255, 255, a8);
   params.full_screen = true;
   params.align_right = align_right;

   if (driver.video_data && driver.video_poke
       && driver.video_poke->set_osd_msg)
       driver.video_poke->set_osd_msg(driver.video_data,
                                      str, &params, xmb->font);
}

static void xmb_render_background(bool force_transparency)
{
   struct gl_coords coords;
   float alpha              = 0.75f;
   gl_t *gl                 = NULL;
   xmb_handle_t *xmb        = NULL;
   static const GLfloat vertex[] = {
      0, 0,
      1, 0,
      0, 1,
      1, 1,
   };

   static const GLfloat tex_coord[] = {
      0, 1,
      1, 1,
      0, 0,
      1, 0,
   };

   if (!driver.menu)
      return;

   xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   GLfloat color[] = {
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
      1.0f, 1.0f, 1.0f, xmb->alpha,
   };

   if (alpha > xmb->alpha)
      alpha = xmb->alpha;

   GLfloat black_color[] = {
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
      0.0f, 0.0f, 0.0f, alpha,
   };

   gl = (gl_t*)video_driver_resolve(NULL);

   if (!gl)
      return;

   glViewport(0, 0, gl->win_width, gl->win_height);


   coords.vertices = 4;
   coords.vertex = vertex;
   coords.tex_coord = tex_coord;
   coords.lut_tex_coord = tex_coord;

   if ((g_settings.menu.pause_libretro
      || !g_extern.main_is_init || g_extern.libretro_dummy)
      && !force_transparency
      && xmb->textures[XMB_TEXTURE_BG].id)
   {
      coords.color = color;
      glBindTexture(GL_TEXTURE_2D, xmb->textures[XMB_TEXTURE_BG].id);
   }
   else
   {
      coords.color = black_color;
      glBindTexture(GL_TEXTURE_2D, 0);
   }

   gl->shader->use(gl, GL_SHADER_STOCK_BLEND);
   gl->shader->set_coords(&coords);
   gl->shader->set_mvp(gl, &gl->mvp_no_rot);

   glEnable(GL_BLEND);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisable(GL_BLEND);

   gl->coords.color = gl->white_color_ptr;
}

static void xmb_get_message(const char *message)
{
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb || !message || !*message)
      return;

   strlcpy(xmb->box_message, message, sizeof(xmb->box_message));
}

static void xmb_render_messagebox(const char *message)
{
   int x, y;
   unsigned i;
   struct string_list *list = NULL;
   gl_t *gl = (gl_t*)video_driver_resolve(NULL);
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!gl || !xmb)
      return;

   list = string_split(message, "\n");
   if (!list)
      return;

   if (list->elems == 0)
   {
      string_list_free(list);
      return;
   }

   x = gl->win_width / 2 - strlen(list->elems[0].data) * xmb->font_size / 4;
   y = gl->win_height / 2 - list->size * xmb->font_size / 2;

   for (i = 0; i < list->size; i++)
   {
      const char *msg = list->elems[i].data;

      if (msg)
         xmb_draw_text(msg, x, y + i * xmb->font_size, 1, 1, 0);
   }

   string_list_free(list);
}

static void xmb_selection_pointer_changed(void)
{
   int i;
   unsigned current, end;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   current = driver.menu->selection_ptr;
   end = menu_list_get_size(driver.menu->menu_list);

   for (i = 0; i < end; i++)
   {
      float iy;
      float ia = xmb->i_passive_alpha;
      float iz = xmb->i_passive_zoom;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(
            driver.menu->menu_list->selection_buf, i);

      if (!node)
         continue;

      iy = xmb->vspacing;

      if (i < current)
         if (xmb->depth > 1)
            iy *= (i - (int)current + xmb->above_subitem_offset);
         else
            iy *= (i - (int)current + xmb->above_item_offset);
      else
         iy *= (i - (int)current + xmb->under_item_offset);

      if (i == current)
      {
         ia = xmb->i_active_alpha;
         iz = xmb->i_active_zoom;
         iy = xmb->vspacing * xmb->active_item_factor;
      }

      add_tween(XMB_DELAY, ia, &node->alpha, &inOutQuad, NULL);
      add_tween(XMB_DELAY, ia, &node->label_alpha, &inOutQuad, NULL);
      add_tween(XMB_DELAY, iz, &node->zoom,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, iy, &node->y,     &inOutQuad, NULL);
   }
}

static void xmb_list_open_old(file_list_t *list, int dir, size_t current)
{
   int i;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      float ia = 0;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      if (!node)
         continue;

      if (i == current)
         ia = xmb->i_active_alpha;
      if (dir == -1)
         ia = 0;
      add_tween(XMB_DELAY, ia, &node->alpha, &inOutQuad, NULL);
      add_tween(XMB_DELAY, 0, &node->label_alpha, &inOutQuad, NULL);
      //if (i == current)
         add_tween(XMB_DELAY, xmb->icon_size*dir*-2, &node->x, &inOutQuad, NULL);
      //else
      //   add_tween(XMB_DELAY, xmb->icon_size*dir*-1, &node->x, &inOutQuad, NULL);
   }
}

static void xmb_list_open_new(file_list_t *list, int dir, size_t current)
{
   int i;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      float iy = 0;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);
       
      if (!xmb)
          continue;

      if (dir == 1 || (dir == -1 && i != current))
         node->alpha = 0;

      if (dir == 1 || dir == -1)
         node->label_alpha = 0;

      //if (dir == 1 || (dir == -1 && i == current))
         node->x = xmb->icon_size*dir*2;
      //else
      //   node->x = xmb->icon_size*dir;

         iy = xmb->vspacing;
      if (i < current)
         if (xmb->depth > 1)
            iy *= (i - (int)current + xmb->above_subitem_offset);
         else
            iy *= (i - (int)current + xmb->above_item_offset);
      else
         iy *= (i - (int)current + xmb->under_item_offset);

      if (i == current)
         iy = xmb->vspacing * xmb->active_item_factor;

      node->y = iy;

      if (i == current)
         node->zoom = 1;
   }
   for (i = 0; i < file_list_get_size(list); i++)
   {
      float ia;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);
       
      if (!xmb)
          continue;

      ia = xmb->i_passive_alpha;
      if (i == current)
         ia = xmb->i_active_alpha;

      add_tween(XMB_DELAY, ia, &node->alpha,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, ia, &node->label_alpha,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, 0, &node->x, &inOutQuad, NULL);
   }

   xmb->old_depth = xmb->depth;
}

static GLuint xmb_png_texture_load_(const char * file_name)
{
   struct texture_image ti = {0};
   GLuint texture = 0;

   if (! path_file_exists(file_name))
      return 0;

   texture_image_load(&ti, file_name);

   /* Generate the OpenGL texture object */
   glGenTextures(1, &texture);
   glBindTexture(GL_TEXTURE_2D, texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ti.width, ti.height, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, ti.pixels);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glGenerateMipmap(GL_TEXTURE_2D);

   free(ti.pixels);

   return texture;
}

static int xmb_png_texture_load_wrap(void *data)
{
   const char *filename = (const char*)data;
   return xmb_png_texture_load_(filename);
}

static GLuint xmb_png_texture_load(const char* file_name)
{
   if (g_settings.video.threaded
         && !g_extern.system.hw_render_callback.context_type)
   {
      thread_video_t *thr = (thread_video_t*)driver.video_data;
      thr->cmd_data.custom_command.method = xmb_png_texture_load_wrap;
      thr->cmd_data.custom_command.data   = (void*)file_name;
      thr->send_cmd_func(thr, CMD_CUSTOM_COMMAND);
      thr->wait_reply_func(thr, CMD_CUSTOM_COMMAND);

      return thr->cmd_data.custom_command.return_value;
   }

   return xmb_png_texture_load_(file_name);
}

static xmb_node_t* xmb_node_for_entry(int i)
{
   xmb_entry_t *entry = NULL;
   xmb_entry_list_t *entry_list = NULL;
   xmb_node_t *node = NULL;

   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return NULL;

   entry_list = (xmb_entry_list_t*)g_extern.xmb_entry;
   entry = NULL;

   if (!entry_list)
      return NULL;

   entry = (xmb_entry_t*)&entry_list->list[i];

   if (!entry)
      return NULL;

   node = (xmb_node_t*)entry->userdata;

   if (node)
      return node;

   entry->userdata = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!entry->userdata)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return NULL;
   }

   node = (xmb_node_t*)entry->userdata;

   if (!node)
      return NULL;

   node->alpha = xmb->c_passive_alpha;
   node->zoom  = xmb->c_passive_zoom;

   if ((i + 1) == xmb->active_category)
   {
      node->alpha = xmb->c_active_alpha;
      node->zoom  = xmb->c_active_zoom;
   }

   return node;
}

static void xmb_list_switch_old(file_list_t *list, int dir, size_t current)
{
   int i;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      if (!xmb)
          continue;

      add_tween(XMB_DELAY, 0, &node->alpha,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, 0, &node->label_alpha,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, -xmb->hspacing*dir,  &node->x, &inOutQuad, NULL);
   }
}

static void xmb_list_switch_new(file_list_t *list, int dir, size_t current)
{
   int i;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < file_list_get_size(list); i++)
   {
      float ia = 0.5;
      xmb_node_t *node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      if (!xmb)
          continue;

      node->x = xmb->hspacing * dir;
      node->alpha = 0;
      node->label_alpha = 0;

      if (i == current)
         ia = 1.0;
      add_tween(XMB_DELAY, ia, &node->alpha,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, ia, &node->label_alpha,  &inOutQuad, NULL);
      add_tween(XMB_DELAY, 0,  &node->x, &inOutQuad, NULL);
   }
}

static void xmb_set_title(void)
{
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   if (driver.menu->cat_selection_ptr == 0)
   {
      const char *dir = NULL;
      const char *label = NULL;
      unsigned menu_type = 0;

      menu_list_get_last_stack(driver.menu->menu_list, &dir, &label, &menu_type);
      get_title(label, dir, menu_type, xmb->title, sizeof(xmb->title));
   }
   else
   {
      xmb_entry_t *entry = NULL;
      xmb_entry_list_t *entry_list = (xmb_entry_list_t*)g_extern.xmb_entry;

      if (!entry_list)
         return;

      entry = (xmb_entry_t*)&entry_list->list[driver.menu->cat_selection_ptr - 1];

      if (!entry)
         return;

      strlcpy(xmb->title, entry->display_name, sizeof(xmb->title));
   }
}

static void xmb_list_open()
{
   int dir;
   unsigned j;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   dir = driver.menu->cat_selection_ptr > xmb->cat_selection_ptr_old ? 1 : -1;

   xmb->active_category += dir;

   for (j = 0; j < xmb->num_categories; j++)
   {
      float ia, iz;
      xmb_node_t *node = j ? xmb_node_for_entry(j-1) : &xmb->settings_node;

      if (!node)
         continue;

      ia = (j == xmb->active_category) ? xmb->c_active_alpha : xmb->c_passive_alpha;
      iz = (j == xmb->active_category) ? xmb->c_active_zoom : xmb->c_passive_zoom;
      add_tween(XMB_DELAY, ia, &node->alpha, &inOutQuad, NULL);
      add_tween(XMB_DELAY, iz, &node->zoom, &inOutQuad, NULL);
   }

   add_tween(XMB_DELAY, xmb->hspacing*-(float)driver.menu->cat_selection_ptr, &xmb->categories_x, &inOutQuad, NULL);
   dir = driver.menu->cat_selection_ptr > xmb->cat_selection_ptr_old ? 1 : -1;
   xmb_list_switch_old(xmb->selection_buf_old, dir, xmb->selection_ptr_old);
   xmb_list_switch_new(driver.menu->menu_list->selection_buf, dir, driver.menu->selection_ptr);
   xmb->active_category_old = driver.menu->cat_selection_ptr;
}

static void xmb_list_switch()
{
   int dir;
   unsigned j;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   xmb->depth = file_list_get_size(driver.menu->menu_list->menu_stack);

   dir = 0;
   if (xmb->depth > xmb->old_depth)
      dir = 1;
   else if (xmb->depth < xmb->old_depth)
      dir = -1;

   for (j = 0; j < xmb->num_categories; j++)
   {
      float ia;
      xmb_node_t *node = j ? xmb_node_for_entry(j-1) : &xmb->settings_node;

      if (!node)
         continue;

      ia = (j == xmb->active_category) ? xmb->c_active_alpha
            : (xmb->depth <= 1) ? xmb->c_passive_alpha : 0;
      add_tween(XMB_DELAY, ia, &node->alpha, &inOutQuad, NULL);
   }

   xmb_list_open_old(xmb->selection_buf_old, dir, xmb->selection_ptr_old);
   xmb_list_open_new(driver.menu->menu_list->selection_buf, dir, driver.menu->selection_ptr);

   if (xmb->depth == 1 || xmb->depth == 2)
      add_tween(XMB_DELAY, xmb->icon_size*-(xmb->depth*2-2), &xmb->x, &inOutQuad, NULL);

   if (xmb->depth == 1)
      add_tween(XMB_DELAY, 0, &xmb->arrow_alpha, &inOutQuad, NULL);

   if (xmb->depth == 2)
      add_tween(XMB_DELAY, 1, &xmb->arrow_alpha, &inOutQuad, NULL);

   xmb->old_depth = xmb->depth;
}

static void xmb_populate_entries(void *data, const char *path,
      const char *label, unsigned k)
{
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   if (xmb->prevent_populate)
   {
      xmb->prevent_populate = false;
      return;
   }

   xmb_set_title();

   // horizontal list switching
   if (driver.menu->cat_selection_ptr != xmb->active_category_old)
      xmb_list_open();
   // list open
   else
      xmb_list_switch();
}

static void xmb_draw_items(file_list_t *list, file_list_t *stack,
      size_t current, size_t cat_selection_ptr)
{
   unsigned i;
   const char *dir = NULL;
   const char *label = NULL;
   unsigned menu_type = 0;
   xmb_node_t *core_node = NULL;
   size_t end = file_list_get_size(list);

   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;
   if (!xmb || !list->size)
      return;

   file_list_get_last(stack, &dir, &label, &menu_type);

   if (xmb->active_category)
      core_node = xmb_node_for_entry(cat_selection_ptr - 1);

   for (i = 0; i < end; i++)
   {
      char val_buf[PATH_MAX_LENGTH], path_buf[PATH_MAX_LENGTH];
      char name[256], value[256];
      const char *path = NULL, *entry_label = NULL;
      unsigned type = 0, w = 0;
      xmb_node_t *node = NULL;

      menu_list_get_at_offset(list, i, &path, &entry_label, &type);
      node = (xmb_node_t*)file_list_get_userdata_at_offset(list, i);

      disp_set_label(list, &w, type, i, label,
            val_buf, sizeof(val_buf),
            entry_label, path,
            path_buf, sizeof(path_buf));

      if (type == MENU_FILE_CONTENTLIST_ENTRY)
         strlcpy(path_buf, path_basename(path_buf), sizeof(path_buf));

      GLuint icon = 0;
      switch(type)
      {
         case MENU_FILE_DIRECTORY:
            icon = xmb->textures[XMB_TEXTURE_FOLDER].id;
            break;
         case MENU_FILE_PLAIN:
            icon = xmb->textures[XMB_TEXTURE_FILE].id;
            break;
         case MENU_FILE_PLAYLIST_ENTRY:
            icon = xmb->textures[XMB_TEXTURE_FILE].id;
            break;
         case MENU_FILE_CONTENTLIST_ENTRY:
            icon = core_node ? core_node->content_icon : xmb->textures[XMB_TEXTURE_FILE].id;
            break;
         case MENU_FILE_CARCHIVE:
            icon = xmb->textures[XMB_TEXTURE_ZIP].id;
            break;
         case MENU_FILE_CORE:
            icon = xmb->textures[XMB_TEXTURE_CORE].id;
            break;
         case MENU_SETTING_ACTION_RUN:
            icon = xmb->textures[XMB_TEXTURE_RUN].id;
            break;
         case MENU_SETTING_ACTION_SAVESTATE:
            icon = xmb->textures[XMB_TEXTURE_SAVESTATE].id;
            break;
         case MENU_SETTING_ACTION_LOADSTATE:
            icon = xmb->textures[XMB_TEXTURE_LOADSTATE].id;
            break;
         case MENU_SETTING_ACTION_SCREENSHOT:
            icon = xmb->textures[XMB_TEXTURE_SCREENSHOT].id;
            break;
         case MENU_SETTING_ACTION_RESET:
            icon = xmb->textures[XMB_TEXTURE_RELOAD].id;
            break;
         case MENU_SETTING_ACTION:
            icon = (xmb->depth == 3) ?
                  xmb->textures[XMB_TEXTURE_SUBSETTING].id :
                  xmb->textures[XMB_TEXTURE_SETTING].id;
            break;
         case MENU_SETTING_GROUP:
            icon = xmb->textures[XMB_TEXTURE_SETTING].id;
            break;
         default:
            icon = xmb->textures[XMB_TEXTURE_SUBSETTING].id;
            break;
      }

      xmb_draw_icon(icon,
            node->x + xmb->margin_left + xmb->hspacing - xmb->icon_size/2.0, 
            xmb->margin_top + node->y + xmb->icon_size/2.0, 
            node->alpha, 
            0, 
            node->zoom);

      menu_ticker_line(name, 35, g_extern.frame_count / 20, path_buf,
         (i == current));

      xmb_draw_text(name,
            node->x + xmb->margin_left + xmb->hspacing + xmb->label_margin_left, 
            xmb->margin_top + node->y + xmb->label_margin_top, 
            1, 
            node->label_alpha,
            0);

      menu_ticker_line(value, 35, g_extern.frame_count / 20, val_buf,
            (i == current));

      if((     strcmp(val_buf, "...")
            && strcmp(val_buf, "(CORE)")
            && strcmp(val_buf, "(FILE)")
            && strcmp(val_buf, "(DIR)")
            && strcmp(val_buf, "(COMP)")
            && strcmp(val_buf, "ON")
            && strcmp(val_buf, "OFF"))
            || ((!strcmp(val_buf, "ON")
            && !xmb->textures[XMB_TEXTURE_SWITCH_ON].id)
            || (!strcmp(val_buf, "OFF")
            && !xmb->textures[XMB_TEXTURE_SWITCH_OFF].id)))
         xmb_draw_text(value,
               node->x + xmb->margin_left + xmb->hspacing + 
               xmb->label_margin_left + xmb->setting_margin_left, 
               xmb->margin_top + node->y + xmb->label_margin_top, 
               1, 
               node->label_alpha,
               0);

      if (!strcmp(val_buf, "ON") && xmb->textures[XMB_TEXTURE_SWITCH_ON].id)
         xmb_draw_icon(xmb->textures[XMB_TEXTURE_SWITCH_ON].id,
               node->x + xmb->margin_left + xmb->hspacing
               + xmb->icon_size/2.0 + xmb->setting_margin_left,
               xmb->margin_top + node->y + xmb->icon_size/2.0,
               node->alpha,
               0,
               1);

      if (!strcmp(val_buf, "OFF") && xmb->textures[XMB_TEXTURE_SWITCH_OFF].id)
         xmb_draw_icon(xmb->textures[XMB_TEXTURE_SWITCH_OFF].id,
               node->x + xmb->margin_left + xmb->hspacing
               + xmb->icon_size/2.0 + xmb->setting_margin_left,
               xmb->margin_top + node->y + xmb->icon_size/2.0,
               node->alpha,
               0,
               1);
   }
}

static void xmb_frame(void)
{
   int i, depth;
   char title_msg[PATH_MAX_LENGTH], timedate[PATH_MAX_LENGTH];
   const char *core_name = NULL;
   const char *core_version = NULL;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   gl_t *gl = (gl_t*)video_driver_resolve(NULL);

   if (!xmb || !gl)
      return;

   update_tweens(0.002);

   glViewport(0, 0, gl->win_width, gl->win_height);

   xmb_render_background(false);

   core_name = g_extern.menu.info.library_name;

   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   xmb_draw_text(
         xmb->title, xmb->title_margin_left, xmb->title_margin_top, 1, 1, 0);

   disp_timedate_set_label(timedate, sizeof(timedate), 0);

   if (g_settings.menu.timedate_enable)
      xmb_draw_text(
            timedate, gl->win_width - xmb->title_margin_left, 
            xmb->title_margin_top, 1, 1, 1);

   core_version = g_extern.menu.info.library_version;

   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
   xmb_draw_text(title_msg, xmb->title_margin_left, 
         gl->win_height - xmb->title_margin_bottom, 1, 1, 0);


   xmb_draw_icon(xmb->textures[XMB_TEXTURE_ARROW].id,
         xmb->x + xmb->margin_left + xmb->hspacing - xmb->icon_size/2.0 + xmb->icon_size,
         xmb->margin_top + xmb->icon_size/2.0 + xmb->vspacing * xmb->active_item_factor,
         xmb->arrow_alpha,
         0,
         1);

   depth = file_list_get_size(driver.menu->menu_list->menu_stack);

   xmb_draw_items(
         xmb->selection_buf_old,
         xmb->menu_stack_old,
         xmb->selection_ptr_old,
         depth > 1 ? driver.menu->cat_selection_ptr :
                     xmb->cat_selection_ptr_old);
   xmb_draw_items(
         driver.menu->menu_list->selection_buf,
         driver.menu->menu_list->menu_stack,
         driver.menu->selection_ptr,
         driver.menu->cat_selection_ptr);

   for (i = 0; i < xmb->num_categories; i++)
   {
      xmb_node_t *node = i ? xmb_node_for_entry(i-1) : &xmb->settings_node;

      if (!node)
         continue;

      xmb_draw_icon(node->icon, 
            xmb->x + xmb->categories_x + xmb->margin_left + xmb->hspacing*(i+1) - xmb->icon_size / 2.0,
            xmb->margin_top + xmb->icon_size / 2.0, 
            node->alpha, 
            0, 
            node->zoom);
   }

#ifdef GEKKO
   const char *message_queue;

   if (driver.menu->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      driver.menu->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   xmb_render_messagebox(message_queue);
#endif

   if (driver.menu->keyboard.display)
   {
      char msg[PATH_MAX_LENGTH];
      const char *str = *driver.menu->keyboard.buffer;
      if (!str)
         str = "";
      snprintf(msg, sizeof(msg), "%s\n%s",
            driver.menu->keyboard.label, str);
      xmb_render_background(true);
      xmb_render_messagebox(msg);
   }

   if (xmb->box_message[0] != '\0')
   {
      xmb_render_background(true);
      xmb_render_messagebox(xmb->box_message);
      xmb->box_message[0] = '\0';
   }

   gl_set_viewport(gl, gl->win_width, gl->win_height, false, false);
}

static void *xmb_init(void)
{
   menu_handle_t *menu = NULL;
   xmb_handle_t *xmb = NULL;
   const video_driver_t *video_driver = NULL;
   float scale_factor = 1;
   gl_t *gl = (gl_t*)video_driver_resolve(&video_driver);

   if (video_driver != &video_gl || !gl)
   {
      RARCH_ERR("Cannot initialize GLUI menu driver: gl video driver is not active.\n");
      return NULL;
   }

   menu = (menu_handle_t*)calloc(1, sizeof(*menu));

   if (!menu)
      return NULL;

   menu->userdata = (xmb_handle_t*)calloc(1, sizeof(xmb_handle_t));

   if (!menu->userdata)
   {
      free(menu);
      return NULL;
   }

   xmb = (xmb_handle_t*)menu->userdata;

   xmb->menu_stack_old = (file_list_t*)calloc(1, sizeof(file_list_t));
   xmb->selection_buf_old = (file_list_t*)calloc(1, sizeof(file_list_t));

   xmb->active_category = 0;
   xmb->active_category_old = 0;
   xmb->x               = 0;
   xmb->categories_x    = 0;
   xmb->alpha           = 1.0f;
   xmb->arrow_alpha     = 0;
   xmb->depth           = 1;
   xmb->old_depth       = 1;
   xmb->alpha           = 0;
   xmb->prevent_populate = false;

   xmb->c_active_zoom   = 1.0;
   xmb->c_passive_zoom  = 0.5;
   xmb->i_active_zoom   = 1.0;
   xmb->i_passive_zoom  = 0.5;

   xmb->c_active_alpha  = 1.0;
   xmb->c_passive_alpha = 0.5;
   xmb->i_active_alpha  = 1.0;
   xmb->i_passive_alpha = 0.5;

   xmb->above_subitem_offset = 1.5;
   xmb->above_item_offset    = -1.0;
   xmb->active_item_factor   = 3.0;
   xmb->under_item_offset    = 5.0;

   if (gl->win_width >= 3840)
      scale_factor = 2.0;
   else if (gl->win_width >= 2560)
      scale_factor = 1.5;
   else if (gl->win_width >= 1920)
      scale_factor = 1.0;
   else if (gl->win_width >= 1280)
      scale_factor = 0.75;
   else if (gl->win_width >=  640)
      scale_factor = 0.5;
   else if (gl->win_width >=  320)
      scale_factor = 0.25;

   strlcpy(xmb->icon_dir, "256", sizeof(xmb->icon_dir));

   xmb->icon_size = 128.0 * scale_factor;
   xmb->font_size = 32.0 * scale_factor;
   xmb->hspacing = 200.0 * scale_factor;
   xmb->vspacing = 64.0 * scale_factor;
   xmb->margin_left = 336.0 * scale_factor;
   xmb->margin_top = (256+32) * scale_factor;
   xmb->title_margin_left = 60 * scale_factor;
   xmb->title_margin_top = 60 * scale_factor + xmb->font_size/3;
   xmb->title_margin_bottom = 60 * scale_factor - xmb->font_size/3;
   xmb->label_margin_left = 85.0 * scale_factor;
   xmb->label_margin_top = xmb->font_size/3.0;
   xmb->setting_margin_left = 600.0 * scale_factor;

   g_extern.xmb_entry = xmb_entry_list_new(g_settings.xmb_entry_path);

   xmb->num_categories = g_extern.xmb_entry ? (g_extern.xmb_entry->count + 1) : 1;

   return menu;
}

static void xmb_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (menu->userdata)
      free(menu->userdata);
}

static bool xmb_font_init_first(const gl_font_renderer_t **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float xmb_font_size)
{
   if (g_settings.video.threaded
         && !g_extern.system.hw_render_callback.context_type)
   {
      thread_video_t *thr = (thread_video_t*)driver.video_data;
      thr->cmd_data.font_init.method = gl_font_init_first;
      thr->cmd_data.font_init.font_driver = font_driver;
      thr->cmd_data.font_init.font_handle = font_handle;
      thr->cmd_data.font_init.video_data = video_data;
      thr->cmd_data.font_init.font_path = font_path;
      thr->cmd_data.font_init.font_size = xmb_font_size;
      thr->send_cmd_func(thr, CMD_FONT_INIT);
      thr->wait_reply_func(thr, CMD_FONT_INIT);

      return thr->cmd_data.font_init.return_value;
   }

   return gl_font_init_first(font_driver, font_handle, video_data,
         font_path, xmb_font_size);
}

static void xmb_context_reset(void *data)
{
   int i, k;
   char bgpath[PATH_MAX_LENGTH];
   char mediapath[PATH_MAX_LENGTH], themepath[PATH_MAX_LENGTH],
         iconpath[PATH_MAX_LENGTH], fontpath[PATH_MAX_LENGTH],
         icon_name[PATH_MAX_LENGTH], content_icon_name[PATH_MAX_LENGTH],
         texturepath[PATH_MAX_LENGTH], content_texturepath[PATH_MAX_LENGTH];

   xmb_entry_t* entry = NULL;
   xmb_entry_list_t* entry_list = NULL;
   gl_t *gl = NULL;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;
   xmb_node_t *node = NULL;

   if (!menu)
      return;

   gl = (gl_t*)video_driver_resolve(NULL);
   if (!gl)
      return;

   xmb = (xmb_handle_t*)menu->userdata;
   if (!xmb)
      return;

   driver.gfx_use_rgba = true;

   fill_pathname_join(bgpath, g_settings.assets_directory,
         "xmb", sizeof(bgpath));

   fill_pathname_join(bgpath, bgpath, "bg.png", sizeof(bgpath));

   fill_pathname_join(mediapath, g_settings.assets_directory,
         "xmb", sizeof(mediapath));
   fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
   fill_pathname_join(iconpath, themepath, xmb->icon_dir, sizeof(iconpath));
   fill_pathname_slash(iconpath, sizeof(iconpath));

   fill_pathname_join(fontpath, themepath, "font.ttf", sizeof(fontpath));

   xmb_font_init_first(&gl->font_driver, &xmb->font, gl, fontpath, xmb->font_size);

   if (*g_settings.menu.wallpaper)
      strlcpy(xmb->textures[XMB_TEXTURE_BG].path, g_settings.menu.wallpaper,
            sizeof(xmb->textures[XMB_TEXTURE_BG].path));
   else
      fill_pathname_join(xmb->textures[XMB_TEXTURE_BG].path, iconpath,
            "bg.png", sizeof(xmb->textures[XMB_TEXTURE_BG].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SETTINGS].path, iconpath,
         "settings.png", sizeof(xmb->textures[XMB_TEXTURE_SETTINGS].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SETTING].path, iconpath,
         "setting.png", sizeof(xmb->textures[XMB_TEXTURE_SETTING].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SUBSETTING].path, iconpath,
         "subsetting.png", sizeof(xmb->textures[XMB_TEXTURE_SUBSETTING].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_ARROW].path, iconpath,
         "arrow.png", sizeof(xmb->textures[XMB_TEXTURE_ARROW].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_RUN].path, iconpath,
         "run.png", sizeof(xmb->textures[XMB_TEXTURE_RUN].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_RESUME].path, iconpath,
         "resume.png", sizeof(xmb->textures[XMB_TEXTURE_RESUME].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SAVESTATE].path, iconpath,
         "savestate.png", sizeof(xmb->textures[XMB_TEXTURE_SAVESTATE].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_LOADSTATE].path, iconpath,
         "loadstate.png", sizeof(xmb->textures[XMB_TEXTURE_LOADSTATE].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SCREENSHOT].path, iconpath,
         "screenshot.png", sizeof(xmb->textures[XMB_TEXTURE_SCREENSHOT].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_RELOAD].path, iconpath,
         "reload.png", sizeof(xmb->textures[XMB_TEXTURE_RELOAD].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_FILE].path, iconpath,
         "file.png", sizeof(xmb->textures[XMB_TEXTURE_FILE].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_FOLDER].path, iconpath,
         "folder.png", sizeof(xmb->textures[XMB_TEXTURE_FOLDER].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_ZIP].path, iconpath,
         "zip.png", sizeof(xmb->textures[XMB_TEXTURE_ZIP].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_CORE].path, iconpath,
         "core.png", sizeof(xmb->textures[XMB_TEXTURE_CORE].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SWITCH_ON].path, iconpath,
         "on.png", sizeof(xmb->textures[XMB_TEXTURE_SWITCH_ON].path));
   fill_pathname_join(xmb->textures[XMB_TEXTURE_SWITCH_OFF].path, iconpath,
         "off.png", sizeof(xmb->textures[XMB_TEXTURE_SWITCH_OFF].path));

   for (k = 0; k < XMB_TEXTURE_LAST; k++)
      xmb->textures[k].id = xmb_png_texture_load(xmb->textures[k].path);

   xmb->settings_node.icon = xmb->textures[XMB_TEXTURE_SETTINGS].id;
   xmb->settings_node.alpha = xmb->c_active_alpha;
   xmb->settings_node.zoom = xmb->c_active_zoom;

   entry_list = (xmb_entry_list_t*)g_extern.xmb_entry;

   if (!entry_list)
      return;

   for (i = 1; i < xmb->num_categories; i++)
   {
      node = xmb_node_for_entry(i-1);

      fill_pathname_join(mediapath, g_settings.assets_directory,
            "xmb", sizeof(mediapath));
      fill_pathname_join(themepath, mediapath, XMB_THEME, sizeof(themepath));
      fill_pathname_join(iconpath, themepath, xmb->icon_dir, sizeof(iconpath));
      fill_pathname_slash(iconpath, sizeof(iconpath));

      entry = (xmb_entry_t*)&entry_list->list[i-1];

      if (!entry)
         continue;

      strlcpy(texturepath, iconpath, sizeof(texturepath));
      strlcat(texturepath, entry->icon_name, sizeof(texturepath));

      strlcpy(content_texturepath, iconpath, sizeof(content_texturepath));
      strlcat(content_texturepath, entry->content_icon_name, sizeof(content_texturepath));

      node->alpha = (i == xmb->active_category) ? xmb->c_active_alpha
            : (xmb->depth <= 1) ? xmb->c_passive_alpha : 0;
      node->zoom  = (i == xmb->active_category) ? xmb->c_active_zoom : xmb->c_passive_zoom;
      node->icon  = xmb_png_texture_load(texturepath);
      node->content_icon = xmb_png_texture_load(content_texturepath);
   }
}

static void xmb_navigation_clear(void *data, bool pending_push)
{
   (void)data;

   if (pending_push)
      return;
   xmb_selection_pointer_changed();
}

static void xmb_navigation_decrement(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_increment(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_set(void *data, bool scroll)
{
   (void)data;
   (void)scroll;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_set_last(void *data)
{
   (void)data;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_descend_alphabet(void *data, size_t *unused)
{
   (void)data;
   (void)unused;

   xmb_selection_pointer_changed();
}

static void xmb_navigation_ascend_alphabet(void *data, size_t *unused)
{
   (void)data;
   (void)unused;

   xmb_selection_pointer_changed();
}

static void xmb_list_insert(void *data,
      const char *path, const char *unused, size_t list_size)
{
   float iy;
   int current = 0, i = list_size;
   xmb_node_t *node = NULL;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;
   file_list_t *list = (file_list_t*)data;

   if (!list || !xmb)
      return;

   list->list[i].userdata = (xmb_node_t*)calloc(1, sizeof(xmb_node_t));

   if (!list->list[i].userdata)
   {
      RARCH_ERR("XMB node could not be allocated.\n");
      return;
   }

   node = (xmb_node_t*)list->list[i].userdata;

   if (!node)
      return;

   current = driver.menu->selection_ptr;

   iy = (i < current) ? xmb->vspacing *
      (i - current + xmb->above_item_offset) :
      xmb->vspacing * (i - current + xmb->under_item_offset);

   if (i == current)
      iy = xmb->vspacing * xmb->active_item_factor;

   node->alpha = (i == current) ? xmb->i_active_alpha : xmb->i_passive_alpha;
   node->label_alpha = node->alpha;
   node->zoom  = (i == current) ? xmb->i_active_zoom : xmb->i_passive_zoom;
   node->y = iy;
   node->x = 0;
}

static void xmb_list_delete(void *data, size_t idx,
      size_t list_size)
{
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   if (list->list[idx].userdata)
      free(list->list[idx].userdata);
   list->list[idx].userdata = NULL;
}

static void xmb_list_clear(void *data)
{
   (void)data;
}

static void xmb_list_cache(bool horizontal, unsigned action)
{
   size_t stack_size;
   xmb_handle_t *xmb = (xmb_handle_t*)driver.menu->userdata;

   if (!xmb)
      return;

   file_list_copy(driver.menu->menu_list->selection_buf, xmb->selection_buf_old);
   file_list_copy(driver.menu->menu_list->menu_stack, xmb->menu_stack_old);
   xmb->selection_ptr_old = driver.menu->selection_ptr;

   if(!horizontal)
      return;

   xmb->cat_selection_ptr_old = driver.menu->cat_selection_ptr;
   driver.menu->cat_selection_ptr += action == MENU_ACTION_LEFT ? -1 : 1;

   stack_size = driver.menu->menu_list->menu_stack->size;

   if (driver.menu->cat_selection_ptr == 0)
   {
      strlcpy(driver.menu->menu_list->menu_stack->list[stack_size-1].label, "Main Menu", PATH_MAX_LENGTH);
      driver.menu->menu_list->menu_stack->list[stack_size-1].type = MENU_SETTINGS;
   }
   else
   {
      strlcpy(driver.menu->menu_list->menu_stack->list[stack_size-1].label, "Horizontal Menu", PATH_MAX_LENGTH);
      driver.menu->menu_list->menu_stack->list[stack_size-1].type = MENU_SETTING_HORIZONTAL_MENU;
   }
}

static void xmb_list_set_selection(void *data)
{
   (void)data;
}

static void xmb_context_destroy(void *data)
{
   unsigned i;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = (menu_handle_t*)driver.menu;

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   for (i = 0; i < XMB_TEXTURE_LAST; i++)
      glDeleteTextures(1, &xmb->textures[i].id);

   for (i = 1; i < xmb->num_categories; i++)
   {
      xmb_node_t *node = xmb_node_for_entry(i-1);

      if (!node)
         continue;

      glDeleteTextures(1, &node->icon);
      glDeleteTextures(1, &node->content_icon);
   }
}

static void xmb_toggle(bool menu_on)
{
   int i;
   xmb_handle_t *xmb = NULL;
   menu_handle_t *menu = (menu_handle_t*)driver.menu;

   if (!menu)
      return;

   xmb = (xmb_handle_t*)menu->userdata;

   if (!xmb)
      return;

   xmb->depth = file_list_get_size(driver.menu->menu_list->menu_stack);

   if (menu_on)
   {
      add_tween(XMB_DELAY, 1.0f, &xmb->alpha, &inOutQuad, NULL);

      if (xmb->active_category) {
         xmb->prevent_populate = true;
         for (i = 0; i < xmb->num_categories; i++)
         {
            xmb_node_t *node = i ? xmb_node_for_entry(i-1) : &xmb->settings_node;

            if (!node)
               continue;

            node->alpha = (i == xmb->active_category) ? xmb->c_active_alpha
                  : (xmb->depth <= 1) ? xmb->c_passive_alpha : 0;
            node->zoom  = (i == xmb->active_category) ? xmb->c_active_zoom
                  : xmb->c_passive_zoom;
         }
      }
   }
   else
      xmb->alpha = 0;
}

menu_ctx_driver_t menu_ctx_xmb = {
   NULL,
   xmb_get_message,
   NULL,
   xmb_frame,
   xmb_init,
   NULL,
   xmb_free,
   xmb_context_reset,
   xmb_context_destroy,
   xmb_populate_entries,
   NULL,
   NULL,
   xmb_toggle,
   xmb_navigation_clear,
   xmb_navigation_decrement,
   xmb_navigation_increment,
   xmb_navigation_set,
   xmb_navigation_set_last,
   xmb_navigation_descend_alphabet,
   xmb_navigation_ascend_alphabet,
   xmb_list_insert,
   xmb_list_delete,
   xmb_list_clear,
   xmb_list_cache,
   xmb_list_set_selection,
   NULL,
   NULL,
   xmb_entry_iterate,
   "xmb",
};
