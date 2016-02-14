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

/* KMS/DRM context, running without any window manager.
 * Based on kmscube example by Rob Clark.
 */

#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>

#include <libdrm/drm.h>
#include <gbm.h>

#include <file/dir_list.h>
#include <retro_file.h>


#include "../../driver.h"
#include "../../runloop.h"
#include "../common/drm_common.h"
#include "../common/egl_common.h"
#include "../common/gl_common.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

static struct gbm_bo *g_bo;
static struct gbm_bo *g_next_bo;
static struct gbm_surface *g_gbm_surface;
static struct gbm_device *g_gbm_dev;

static bool waiting_for_flip;

typedef struct gfx_ctx_drm_egl_data
{
   egl_ctx_data_t egl;
   RFILE *g_drm;
   unsigned g_fb_width;
   unsigned g_fb_height;
} gfx_ctx_drm_egl_data_t;

struct drm_fb
{
   struct gbm_bo *bo;
   uint32_t fb_id;
};

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
   struct drm_fb *fb = (struct drm_fb*)data;

   if (fb && fb->fb_id)
      drmModeRmFB(g_drm_fd, fb->fb_id);

   free(fb);
}

static struct drm_fb *drm_fb_get_from_bo(struct gbm_bo *bo)
{
   int ret;
   unsigned width, height, stride, handle;
   struct drm_fb *fb = (struct drm_fb*)gbm_bo_get_user_data(bo);
   if (fb)
      return fb;

   fb     = (struct drm_fb*)calloc(1, sizeof(*fb));
   fb->bo = bo;

   width  = gbm_bo_get_width(bo);
   height = gbm_bo_get_height(bo);
   stride = gbm_bo_get_stride(bo);
   handle = gbm_bo_get_handle(bo).u32;

   RARCH_LOG("[KMS/EGL]: New FB: %ux%u (stride: %u).\n",
         width, height, stride);

   ret = drmModeAddFB(g_drm_fd, width, height, 24, 32,
         stride, handle, &fb->fb_id);
   if (ret < 0)
      goto error;

   gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);
   return fb;

error:
   RARCH_ERR("[KMS/EGL]: Failed to create FB: %s\n", strerror(errno));
   free(fb);
   return NULL;
}

static void gfx_ctx_drm_egl_swap_interval(void *data, unsigned interval)
{
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)data;
   drm->egl.interval = interval;
   if (interval > 1)
      RARCH_WARN("[KMS/EGL]: Swap intervals > 1 currently not supported. Will use swap interval of 1.\n");
}

static void gfx_ctx_drm_egl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)data;
   (void)frame_count;
   (void)width;
   (void)height;

   *resize = false;
   *quit   = g_egl_quit;
}


static void drm_egl_flip_handler(int fd, unsigned frame,
      unsigned sec, unsigned usec, void *data)
{
   static unsigned first_page_flip;
   static unsigned last_page_flip;

   (void)fd;
   (void)sec;
   (void)usec;

   if (!first_page_flip)
      first_page_flip = frame;

   if (last_page_flip)
   {
      unsigned missed = frame - last_page_flip - 1;
      if (missed)
         RARCH_LOG("[KMS/EGL]: Missed %u VBlank(s) (Frame: %u, DRM frame: %u).\n",
               missed, frame - first_page_flip, frame);
   }

   last_page_flip = frame;
   *(bool*)data = false;
}

static bool wait_flip(bool block)
{
   int timeout = 0;

   if (!waiting_for_flip)
      return false;
   
   if (block)
      timeout = -1;

   while (waiting_for_flip)
   {
      if (!drm_wait_flip(timeout))
         break;
   }

   if (waiting_for_flip)
      return true;

   /* Page flip has taken place. */

   /* This buffer is not on-screen anymore. Release it to GBM. */
   gbm_surface_release_buffer(g_gbm_surface, g_bo);
   /* This buffer is being shown now. */
   g_bo = g_next_bo; 

   return false;
}

static bool queue_flip(void)
{
   struct drm_fb *fb = NULL;

   g_next_bo = gbm_surface_lock_front_buffer(g_gbm_surface);
   fb        = (struct drm_fb*)drm_fb_get_from_bo(g_next_bo);

   if (drmModePageFlip(g_drm_fd, g_crtc_id, fb->fb_id,
         DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip) == 0)
      return true;
   
   /* Failed to queue page flip. */
   return false;
}

static void gfx_ctx_drm_egl_swap_buffers(void *data)
{
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)data;
   egl_swap_buffers(data);

   /* I guess we have to wait for flip to have taken 
    * place before another flip can be queued up.
    *
    * If true, we are still waiting for a flip
    * (nonblocking mode, so just drop the frame). */
   if (wait_flip(drm->egl.interval))
      return;

   waiting_for_flip = queue_flip();

   if (gbm_surface_has_free_buffers(g_gbm_surface))
      return;

   /* We have to wait for this flip to finish. 
    * This shouldn't happen as we have triple buffered page-flips. */
   RARCH_WARN("[KMS/EGL]: Triple buffering is not working correctly ...\n");
   wait_flip(true);  
}

static bool gfx_ctx_drm_egl_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   return false;
}

static void gfx_ctx_drm_egl_update_window_title(void *data)
{
   char buf[128]        = {0};
   char buf_fps[128]    = {0};
   settings_t *settings = config_get_ptr();

   video_monitor_get_fps(buf, sizeof(buf),
         buf_fps, sizeof(buf_fps));
   if (settings->fps_show)
      runloop_msg_queue_push( buf_fps, 1, 1, false);
}

static void gfx_ctx_drm_egl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)data;

   if (!drm)
      return;

   (void)data;
   *width  = drm->g_fb_width;
   *height = drm->g_fb_height;
}

static void free_drm_resources(gfx_ctx_drm_egl_data_t *drm)
{
   if (!drm)
      return;

   if (g_gbm_surface)
      gbm_surface_destroy(g_gbm_surface);

   if (g_gbm_dev)
      gbm_device_destroy(g_gbm_dev);

   drm_free();

   if (g_drm_fd >= 0)
      retro_fclose(drm->g_drm);

   g_gbm_surface      = NULL;
   g_gbm_dev          = NULL;
   g_drm_fd           = -1;
}

static void gfx_ctx_drm_egl_destroy_resources(gfx_ctx_drm_egl_data_t *drm)
{
   if (!drm)
      return;

   /* Make sure we acknowledge all page-flips. */
   wait_flip(true);

   egl_destroy(drm);

   /* Restore original CRTC. */
   drm_restore_crtc();
   free_drm_resources(drm);

   g_drm_mode          = NULL;
   g_crtc_id           = 0;
   g_connector_id      = 0;

   drm->g_fb_width  = 0;
   drm->g_fb_height = 0;

   g_bo             = NULL;
   g_next_bo        = NULL;
}

static void *gfx_ctx_drm_egl_init(void *video_driver)
{
   int fd, i;
   unsigned monitor_index;
   unsigned gpu_index                   = 0;
   const char *gpu                      = NULL;
   struct string_list *gpu_descriptors  = NULL;
   gfx_ctx_drm_egl_data_t *drm          = (gfx_ctx_drm_egl_data_t*)
      calloc(1, sizeof(gfx_ctx_drm_egl_data_t));

   if (!drm)
      return NULL;

   fd   = -1;
   gpu_descriptors = dir_list_new("/dev/dri", NULL, false, false);

nextgpu:
   drm_restore_crtc();
   free_drm_resources(drm);

   if (!gpu_descriptors || gpu_index == gpu_descriptors->size)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't find a suitable DRM device.\n");
      goto error;
   }
   gpu = gpu_descriptors->elems[gpu_index++].data;

   drm->g_drm    = retro_fopen(gpu, RFILE_MODE_READ_WRITE, -1);
   if (!drm->g_drm)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't open DRM device.\n");
      goto nextgpu;
   }

   fd = retro_get_fd(drm->g_drm);

   if (!drm_get_resources(fd))
      goto nextgpu;

   if (!drm_get_connector(fd))
      goto nextgpu;

   if (!drm_get_encoder(fd))
      goto nextgpu;

   drm_setup(fd);

   /* First mode is assumed to be the "optimal" 
    * one for get_video_size() purposes. */
   drm->g_fb_width  = g_drm_connector->modes[0].hdisplay;
   drm->g_fb_height = g_drm_connector->modes[0].vdisplay;

   g_gbm_dev        = gbm_create_device(fd);

   if (!g_gbm_dev)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't create GBM device.\n");
      goto nextgpu;
   }

   dir_list_free(gpu_descriptors);

   /* Setup the flip handler. */
   g_drm_fds.fd                         = fd;
   g_drm_fds.events                     = POLLIN;
   g_drm_evctx.version                  = DRM_EVENT_CONTEXT_VERSION;
   g_drm_evctx.page_flip_handler        = drm_egl_flip_handler;

   g_drm_fd = fd;

   return drm;

error:
   dir_list_free(gpu_descriptors);

   gfx_ctx_drm_egl_destroy_resources(drm);

   if (drm)
      free(drm);

   return NULL;
}

static EGLint *egl_fill_attribs(gfx_ctx_drm_egl_data_t *drm, EGLint *attr)
{
   switch (drm->egl.api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         unsigned version = drm->egl.major * 1000 + drm->egl.minor;
         bool core        = version >= 3001;
#ifdef GL_DEBUG
         bool debug       = true;
#else
         const struct retro_hw_render_callback *hw_render = 
            (const struct retro_hw_render_callback*)video_driver_callback();
         bool debug       = hw_render->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = drm->egl.major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = drm->egl.minor;

            /* Technically, we don't have core/compat until 3.2.
             * Version 3.1 is either compat or not depending 
             * on GL_ARB_compatibility. */
            if (version >= 3002)
            {
               *attr++ = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
               *attr++ = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
            }
         }

         if (debug)
         {
            *attr++ = EGL_CONTEXT_FLAGS_KHR;
            *attr++ = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
         }

         break;
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
         *attr++ = EGL_CONTEXT_CLIENT_VERSION;
         *attr++ = drm->egl.major ? (EGLint)drm->egl.major : 2;
#ifdef EGL_KHR_create_context
         if (drm->egl.minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = drm->egl.minor;
         }
#endif
         break;

      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}

#define DRM_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0

static bool gfx_ctx_drm_egl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   static const EGLint egl_attribs_gl[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

   static const EGLint egl_attribs_gles[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };

#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif

   static const EGLint egl_attribs_vg[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };
   EGLint *egl_attribs_ptr = NULL;
   const EGLint *attrib_ptr;
   EGLint major, minor, n, egl_attribs[16], *attr;
   float refresh_mod;
   int i, ret                  = 0;
   struct drm_fb *fb           = NULL;
   settings_t *settings        = config_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)data;

   if (!drm)
      return false;

   egl_install_sighandlers();

   switch (g_egl_api)
   {
      case GFX_CTX_OPENGL_API:
         attrib_ptr = egl_attribs_gl;
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef EGL_KHR_create_context
         if (drm->egl.major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
         attrib_ptr = egl_attribs_gles;
         break;
      case GFX_CTX_OPENVG_API:
         attrib_ptr = egl_attribs_vg;
         break;
      default:
         attrib_ptr = NULL;
   }

   /* If we use black frame insertion, 
    * we fake a 60 Hz monitor for 120 Hz one, etc, so try to match that. */
   refresh_mod = settings->video.black_frame_insertion ? 0.5f : 1.0f;

   /* Find desired video mode, and use that.
    * If not fullscreen, we get desired windowed size, 
    * which is not appropriate. */
   if ((width == 0 && height == 0) || !fullscreen)
      g_drm_mode = &g_drm_connector->modes[0];
   else
   {
      /* Try to match settings->video.refresh_rate as closely as possible.
       * Lower resolutions tend to have multiple supported 
       * refresh rates as well.
       */
      float minimum_fps_diff = 0.0f;

      /* Find best match. */
      for (i = 0; i < g_drm_connector->count_modes; i++)
      {
         float diff;
         if (width != g_drm_connector->modes[i].hdisplay || 
               height != g_drm_connector->modes[i].vdisplay)
            continue;

         diff = fabsf(refresh_mod * g_drm_connector->modes[i].vrefresh
               - settings->video.refresh_rate);

         if (!g_drm_mode || diff < minimum_fps_diff)
         {
            g_drm_mode = &g_drm_connector->modes[i];
            minimum_fps_diff = diff;
         }
      }
   }

   if (!g_drm_mode)
   {
      RARCH_ERR("[KMS/EGL]: Did not find suitable video mode for %u x %u.\n",
            width, height);
      goto error;
   }

   drm->g_fb_width  = g_drm_mode->hdisplay;
   drm->g_fb_height = g_drm_mode->vdisplay;

   /* Create GBM surface. */
   g_gbm_surface = gbm_surface_create(
         g_gbm_dev,
         drm->g_fb_width,
         drm->g_fb_height,
         GBM_FORMAT_XRGB8888,
         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

   if (!g_gbm_surface)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't create GBM surface.\n");
      goto error;
   }


   if (!egl_init_context(drm, (EGLNativeDisplayType)g_gbm_dev, &major,
            &minor, &n, attrib_ptr))
   {
      egl_report_error();
      goto error;
   }

   attr            = egl_fill_attribs(drm, egl_attribs);
   egl_attribs_ptr = &egl_attribs[0];

   if (!egl_create_context(drm, (attr != egl_attribs_ptr) 
            ? egl_attribs_ptr : NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(drm, (EGLNativeWindowType)g_gbm_surface))
      goto error;

   glClear(GL_COLOR_BUFFER_BIT);
   egl_swap_buffers(drm);

   g_bo = gbm_surface_lock_front_buffer(g_gbm_surface);
   fb = drm_fb_get_from_bo(g_bo);

   ret = drmModeSetCrtc(g_drm_fd,
         g_crtc_id, fb->fb_id, 0, 0, &g_connector_id, 1, g_drm_mode);
   if (ret < 0)
      goto error;

   return true;

error:
   gfx_ctx_drm_egl_destroy_resources(drm);

   if (drm)
      free(drm);

   return false;
}


static void gfx_ctx_drm_egl_destroy(void *data)
{
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)data;

   if (!drm)
      return;

   gfx_ctx_drm_egl_destroy_resources(drm);
   free(drm);
}

static void gfx_ctx_drm_egl_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_drm_egl_has_focus(void *data)
{
   return true;
}

static bool gfx_ctx_drm_egl_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gfx_ctx_drm_egl_has_windowed(void *data)
{
   (void)data;
   return false;
}

static bool gfx_ctx_drm_egl_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)video_driver;

   g_egl_major = major;
   g_egl_minor = minor;
   g_egl_api   = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return eglBindAPI(EGL_OPENVG_API);
      default:
         break;
   }

   return false;
}


const gfx_ctx_driver_t gfx_ctx_drm_egl = {
   gfx_ctx_drm_egl_init,
   gfx_ctx_drm_egl_destroy,
   gfx_ctx_drm_egl_bind_api,
   gfx_ctx_drm_egl_swap_interval,
   gfx_ctx_drm_egl_set_video_mode,
   gfx_ctx_drm_egl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_drm_egl_update_window_title,
   gfx_ctx_drm_egl_check_window,
   gfx_ctx_drm_egl_set_resize,
   gfx_ctx_drm_egl_has_focus,
   gfx_ctx_drm_egl_suppress_screensaver,
   gfx_ctx_drm_egl_has_windowed,
   gfx_ctx_drm_egl_swap_buffers,
   gfx_ctx_drm_egl_input_driver,
   egl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "kms-egl",
   egl_bind_hw_render,
};
