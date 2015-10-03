/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "ctr_gu.h"
#include "ctr_sprite_shader_shbin.h"

#include "../../general.h"
#include "../../driver.h"
#include "../video_viewport.h"
#include "../video_monitor.h"

#include "retroarch.h"
#include "performance.h"
#include "retro_inline.h"

#define CTR_TOP_FRAMEBUFFER_WIDTH    400
#define CTR_TOP_FRAMEBUFFER_HEIGHT   240
#define CTR_GPU_FRAMEBUFFER         ((void*)0x1F119400)
#define CTR_GPU_DEPTHBUFFER         ((void*)0x1F370800)


typedef struct
{
   float v;
   float u;
   float y;
   float x;
} ctr_scale_vector_t;

typedef struct
{
   s16 x0, y0, x1, y1;
   s16 u, v;
} ctr_vertex_t;

typedef struct ctr_video
{
   struct
   {
      uint32_t* display_list;
      int display_list_size;
      void* texture_linear;
      void* texture_swizzled;
      int texture_width;
      int texture_height;
      ctr_scale_vector_t scale_vector;
      ctr_vertex_t* frame_coords;
   }menu;

   uint32_t* display_list;
   int display_list_size;
   void* texture_linear;
   void* texture_swizzled;
   int texture_width;
   int texture_height;

   ctr_scale_vector_t scale_vector;
   ctr_vertex_t* frame_coords;

   DVLB_s*         dvlb;
   shaderProgram_s shader;

   video_viewport_t vp;

   bool rgb32;
   bool vsync;
   bool smooth;
   bool menu_texture_enable;
   unsigned rotation;
   bool keep_aspect;
   bool should_resize;
   bool lcd_buttom_on;

   void* empty_framebuffer;
} ctr_video_t;

static INLINE void ctr_set_scale_vector(ctr_scale_vector_t* vec,
      int viewport_width, int viewport_height,
      int texture_width, int texture_height)
{
   vec->x = -2.0 / viewport_width;
   vec->y = -2.0 / viewport_height;
   vec->u =  1.0 / texture_width;
   vec->v = -1.0 / texture_height;
}

static INLINE void ctr_set_screen_coords(ctr_video_t * ctr)
{
   if (ctr->rotation == 0)
   {
      ctr->frame_coords->x0 = ctr->vp.x;
      ctr->frame_coords->y0 = ctr->vp.y;
      ctr->frame_coords->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y1 = ctr->vp.y + ctr->vp.height;
   }
   else if (ctr->rotation == 1) /* 90° */
   {
      ctr->frame_coords->x1 = ctr->vp.x;
      ctr->frame_coords->y0 = ctr->vp.y;
      ctr->frame_coords->x0 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y1 = ctr->vp.y + ctr->vp.height;
   }
   else if (ctr->rotation == 2) /* 180° */
   {
      ctr->frame_coords->x1 = ctr->vp.x;
      ctr->frame_coords->y1 = ctr->vp.y;
      ctr->frame_coords->x0 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y0 = ctr->vp.y + ctr->vp.height;
   }
   else /* 270° */
   {
      ctr->frame_coords->x0 = ctr->vp.x;
      ctr->frame_coords->y1 = ctr->vp.y;
      ctr->frame_coords->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y0 = ctr->vp.y + ctr->vp.height;
   }
}


static void ctr_update_viewport(ctr_video_t* ctr)
{
   int x                = 0;
   int y                = 0;
   float device_aspect  = ((float)ctr->vp.full_width) / ctr->vp.full_height;
   float width          = ctr->vp.full_width;
   float height         = ctr->vp.full_height;
   settings_t *settings = config_get_ptr();

   if (settings->video.scale_integer)
   {
      video_viewport_get_scaled_integer(&ctr->vp, ctr->vp.full_width,
            ctr->vp.full_height, video_driver_get_aspect_ratio(), ctr->keep_aspect);
      width  = ctr->vp.width;
      height = ctr->vp.height;
   }
   else if (ctr->keep_aspect)
   {
#if defined(HAVE_MENU)
      if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         struct video_viewport *custom = video_viewport_get_custom();

         if (custom)
         {
            x      = custom->x;
            y      = custom->y;
            width  = custom->width;
            height = custom->height;
         }
      }
      else
#endif
      {
         float delta;
         float desired_aspect = video_driver_get_aspect_ratio();

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x     = (int)roundf(width * (0.5f - delta));
            width = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y      = (int)roundf(height * (0.5f - delta));
            height = (unsigned)roundf(2.0f * height * delta);
         }
      }

      ctr->vp.x      = x;
      ctr->vp.y      = y;
      ctr->vp.width  = width;
      ctr->vp.height = height;
   }
   else
   {
      ctr->vp.x = ctr->vp.y = 0;
      ctr->vp.width = width;
      ctr->vp.height = height;
   }

   ctr_set_screen_coords(ctr);

   ctr->should_resize = false;

}

static void* ctr_init(const video_info_t* video,
      const input_driver_t** input, void** input_data)
{
   void* ctrinput = NULL;
   ctr_video_t* ctr = (ctr_video_t*)linearAlloc(sizeof(ctr_video_t));

   if (!ctr)
      return NULL;

//   gfxInitDefault();
//   gfxSet3D(false);

   memset(ctr, 0, sizeof(ctr_video_t));


   ctr->vp.x                = 0;
   ctr->vp.y                = 0;
   ctr->vp.width            = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->vp.height           = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->vp.full_width       = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->vp.full_height      = CTR_TOP_FRAMEBUFFER_HEIGHT;


   ctr->display_list_size = 0x40000;
   ctr->display_list = linearAlloc(ctr->display_list_size * sizeof(uint32_t));
   GPU_Reset(NULL, ctr->display_list, ctr->display_list_size);

   ctr->texture_width = 512;
   ctr->texture_height = 512;
   ctr->texture_linear =
         linearMemAlign(ctr->texture_width * ctr->texture_height * sizeof(uint32_t), 128);
   ctr->texture_swizzled =
         linearMemAlign(ctr->texture_width * ctr->texture_height * sizeof(uint32_t), 128);

   ctr->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
   ctr->frame_coords->x0 = 0;
   ctr->frame_coords->y0 = 0;
   ctr->frame_coords->x1 = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->frame_coords->y1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->frame_coords->u = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->frame_coords->v = CTR_TOP_FRAMEBUFFER_HEIGHT;
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->frame_coords, sizeof(ctr_vertex_t));

   ctr->menu.texture_width = 512;
   ctr->menu.texture_height = 512;
   ctr->menu.texture_linear =
         linearMemAlign(ctr->texture_width * ctr->texture_height * sizeof(uint16_t), 128);
   ctr->menu.texture_swizzled =
         linearMemAlign(ctr->texture_width * ctr->texture_height * sizeof(uint16_t), 128);

   ctr->menu.frame_coords = linearAlloc(sizeof(ctr_vertex_t));

   ctr->menu.frame_coords->x0 = 40;
   ctr->menu.frame_coords->y0 = 0;
   ctr->menu.frame_coords->x1 = CTR_TOP_FRAMEBUFFER_WIDTH - 40;
   ctr->menu.frame_coords->y1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->menu.frame_coords->u = CTR_TOP_FRAMEBUFFER_WIDTH - 80;
   ctr->menu.frame_coords->v = CTR_TOP_FRAMEBUFFER_HEIGHT;
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->menu.frame_coords, sizeof(ctr_vertex_t));

   ctr_set_scale_vector(&ctr->scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->texture_width, ctr->texture_height);
   ctr_set_scale_vector(&ctr->menu.scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->menu.texture_width, ctr->menu.texture_height);

   ctr->dvlb = DVLB_ParseFile((u32*)ctr_sprite_shader_shbin, ctr_sprite_shader_shbin_size);
   ctrGuSetVshGsh(&ctr->shader, ctr->dvlb, 2, 2);
   shaderProgramUse(&ctr->shader);

   GPU_SetViewport(VIRT_TO_PHYS(CTR_GPU_DEPTHBUFFER),
                   VIRT_TO_PHYS(CTR_GPU_FRAMEBUFFER),
                   0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT, CTR_TOP_FRAMEBUFFER_WIDTH);

//      GPU_SetViewport(NULL,
//                      VIRT_TO_PHYS(CTR_GPU_FRAMEBUFFER),
//                      0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT, CTR_TOP_FRAMEBUFFER_WIDTH);

   GPU_DepthMap(-1.0f, 0.0f);
   GPU_SetFaceCulling(GPU_CULL_NONE);
   GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
   GPU_SetStencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
   GPU_SetBlendingColor(0, 0, 0, 0);
//      GPU_SetDepthTestAndWriteMask(true, GPU_GREATER, GPU_WRITE_ALL);
   GPU_SetDepthTestAndWriteMask(false, GPU_ALWAYS, GPU_WRITE_ALL);
   //   GPU_SetDepthTestAndWriteMask(true, GPU_ALWAYS, GPU_WRITE_ALL);

   GPUCMD_AddMaskedWrite(GPUREG_0062, 0x1, 0);
   GPUCMD_AddWrite(GPUREG_0118, 0);

   GPU_SetAlphaBlending(GPU_BLEND_ADD, GPU_BLEND_ADD,
                        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
   GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);

   GPU_SetTextureEnable(GPU_TEXUNIT0);

   GPU_SetTexEnv(0,
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0),
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0),
                 GPU_TEVOPERANDS(0, 0, 0),
                 GPU_TEVOPERANDS(0, 0, 0),
                 GPU_MODULATE, GPU_MODULATE,
                 0xFFFFFFFF);

   GPU_SetTexEnv(1, GPU_PREVIOUS,GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(2, GPU_PREVIOUS,GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(3, GPU_PREVIOUS,GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(4, GPU_PREVIOUS,GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(5, GPU_PREVIOUS,GPU_PREVIOUS, 0, 0, 0, 0, 0);

   ctrGuSetAttributeBuffers(2,
                            VIRT_TO_PHYS(ctr->menu.frame_coords),
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
                            CTRGU_ATTRIBFMT(GPU_SHORT, 2) << 4,
                            sizeof(ctr_vertex_t));
   GPUCMD_Finalize();
   ctrGuFlushAndRun(true);
   gspWaitForEvent(GSPEVENT_P3D, false);

   if (input && input_data)
   {
      ctrinput = input_ctr.init();
      *input = ctrinput ? &input_ctr : NULL;
      *input_data = ctrinput;
   }

   ctr->keep_aspect   = true;
   ctr->should_resize = true;
   ctr->smooth        = true;
   ctr->lcd_buttom_on = true;

   ctr->empty_framebuffer = linearAlloc(320 * 240 * 2);
   memset(ctr->empty_framebuffer, 0, 320 * 240 * 2);

   driver_set_refresh_rate((32730.0 * 8192.0) / 4481134.0);

   return ctr;
}

static bool ctr_frame(void* data, const void* frame,
      unsigned width, unsigned height, 
      uint64_t frame_count,
      unsigned pitch, const char* msg)
{
   uint32_t diff;
   static uint64_t currentTick,lastTick;
   ctr_video_t       *ctr  = (ctr_video_t*)data;
   settings_t   *settings  = config_get_ptr();
   static float        fps = 0.0;
   static int total_frames = 0;
   static int       frames = 0;
   static struct retro_perf_counter ctrframe_f = {0};
   uint32_t state_tmp;
   touchPosition state_tmp_touch;

   extern bool select_pressed;

   if (!width || !height)
   {
      gspWaitForEvent(GSPEVENT_VBlank0, true);
      return true;
   }

   if(!aptMainLoop())
   {
      event_command(EVENT_CMD_QUIT);
      return true;
   }

   if (select_pressed)
   {
      event_command(EVENT_CMD_QUIT);
      return true;
   }

   state_tmp = hidKeysDown();
   hidTouchRead(&state_tmp_touch);
   if((state_tmp & KEY_TOUCH) && (state_tmp_touch.py < 120))
   {
      Handle lcd_handle;
      u8 not_2DS;
      extern PrintConsole* currentConsole;

      gfxBottomFramebuffers[0] = ctr->lcd_buttom_on ? (u8*)ctr->empty_framebuffer:
                                                      (u8*)currentConsole->frameBuffer;

      CFGU_GetModelNintendo2DS(&not_2DS);
      if(not_2DS && srvGetServiceHandle(&lcd_handle, "gsp::Lcd") >= 0)
      {
         u32 *cmdbuf = getThreadCommandBuffer();
         cmdbuf[0] = ctr->lcd_buttom_on? 0x00120040:  0x00110040;
         cmdbuf[1] = 2;
         svcSendSyncRequest(lcd_handle);
         svcCloseHandle(lcd_handle);
      }

      ctr->lcd_buttom_on = !ctr->lcd_buttom_on;
   }

   svcWaitSynchronization(gspEvents[GSPEVENT_P3D], 20000000);
   svcClearEvent(gspEvents[GSPEVENT_P3D]);
   svcWaitSynchronization(gspEvents[GSPEVENT_PPF], 20000000);
   svcClearEvent(gspEvents[GSPEVENT_PPF]);

   frames++;

   if (ctr->vsync)
      svcWaitSynchronization(gspEvents[GSPEVENT_VBlank0], U64_MAX);

   svcClearEvent(gspEvents[GSPEVENT_VBlank0]);

   currentTick = svcGetSystemTick();
   diff        = currentTick - lastTick;
   if(diff > CTR_CPU_TICKS_PER_SECOND)
   {
      fps = (float)frames * ((float) CTR_CPU_TICKS_PER_SECOND / (float) diff);
      lastTick = currentTick;
      frames = 0;
   }

   printf("fps: %8.4f frames: %i\r", fps, total_frames++);
   fflush(stdout);

   rarch_perf_init(&ctrframe_f, "ctrframe_f");
   retro_perf_start(&ctrframe_f);

   if (ctr->should_resize)
      ctr_update_viewport(ctr);

   ctrGuSetMemoryFill(true, (u32*)CTR_GPU_FRAMEBUFFER, 0x00000000,
                    (u32*)(CTR_GPU_FRAMEBUFFER + CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT * sizeof(uint32_t)),
                    0x201, (u32*)CTR_GPU_DEPTHBUFFER, 0x00000000,
                    (u32*)(CTR_GPU_DEPTHBUFFER + CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT * sizeof(uint32_t)),
                    0x201);

   GPUCMD_SetBufferOffset(0);

   if (width > ctr->texture_width)
      width = ctr->texture_width;
   if (height > ctr->texture_height)
      height = ctr->texture_height;

   if(frame)
   {
      if(((((u32)(frame)) >= 0x14000000 && ((u32)(frame)) < 0x40000000)) /* frame in linear memory */
         && !((u32)frame & 0x7F)                                         /* 128-byte aligned */
         && !((pitch) & 0xF))                                            /* 16-byte aligned */
      {
         /* can copy the buffer directly with the GPU */
         ctrGuCopyImage(false, frame, pitch / 2, height, CTRGU_RGB565, false,
                        ctr->texture_swizzled, ctr->texture_width, CTRGU_RGB565,  true);
      }
      else
      {
         int i;
         uint16_t      *dst = (uint16_t*)ctr->texture_linear;
         const uint8_t *src = frame;

         for (i = 0; i < height; i++)
         {
            memcpy(dst, src, width * sizeof(uint16_t));
            dst += ctr->texture_width;
            src += pitch;
         }
         GSPGPU_FlushDataCache(NULL, ctr->texture_linear,
                               ctr->texture_width * ctr->texture_height * sizeof(uint16_t));

         ctrGuCopyImage(false, ctr->texture_linear, ctr->texture_width, ctr->menu.texture_height, CTRGU_RGB565, false,
                        ctr->texture_swizzled, ctr->texture_width, CTRGU_RGB565,  true);

      }

   }


   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->texture_swizzled), ctr->texture_width, ctr->texture_height,
                  (ctr->smooth? GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)  | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
                              : GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST)) |
                  GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                  GPU_RGB565);

   ctr->frame_coords->u = width;
   ctr->frame_coords->v = height;
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->frame_coords, sizeof(ctr_vertex_t));

   ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->frame_coords));
   ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->scale_vector, 1);
   GPU_DrawArray(GPU_UNKPRIM, 0, 1);

   if (ctr->menu_texture_enable)
   {

      GSPGPU_FlushDataCache(NULL, ctr->menu.texture_linear,
                            ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t));

      ctrGuCopyImage(false, ctr->menu.texture_linear, ctr->menu.texture_width, ctr->menu.texture_height, CTRGU_RGBA4444,false,
                     ctr->menu.texture_swizzled, ctr->menu.texture_width, CTRGU_RGBA4444,  true);

      ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->menu.texture_swizzled), ctr->menu.texture_width, ctr->menu.texture_height,
                     GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
                     GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                     GPU_RGBA4);


      ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->menu.frame_coords));
      ctrGuSetVertexShaderFloatUniform(1, (float*)&ctr->menu.scale_vector, 1);
      GPU_DrawArray(GPU_UNKPRIM, 0, 1);
   }

   GPU_FinishDrawing();
   GPUCMD_Finalize();
   ctrGuFlushAndRun(true);

   ctrGuDisplayTransfer(true, CTR_GPU_FRAMEBUFFER, 240,400, CTRGU_RGBA8,
                        gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 240,400,CTRGU_RGB8, CTRGU_MULTISAMPLE_NONE);

   gfxSwapBuffersGpu();
   retro_perf_stop(&ctrframe_f);

   return true;
}

static void ctr_set_nonblock_state(void* data, bool toggle)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->vsync = !toggle;
}

static bool ctr_alive(void* data)
{
   (void)data;
   return true;
}

static bool ctr_focus(void* data)
{
   (void)data;
   return true;
}

static bool ctr_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool ctr_has_windowed(void* data)
{
   (void)data;
   return false;
}

static void ctr_free(void* data)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   shaderProgramFree(&ctr->shader);
   DVLB_Free(ctr->dvlb);
   linearFree(ctr->display_list);
   linearFree(ctr->texture_linear);
   linearFree(ctr->texture_swizzled);
   linearFree(ctr->frame_coords);
   linearFree(ctr->menu.texture_linear);
   linearFree(ctr->menu.texture_swizzled);
   linearFree(ctr->menu.frame_coords);
   linearFree(ctr->empty_framebuffer);
   linearFree(ctr);
   //   gfxExit();
}

static void ctr_set_texture_frame(void* data, const void* frame, bool rgb32,
                                  unsigned width, unsigned height, float alpha)
{
   int i;
   ctr_video_t* ctr = (ctr_video_t*)data;
   uint16_t* dst = (uint16_t*)ctr->menu.texture_linear;
   const uint16_t* src = frame;
   int line_width = width;

   (void)rgb32;
   (void)alpha;

   if (line_width > ctr->menu.texture_width)
      line_width = ctr->menu.texture_width;

   if (height > (unsigned)ctr->menu.texture_height)
      height = (unsigned)ctr->menu.texture_height;

   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, line_width * sizeof(uint16_t));
      dst += ctr->menu.texture_width;
      src += width;
   }

   ctr->menu.frame_coords->x0 = (CTR_TOP_FRAMEBUFFER_WIDTH - width) / 2;
   ctr->menu.frame_coords->y0 = (CTR_TOP_FRAMEBUFFER_HEIGHT - height) / 2;
   ctr->menu.frame_coords->x1 = ctr->menu.frame_coords->x0 + width;
   ctr->menu.frame_coords->y1 = ctr->menu.frame_coords->y0 + height;
   ctr->menu.frame_coords->u = width;
   ctr->menu.frame_coords->v = height;
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->menu.frame_coords, sizeof(ctr_vertex_t));
}

static void ctr_set_texture_enable(void* data, bool state, bool full_screen)
{
   (void) full_screen;

   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->menu_texture_enable = state;
}


static void ctr_set_rotation(void* data, unsigned rotation)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   ctr->rotation = rotation;
   ctr->should_resize = true;
}
static void ctr_set_filtering(void* data, unsigned index, bool smooth)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->smooth = smooth;
}

static void ctr_set_aspect_ratio(void* data, unsigned aspectratio_index)
{
   ctr_video_t *ctr = (ctr_video_t*)data;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   switch (aspectratio_index)
   {
      case ASPECT_RATIO_SQUARE:
         video_viewport_set_square_pixel(
               av_info->geometry.base_width,
               av_info->geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         video_viewport_set_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_viewport_set_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(aspectratio_lut[aspectratio_index].value);

   ctr->keep_aspect = true;
   ctr->should_resize = true;
}

static void ctr_apply_state_changes(void* data)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->should_resize = true;

}

static void ctr_viewport_info(void* data, struct video_viewport* vp)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      *vp = ctr->vp;
}

static const video_poke_interface_t ctr_poke_interface =
{
   NULL,
   ctr_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL,
   ctr_set_aspect_ratio,
   ctr_apply_state_changes,
#ifdef HAVE_MENU
   ctr_set_texture_frame,
   ctr_set_texture_enable,
#endif
   NULL,
   NULL,
   NULL
};

static void ctr_get_poke_interface(void* data,
                                   const video_poke_interface_t** iface)
{
   (void)data;
   *iface = &ctr_poke_interface;
}

static bool ctr_read_viewport(void* data, uint8_t* buffer)
{
   (void)data;
   (void)buffer;
   return false;
}

static bool ctr_set_shader(void* data,
                           enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_ctr =
{
   ctr_init,
   ctr_frame,
   ctr_set_nonblock_state,
   ctr_alive,
   ctr_focus,
   ctr_suppress_screensaver,
   ctr_has_windowed,
   ctr_set_shader,
   ctr_free,
   "ctr",
   NULL, /* set_viewport */
   ctr_set_rotation,
   ctr_viewport_info,
   ctr_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
   ctr_get_poke_interface
};
