/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Manuel Alfayate
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

#include "../../driver.h"
#include <stdlib.h>
#include <string.h>
#include "../../general.h"
#include "../../retroarch.h"
#include <gfx/scaler/scaler.h>
#include "../video_viewport.h"
#include "../video_monitor.h"
#include "../video_context_driver.h"
#include "../font_renderer_driver.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <linux/fb.h>

#include <sys/mman.h>

#include <bcm_host.h>

#include <rthreads/rthreads.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define NUMPAGES 2

struct dispmanx_video
{ 
   DISPMANX_DISPLAY_HANDLE_T display;
   DISPMANX_MODEINFO_T amode;
   DISPMANX_UPDATE_HANDLE_T update;
   DISPMANX_ELEMENT_HANDLE_T element;
   VC_IMAGE_TYPE_T pixFormat;
   VC_DISPMANX_ALPHA_T *alpha;    
   VC_RECT_T srcRect;
   VC_RECT_T dstRect; 
   VC_RECT_T bmpRect;
   unsigned int vcImagePtr;
   unsigned int screen;
   unsigned int pitch;
   unsigned int flipPage;

   /* Internal frame dimensions */
   unsigned int width;	
   unsigned int height;	
   unsigned int bytes_per_pixel;
   /* Some cores render things we don't need between scanlines */
   unsigned int visible_width;

   bool aspectRatioCorrection;
   void *pixmem;

   struct dispmanx_page *pages;
   struct dispmanx_page *currentPage;
   struct dispmanx_page *nextPage;
   bool pageflip_pending;

   /* For console blanking */
   int fd;
   char *fbp;
   unsigned int screensize;
   char *screen_bck;

   /* For threading */
   scond_t *vsync_condition;	
   slock_t *pending_mutex;

   /* Menu */
   bool menu_active;
   DISPMANX_ELEMENT_HANDLE_T menu_element;
   DISPMANX_RESOURCE_HANDLE_T menu_resource;
   VC_IMAGE_TYPE_T menu_pixFormat;
   VC_DISPMANX_ALPHA_T *menu_alpha;    
   VC_RECT_T menu_srcRect;
   VC_RECT_T menu_dstRect; 
   VC_RECT_T menu_bmpRect;
   unsigned int menu_width;
   unsigned int menu_height;
   unsigned int menu_pitch;
   unsigned int menu_flip_page;	

   /* External aspect ratio */
   float aspect;
};

struct dispmanx_page
{
   DISPMANX_RESOURCE_HANDLE_T resource;
};

static void dispmanx_blank_console(void *data)
{
   struct fb_var_screeninfo vinfo;
   unsigned int fb_bytes_per_pixel;
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return;

   _dispvars->fd = open("/dev/fb0", O_RDWR);

   /* We need this just to know the framebuffer 
    * color depth, which vc_get_display_info() doesn't provide. */
   ioctl(_dispvars->fd, FBIOGET_VSCREENINFO, &vinfo);
   fb_bytes_per_pixel = vinfo.bits_per_pixel / 8;

   _dispvars->screensize = _dispvars->amode.width * _dispvars->amode.height * fb_bytes_per_pixel; 	
   _dispvars->fbp = (char *)mmap(0, _dispvars->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, _dispvars->fd, 0);

   /* Disable cursor blinking */
   system("setterm -cursor off");

   /* Backup console screen contents */
   _dispvars->screen_bck = (char*)malloc(_dispvars->screensize * sizeof(char));

   if (!_dispvars->screen_bck)
      goto end;

   memcpy((char*)_dispvars->screen_bck, (char*)_dispvars->fbp, _dispvars->screensize);	

   /* Blank console */
   memset((char*)(_dispvars->fbp), 0x00, _dispvars->screensize);

end:
   /* Unmap and close */
   munmap(&_dispvars->fd, _dispvars->screensize);
   close (_dispvars->fd);
}

static void dispmanx_restore_console(void *data)
{
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return;

   _dispvars->fd = open("/dev/fb0", O_RDWR);	
   _dispvars->fbp = (char *)mmap(0, _dispvars->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, _dispvars->fd, 0);

   /* Restore console screen contents. */
   memcpy((char*)_dispvars->fbp, (char*)_dispvars->screen_bck, _dispvars->screensize);
   free(_dispvars->screen_bck);

   /* Unmap and close. */
   munmap(&_dispvars->fd, _dispvars->screensize);
   close (_dispvars->fd);

   /* Restore cursor blinking. */
   system("setterm -cursor on");
}

static void vsync_callback(DISPMANX_UPDATE_HANDLE_T u, void *data)
{
   struct dispmanx_video *_dispvars = data;

   /* Changing the page to write must be done before the signaling
    * so we have the right page in nextPage when update_main continues */
   if (_dispvars->nextPage == &_dispvars->pages[0])	
      _dispvars->nextPage = &_dispvars->pages[1];
   else 
      _dispvars->nextPage = &_dispvars->pages[0];

   /* These two things must be isolated "atomically" to avoid getting 
    * a false positive in the pending_mutex test in update_main. */ 
   slock_lock(_dispvars->pending_mutex);
   
   _dispvars->pageflip_pending = false;	
   scond_signal(_dispvars->vsync_condition);
  
   slock_unlock(_dispvars->pending_mutex);
}

static void dispmanx_free_main_resources(void *data)
{
   int i;	
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return;

   _dispvars->update = vc_dispmanx_update_start(0);

   for (i = 0; i < NUMPAGES; i++) 
      vc_dispmanx_resource_delete(_dispvars->pages[i].resource);

   vc_dispmanx_element_remove(_dispvars->update, _dispvars->element);
   vc_dispmanx_update_submit_sync(_dispvars->update);		
}

static bool dispmanx_setup_scale(void *data, unsigned width,
      unsigned height, unsigned pitch)
{
   int i, dst_ypos;
   VC_DISPMANX_ALPHA_T layerAlpha;    
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return false;

   /* Since internal frame resolution seems to have changed, we change the 
    * internal frame resolution in use, and call dispmanx_setup_scale()
    * again to set the rects, etc. */
   _dispvars->width = width;
   _dispvars->height = height;

   /* Total pitch, including things the cores 
    * render between "visible" scanlines. */
   _dispvars->pitch = pitch;

   /* The "visible" width obtained from the core pitch. */
   _dispvars->visible_width = pitch / _dispvars->bytes_per_pixel;

   dispmanx_free_main_resources(_dispvars);
   vc_dispmanx_display_get_info(_dispvars->display, &(_dispvars->amode));

   /* We choose the pixel format depending on the bpp of the frame. */
   switch (_dispvars->bytes_per_pixel)
   {
      case 2:	
         _dispvars->pixFormat = VC_IMAGE_RGB565;	       
         break;
      case 4:	
         _dispvars->pixFormat = VC_IMAGE_XRGB8888;	       
         break;
      default:
         RARCH_ERR("video_dispmanx: wrong pixel format\n");
         return NULL;
   }	

   /* Transparency disabled */
   layerAlpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   layerAlpha.opacity = 255;
   layerAlpha.mask = 0;
   _dispvars->alpha = &layerAlpha;	

   switch (g_settings.video.aspect_ratio_idx)
   {
      case ASPECT_RATIO_4_3: 
         _dispvars->aspect = (float)4 / (float)3;
         break;
      case ASPECT_RATIO_16_9: 
         _dispvars->aspect = (float)16 / (float)9;
         break;
      case ASPECT_RATIO_16_10: 
         _dispvars->aspect = (float)16 / (float)10;
         break;
      case ASPECT_RATIO_16_15: 
         _dispvars->aspect = (float)16 / (float)15;
         break;
      case ASPECT_RATIO_CORE: 
         _dispvars->aspect = (float)_dispvars->width / (float)_dispvars->height;
         break;
      default: 
         _dispvars->aspect = (float)_dispvars->width / (float)_dispvars->height;
         break;
   }    

   int dst_width = _dispvars->amode.height * _dispvars->aspect;	
	   
   /* If we obtain a scaled image width that is bigger than the physical screen width,
   * then we keep the physical screen width as our maximun width. */
#if 0
   if (dst_width > _dispvars->amode.width) 
   dst_width = _dispvars->amode.width;
#endif
   dst_ypos = (_dispvars->amode.width - dst_width) / 2; 

   vc_dispmanx_rect_set(&(_dispvars->dstRect), dst_ypos, 0, 
	   dst_width, _dispvars->amode.height);

   /* We configure the rects now. */
   vc_dispmanx_rect_set(&(_dispvars->bmpRect), 0, 0, _dispvars->width, _dispvars->height);	
   vc_dispmanx_rect_set(&(_dispvars->srcRect), 0, 0, _dispvars->width << 16, _dispvars->height << 16);	

   /* We create as many resources as pages */
   for (i = 0; i < NUMPAGES; i++)
      _dispvars->pages[i].resource = vc_dispmanx_resource_create(_dispvars->pixFormat, 
	 _dispvars->visible_width, _dispvars->height, &(_dispvars->vcImagePtr));

   /* Add element. */
   _dispvars->update = vc_dispmanx_update_start(0);

   _dispvars->element = vc_dispmanx_element_add(_dispvars->update, _dispvars->display, 0,
	   &(_dispvars->dstRect), _dispvars->pages[0].resource, &(_dispvars->srcRect), 
	   DISPMANX_PROTECTION_NONE, _dispvars->alpha, 0, (DISPMANX_TRANSFORM_T)0);

   vc_dispmanx_update_submit_sync(_dispvars->update);		

   return true;
}

static void dispmanx_update_main(void *data, const void *frame)
{
   struct dispmanx_video *_dispvars = data;

   /* Wait until last issued flip completes to get the free page 
    * that the vsync cb will put in nextPage. Also, dispmanx doesn't 
    * support issuing more than one pageflip.*/
   slock_lock(_dispvars->pending_mutex);
   if (_dispvars->pageflip_pending)
   {
      scond_wait(_dispvars->vsync_condition, _dispvars->pending_mutex);
   }
   slock_unlock(_dispvars->pending_mutex);
   
   /* Frame blitting */
   vc_dispmanx_resource_write_data(_dispvars->nextPage->resource, _dispvars->pixFormat,
         _dispvars->pitch, (void *)frame, &(_dispvars->bmpRect));

   /* Issue a page flip that will be done at the next vsync. */
   _dispvars->update = vc_dispmanx_update_start(0);

   vc_dispmanx_element_change_source(_dispvars->update, _dispvars->element,
         _dispvars->nextPage->resource);

   vc_dispmanx_update_submit(_dispvars->update, vsync_callback, (void*)_dispvars);

   slock_lock(_dispvars->pending_mutex);
   _dispvars->pageflip_pending = true;	
   slock_unlock(_dispvars->pending_mutex);
}

static void *dispmanx_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   int i; 
   struct dispmanx_video *_dispvars = calloc(1, sizeof(struct dispmanx_video));

   if (!_dispvars)
      return NULL;

   _dispvars->bytes_per_pixel  = video->rgb32 ? 4 : 2;
   _dispvars->screen           = 0;
   _dispvars->vcImagePtr       = 0;
   _dispvars->pageflip_pending = false;	
   _dispvars->currentPage      = NULL;
   _dispvars->pages            = calloc(NUMPAGES, sizeof(struct dispmanx_page));
   _dispvars->menu_active      = false;
   _dispvars->nextPage         = &_dispvars->pages[0];
 
   if (!_dispvars->pages)
   {
      free(_dispvars);
      return NULL;
   }

   /* Initialize the rest of the mutexes and conditions. */
   _dispvars->vsync_condition  = scond_new();
   _dispvars->pending_mutex    = slock_new();

   bcm_host_init();

   _dispvars->display = vc_dispmanx_display_open(_dispvars->screen);

   if (input && input_data)
      *input = NULL;

   return _dispvars;
}

static bool dispmanx_gfx_frame(void *data, const void *frame, unsigned width,
      unsigned height, unsigned pitch, const char *msg)
{
   struct dispmanx_video *_dispvars = data;

   /* Check if neither menu nor core framebuffer is to be displayed. */
   if (!_dispvars->menu_active && !frame)
      return true;

   if (width != _dispvars->width || height != _dispvars->height)
   {
       /* Sanity check. */
       if (width == 0 || height == 0)
       return true;

       RARCH_LOG("video_dispmanx: internal frame resolution changed by core\n");
       
       if (!dispmanx_setup_scale(_dispvars, width, height, pitch))
       {
	  RARCH_ERR("video_dispmanx: frame resolution set failed\n");
	  return false;
       }
 
       dispmanx_blank_console (_dispvars);
   }

   if (_dispvars->menu_active)
   {
      char buf[128];
      video_monitor_get_fps(buf, sizeof(buf), NULL, 0);
   }

   /* Update main game screen: locate free page, blit and flip. */
   dispmanx_update_main(_dispvars, frame);

   return true;
}

static void dispmanx_free_menu_resources (void *data)
{
   struct dispmanx_video *_dispvars = data;
   
   if (!_dispvars)
      return;

   _dispvars->update = vc_dispmanx_update_start(0);

   vc_dispmanx_resource_delete(_dispvars->menu_resource);
   vc_dispmanx_element_remove(_dispvars->update, _dispvars->menu_element);

   vc_dispmanx_update_submit_sync(_dispvars->update);		

   _dispvars->menu_width = 0;
   _dispvars->menu_height = 0;
}

static void dispmanx_set_texture_enable(void *data, bool state, bool full_screen)
{
   struct dispmanx_video *_dispvars = data;

   if (_dispvars)
      _dispvars->menu_active = state;
   if (!_dispvars->menu_active)
      dispmanx_free_menu_resources(_dispvars);
}

static void dispmanx_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   struct dispmanx_video *_dispvars = data;
   
   if (!_dispvars)
      return;

   /* If we're entering the menu in this frame, 
    * we must setup rects, resources and menu element. */
   if (width != _dispvars->menu_width || height != _dispvars->menu_height)
   {
      int i, dst_width, dst_ypos;
      VC_DISPMANX_ALPHA_T layerAlpha;

      /* Sanity check */
      if (width == 0 || height == 0)
         return;

      _dispvars->menu_width = width;
      _dispvars->menu_height = height;

      _dispvars->menu_pitch = width * (rgb32 ? 4 : 2);

      _dispvars->menu_pixFormat = VC_IMAGE_RGBA16;	       
      _dispvars->menu_flip_page = 0;	

      /* Menu transparency has a fixed value for now. */
      layerAlpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
#if 0
      layerAlpha.opacity = (unsigned char)(255.0f * alpha);
#endif
      layerAlpha.opacity = 210;
      layerAlpha.mask = 0;
      _dispvars->menu_alpha = &layerAlpha;	

      dst_width = _dispvars->amode.height * _dispvars->aspect;	

      /* If we obtain a scaled image width that is 
       * bigger than the physical screen width,
       * then we keep the physical screen width as our maximun width. */
#if 0
      if (dst_width > _dispvars->amode.width) 
      dst_width = _dispvars->amode.width;
#endif
      dst_ypos  = (_dispvars->amode.width - dst_width) / 2; 
      vc_dispmanx_rect_set(&(_dispvars->menu_dstRect), dst_ypos, 0, 
            dst_width, _dispvars->amode.height);

      /* We configure the rects now. */
      vc_dispmanx_rect_set(&(_dispvars->menu_bmpRect), 0, 0, _dispvars->menu_width, _dispvars->menu_height);	
      vc_dispmanx_rect_set(&(_dispvars->menu_srcRect), 0, 0, _dispvars->menu_width << 16, _dispvars->menu_height << 16);	

      /* Create resource for the menu element. */
      _dispvars->menu_resource = vc_dispmanx_resource_create(_dispvars->menu_pixFormat, 
            _dispvars->menu_width, _dispvars->menu_height, &(_dispvars->vcImagePtr));

      /* Add the menu element. */
      _dispvars->update = vc_dispmanx_update_start(0);

      _dispvars->menu_element = vc_dispmanx_element_add(_dispvars->update, _dispvars->display, 0,
            &(_dispvars->menu_dstRect), _dispvars->menu_resource, &(_dispvars->menu_srcRect), 
            DISPMANX_PROTECTION_NONE, _dispvars->menu_alpha, 0, (DISPMANX_TRANSFORM_T)0);

      vc_dispmanx_update_submit_sync(_dispvars->update);	
   }

   /* Frame blitting. */
   vc_dispmanx_resource_write_data(_dispvars->menu_resource,
         _dispvars->menu_pixFormat, _dispvars->menu_pitch, (void *)frame, &(_dispvars->menu_bmpRect));
 
   /* We don't need to flip buffers: one is enough for menu. We sync in gfx_frame, on the 
    * dispmanx_update_main() call, that is called with menu active, too. */
}

static void dispmanx_gfx_set_nonblock_state(void *data, bool state)
{
   struct dispmanx_video *vid = data;

   (void)data;
   (void)vid;

   /* TODO */
}

static bool dispmanx_gfx_alive(void *data)
{
   (void)data;
   return true; /* always alive */
}

static bool dispmanx_gfx_focus(void *data)
{
   (void)data;
   return true; /* fb device always has focus */
}

static void dispmanx_gfx_viewport_info(void *data, struct video_viewport *vp)
{
    struct dispmanx_video *vid = data;

    if (!vid)
	    return;

    vp->x = vp->y = 0;

    vp->width  = vp->full_width  = vid->width;
    vp->height = vp->full_height = vid->height;
}

static bool dispmanx_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool dispmanx_gfx_has_windowed(void *data)
{
   (void)data;

   return false;
}

static bool dispmanx_gfx_set_shader(void *data,
enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

static void dispmanx_gfx_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static bool dispmanx_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static const video_poke_interface_t dispmanx_poke_interface = {
  NULL, /* set_video_mode */
  NULL, /* set_filtering */
  NULL, /* get_video_output_size */
  NULL, /* get_video_output_prev */
  NULL, /* get_video_output_next */
#ifdef HAVE_FBO
  NULL, /* get_current_framebuffer */
#endif
  NULL, /* get_proc_address */
  NULL, /* dispmanx_set_aspect_ratio */
  NULL, /* dispmanx_apply_state_changes */
#ifdef HAVE_MENU
  dispmanx_set_texture_frame,
  dispmanx_set_texture_enable,
#endif
  NULL, /* dispmanx_set_osd_msg */
  NULL  /* dispmanx_show_mouse */
};

static void dispmanx_gfx_get_poke_interface(void *data,
	const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &dispmanx_poke_interface;
}

static void dispmanx_gfx_free(void *data)
{
   int i;
   struct dispmanx_video *_dispvars = data;

   if (!_dispvars)
      return;
  
   if (_dispvars->menu_active)
	dispmanx_free_menu_resources(_dispvars);

   dispmanx_free_main_resources(_dispvars);

   /* Close display and deinitialize. */
   vc_dispmanx_display_close(_dispvars->display);
   bcm_host_deinit();

   /* Destroy mutexes and conditions. */
   slock_free(_dispvars->pending_mutex);
   scond_free(_dispvars->vsync_condition);		

   if (_dispvars->pages)
      free (_dispvars->pages);
   _dispvars->pages = NULL;

   dispmanx_restore_console(_dispvars);
}

video_driver_t video_dispmanx = {
	dispmanx_gfx_init,
	dispmanx_gfx_frame,
	dispmanx_gfx_set_nonblock_state,
	dispmanx_gfx_alive,
	dispmanx_gfx_focus,
	dispmanx_gfx_suppress_screensaver,
	dispmanx_gfx_has_windowed,
	dispmanx_gfx_set_shader,
	dispmanx_gfx_free,
	"dispmanx",

	dispmanx_gfx_set_rotation,
	dispmanx_gfx_viewport_info,
	dispmanx_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
	NULL, /* overlay_interface */
#endif
	dispmanx_gfx_get_poke_interface
};
