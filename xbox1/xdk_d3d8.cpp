﻿/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifdef _XBOX
#include <xtl.h>
#include <xgraphics.h>
#endif

#include "../driver.h"
#include "xdk_d3d8.h"

#include "./../gfx/gfx_context.h"
#include "../console/retroarch_console.h"
#include "../general.h"
#include "../message.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static void check_window(xdk_d3d_video_t *d3d)
{
   bool quit, resize;

   gfx_ctx_check_window(&quit,
         &resize, NULL, NULL,
         d3d->frame_count);

   if (quit)
      d3d->quitting = true;
   else if (resize)
      d3d->should_resize = true;
}

static void xdk_d3d_free(void * data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
	   return;
#endif

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if (!d3d)
      return;

   d3d->d3d_render_device->Release();
   d3d->d3d_device->Release();

   free(d3d);
}

static void xdk_d3d_set_viewport(bool force_full)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   // Get the "video mode"
   d3d->video_mode = XGetVideoFlags();

   // Set the viewport based on the current resolution
   int width, height;

   width  = d3d->d3dpp.BackBufferWidth;
   height = d3d->d3dpp.BackBufferHeight;

   int m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
   float m_zNear, m_zFar;

   m_viewport_x_temp = 0;
   m_viewport_y_temp = 0;
   m_viewport_width_temp = width;
   m_viewport_height_temp = height;

   m_zNear = 0.0f;
   m_zFar = 1.0f;

   if (!force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)width / height;
      float delta;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
      	 m_viewport_x_temp = g_console.viewports.custom_vp.x;
      	 m_viewport_y_temp = g_console.viewports.custom_vp.y;
      	 m_viewport_width_temp = g_console.viewports.custom_vp.width;
      	 m_viewport_height_temp = g_console.viewports.custom_vp.height;
      }
      else if (device_aspect > desired_aspect)
      {
         delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_x_temp = (int)(width * (0.5 - delta));
         m_viewport_width_temp = (int)(2.0 * width * delta);
         width = (unsigned)(2.0 * width * delta);
      }
      else
      {
         delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         m_viewport_y_temp = (int)(height * (0.5 - delta));
         m_viewport_height_temp = (int)(2.0 * height * delta);
         height = (unsigned)(2.0 * height * delta);
      }
   }

   D3DVIEWPORT vp = {0};
   vp.Width  = m_viewport_width_temp;
   vp.Height = m_viewport_height_temp;
   vp.X      = m_viewport_x_temp;
   vp.Y      = m_viewport_y_temp;
   vp.MinZ   = m_zNear;
   vp.MaxZ   = m_zFar;
   d3d->d3d_render_device->SetViewport(&vp);

   //if(gl->overscan_enable && !force_full)
   //{
   //	m_left = -gl->overscan_amount/2;
   //	m_right = 1 + gl->overscan_amount/2;
   //	m_bottom = -gl->overscan_amount/2;
   //}
}

static void xdk_d3d_set_rotation(void * data, unsigned orientation)
{
   (void)data;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   FLOAT angle;

   switch(orientation)
   {
      case ORIENTATION_NORMAL:
         angle = M_PI * 0 / 180;
	 break;
      case ORIENTATION_VERTICAL:
         angle = M_PI * 270 / 180;
         break;
      case ORIENTATION_FLIPPED:
         angle = M_PI * 180 / 180;
         break;
      case ORIENTATION_FLIPPED_ROTATED:
         angle = M_PI * 90 / 180;
         break;
   }

   D3DXMATRIX p_out;
   D3DXMatrixIdentity(&p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &p_out);

   d3d->should_resize = TRUE;
}

static void *xdk_d3d_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   if (driver.video_data)
      return driver.video_data;

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)calloc(1, sizeof(xdk_d3d_video_t));
   if (!d3d)
      return NULL;

   d3d->d3d_device = direct3d_create_ctx(D3D_SDK_VERSION);
   if (!d3d->d3d_device)
   {
      free(d3d);
	  OutputDebugString("RetroArch: Failed to create a D3D8 object!");
      return NULL;
   }

   memset(&d3d->d3dpp, 0, sizeof(d3d->d3dpp));

   // Get the "video mode"
   d3d->video_mode = XGetVideoFlags();

   // Check if we are able to use progressive mode
   if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
        d3d->d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
    else
        d3d->d3dpp.Flags = D3DPRESENTFLAG_INTERLACED;

    // Safe mode
   	d3d->d3dpp.BackBufferWidth	= 640;
	d3d->d3dpp.BackBufferHeight = 480;
	g_console.menus_hd_enable = false;

   // Only valid in PAL mode, not valid for HDTV modes!
   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
		if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
			d3d->d3dpp.FullScreen_RefreshRateInHz = 60;
		else
			d3d->d3dpp.FullScreen_RefreshRateInHz = 50;
		
		// Check for 16:9 mode (PAL REGION)
		if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
		{
			if(d3d->video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
			{	//60 Hz, 720x480i
				d3d->d3dpp.BackBufferWidth = 720;
				d3d->d3dpp.BackBufferHeight = 480;
			}
			else
			{	//50 Hz, 720x576i
				d3d->d3dpp.BackBufferWidth = 720;
				d3d->d3dpp.BackBufferHeight = 576;
			}
		}
   }
		else
   {
		// Check for 16:9 mode (NTSC REGIONS)
		if(d3d->video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
		{
				d3d->d3dpp.BackBufferWidth = 720;
				d3d->d3dpp.BackBufferHeight = 480;
		}
   }

		
	if(XGetAVPack() == XC_AV_PACK_HDTV)
	{
	   	if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_480p)
		{
			g_console.menus_hd_enable = false;
			d3d->d3dpp.BackBufferWidth	= 640;
			d3d->d3dpp.BackBufferHeight = 480;
			d3d->d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
		}

	   	else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_720p)
		{
			g_console.menus_hd_enable = true;
			d3d->d3dpp.BackBufferWidth	= 1280;
			d3d->d3dpp.BackBufferHeight = 720;
			d3d->d3dpp.Flags = D3DPRESENTFLAG_PROGRESSIVE;
		}

		else if(d3d->video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
		{
			g_console.menus_hd_enable = true;
			d3d->d3dpp.BackBufferWidth	= 1920;
			d3d->d3dpp.BackBufferHeight = 1080;
			d3d->d3dpp.Flags = D3DPRESENTFLAG_INTERLACED;
		}
	}


    if(d3d->d3dpp.BackBufferWidth > 640 && ((float)d3d->d3dpp.BackBufferHeight / (float)d3d->d3dpp.BackBufferWidth != 0.75) ||
                            ((d3d->d3dpp.BackBufferWidth == 720) && (d3d->d3dpp.BackBufferHeight == 576))) // 16:9
    {
        d3d->d3dpp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
    }


   // no letterboxing in 4:3 mode (if widescreen is unsupported
   d3d->d3dpp.BackBufferFormat						= D3DFMT_A8R8G8B8;
   d3d->d3dpp.FullScreen_PresentationInterval		= video->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
   d3d->d3dpp.MultiSampleType						= D3DMULTISAMPLE_NONE;
   d3d->d3dpp.BackBufferCount						= 2;
   d3d->d3dpp.EnableAutoDepthStencil				= FALSE;
   d3d->d3dpp.SwapEffect							= D3DSWAPEFFECT_COPY; //D3DSWAPEFFECT_DISCARD;

   d3d->d3d_device->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3d->d3dpp, &d3d->d3d_render_device);

   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   // use an orthogonal matrix for the projection matrix
   D3DXMATRIX mat;
   D3DXMatrixOrthoOffCenterLH(&mat, 0,  d3d->d3dpp.BackBufferWidth ,  d3d->d3dpp.BackBufferHeight , 0, 0.0f, 1.0f);

   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &mat);

   // use an identity matrix for the world and view matrices
   D3DXMatrixIdentity(&mat);
   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &mat);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &mat);

   d3d->d3d_render_device->CreateTexture(512, 512, 1, 0, D3DFMT_LIN_X1R5G5B5, 0, &d3d->lpTexture);
   D3DLOCKED_RECT d3dlr;
   d3d->lpTexture->LockRect(0, &d3dlr, NULL, 0);
   memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
   d3d->lpTexture->UnlockRect(0);

   d3d->last_width = 512;
   d3d->last_height = 512;

   d3d->d3d_render_device->CreateVertexBuffer(4 * sizeof(DrawVerticeFormats), 
	   D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &d3d->vertex_buf);

   static const DrawVerticeFormats init_verts[] = {
      { -1.0f, -1.0f, 1.0f, 0.0f, 1.0f },
      {  1.0f, -1.0f, 1.0f, 1.0f, 1.0f },
      { -1.0f,  1.0f, 1.0f, 0.0f, 0.0f },
      {  1.0f,  1.0f, 1.0f, 1.0f, 0.0f },
   };

   BYTE *verts_ptr;
   d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
   memcpy(verts_ptr, init_verts, sizeof(init_verts));
   d3d->vertex_buf->Unlock();

   d3d->d3d_render_device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);

   // disable lighting
   d3d->d3d_render_device->SetRenderState(D3DRS_LIGHTING, FALSE);

   // disable culling
   d3d->d3d_render_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

   // disable z-buffer
   d3d->d3d_render_device->SetRenderState(D3DRS_ZENABLE, FALSE);

   D3DVIEWPORT vp = {0};
   vp.Width  = d3d->d3dpp.BackBufferWidth;
   vp.Height = d3d->d3dpp.BackBufferHeight;
   vp.MinZ   = 0.0f;
   vp.MaxZ   = 1.0f;
   d3d->d3d_render_device->SetViewport(&vp);

   if(g_console.viewports.custom_vp.width == 0)
      g_console.viewports.custom_vp.width = vp.Width;

   if(g_console.viewports.custom_vp.height == 0)
      g_console.viewports.custom_vp.height = vp.Height;

   xdk_d3d_set_rotation(d3d, g_console.screen_orientation);

   d3d->vsync = video->vsync;

	// load debug font (toggle option in later revisions ?)
#ifdef SHOW_DEBUG_INFO
	XFONT_OpenDefaultFont(&d3d->debug_font);
	d3d->debug_font->SetBkMode(XFONT_TRANSPARENT);
	d3d->debug_font->SetBkColor(D3DCOLOR_ARGB(100,0,0,0));
	d3d->debug_font->SetTextHeight(14);
	d3d->debug_font->SetTextAntialiasLevel(d3d->debug_font->GetTextAntialiasLevel());
#endif

   return d3d;
}

static bool xdk_d3d_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
#if 0
   if (!frame)
      return true;
#endif

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   bool menu_enabled = g_console.menu_enable;

   if (d3d->last_width != width || d3d->last_height != height) //240*160
   {

      D3DLOCKED_RECT d3dlr;

      d3d->lpTexture->LockRect(0, &d3dlr, NULL, 0);
      memset(d3dlr.pBits, 0, 512 * d3dlr.Pitch);
      d3d->lpTexture->UnlockRect(0);

      float tex_w = width;  // / 512.0f;
      float tex_h = height; // / 512.0f;

      DrawVerticeFormats verts[] = {
         { -1.0f, -1.0f, 1.0f, 0.0f,  tex_h },
         {  1.0f, -1.0f, 1.0f, tex_w, tex_h },
         { -1.0f,  1.0f, 1.0f, 0.0f,  0.0f },
         {  1.0f,  1.0f, 1.0f, tex_w, 0.0f },
      };

      // Align texels and vertices (D3D9 quirk).
      for (unsigned i = 0; i < 4; i++)
      {
         verts[i].x -= 0.5f / 512.0f;
         verts[i].y += 0.5f / 512.0f;
      }

      BYTE *verts_ptr;
      d3d->vertex_buf->Lock(0, 0, &verts_ptr, 0);
      memcpy(verts_ptr, verts, sizeof(verts));
      d3d->vertex_buf->Unlock();

      d3d->last_width = width;
      d3d->last_height = height;
   }

   if (d3d->should_resize)
      xdk_d3d_set_viewport(false);

   d3d->frame_count++;

   d3d->d3d_render_device->SetTexture(0, d3d->lpTexture);

   D3DLOCKED_RECT d3dlr;

   d3d->lpTexture->LockRect(0, &d3dlr, NULL, 0);
   for (unsigned y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)d3dlr.pBits + y * d3dlr.Pitch;
      memcpy(out, in, width * sizeof(uint16_t));
   }
   d3d->lpTexture->UnlockRect(0);


   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MINFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_MAGFILTER, g_settings.video.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   d3d->d3d_render_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);

   D3DXMATRIX p_out;
   D3DXMatrixIdentity(&p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_WORLD, &p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_VIEW, &p_out);
   d3d->d3d_render_device->SetTransform(D3DTS_PROJECTION, &p_out);

   d3d->d3d_render_device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);
   d3d->d3d_render_device->SetStreamSource(0, d3d->vertex_buf, sizeof(DrawVerticeFormats));
   d3d->d3d_render_device->Clear(0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);

   d3d->d3d_render_device->BeginScene();
   d3d->d3d_render_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
   d3d->d3d_render_device->EndScene();


#ifdef SHOW_DEBUG_INFO
    static MEMORYSTATUS stat;
    GlobalMemoryStatus(&stat);
	d3d->d3d_render_device->GetBackBuffer(-1, D3DBACKBUFFER_TYPE_MONO, &d3d->pFrontBuffer);
	d3d->d3d_render_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &d3d->pBackBuffer);

	//Output memory usage
	d3d->debug_font->TextOut(d3d->pFrontBuffer, L"RetroArch XBOX1", (unsigned)-1, 30, 30 );
	d3d->debug_font->TextOut(d3d->pBackBuffer, L"RetroArch XBOX1", (unsigned)-1, 30, 30 );

	swprintf(d3d->buffer, L"%.2f MB free / %.2f MB total", stat.dwAvailPhys/(1024.0f*1024.0f), stat.dwTotalPhys/(1024.0f*1024.0f));
	d3d->debug_font->TextOut(d3d->pFrontBuffer, d3d->buffer, (unsigned)-1, 30, 50 );
	d3d->debug_font->TextOut(d3d->pBackBuffer, d3d->buffer, (unsigned)-1, 30, 50 );

	// FIXME: Add fps counter
	/*
	swprintf(buffer, L"%02d / %02d FPS", fps, IsPal ? 50 : 60);
	d3d->debug_font->TextOut(d3d->pFrontBuffer, d3d->buffer, (unsigned)-1, 30, 70 );
	d3d->debug_font->TextOut(d3d->pBackBuffer,	 d3d->buffer, (unsigned)-1, 30, 70 );
	*/

	d3d->pFrontBuffer->Release();
	d3d->pBackBuffer->Release();
#endif

   if(!d3d->block_swap)
      gfx_ctx_swap_buffers();

   return true;
}

static void xdk_d3d_set_nonblock_state(void *data, bool state)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;

   if(d3d->vsync)
   {
      RARCH_LOG("D3D Vsync => %s\n", state ? "off" : "on");
      gfx_ctx_set_swap_interval(state ? 0 : 1, TRUE);
   }
}

static bool xdk_d3d_alive(void *data)
{
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)data;
   check_window(d3d);
   return !d3d->quitting;
}

static bool xdk_d3d_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

static void xdk_d3d_start(void)
{
   video_info_t video_info = {0};

   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.fullscreen = true;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;

   driver.video_data = xdk_d3d_init(&video_info, NULL, NULL);

   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   gfx_ctx_set_swap_interval(d3d->vsync ? 1 : 0, false);
}

static void xdk_d3d_restart(void)
{
}

static void xdk_d3d_stop(void)
{
   void *data = driver.video_data;
   driver.video_data = NULL;
   xdk_d3d_free(data);
}

const video_driver_t video_xdk_d3d = {
   xdk_d3d_init,
   xdk_d3d_frame,
   xdk_d3d_set_nonblock_state,
   xdk_d3d_alive,
   xdk_d3d_focus,
   NULL,
   xdk_d3d_free,
   "xdk_d3d",
   xdk_d3d_start,
   xdk_d3d_stop,
   xdk_d3d_restart,
   xdk_d3d_set_rotation,
};
