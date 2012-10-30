/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef D3DVIDEO_HPP__
#define D3DVIDEO_HPP__

#include "../../general.h"
#include "../../driver.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9core.h>
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#include <string>
#include <vector>
#include <memory>

class ConfigFile;
class RenderChain;

class D3DVideo
{
   public:
      D3DVideo(const video_info_t* info);
      bool frame(const void* frame,
            unsigned width, unsigned height, unsigned pitch,
            const char *msg);
      ~D3DVideo();

      bool alive();
      bool focus() const;
      void set_nonblock_state(bool state);
      void set_rotation(unsigned rot);
      void viewport_info(rarch_viewport &vp);
      bool read_viewport(uint8_t *buffer);
      void resize(unsigned new_width, unsigned new_height);

   private:

      WNDCLASSEX windowClass;
      HWND hWnd;
      IDirect3D9 *g_pD3D;
      IDirect3DDevice9 *dev;
      LPD3DXFONT font;

      void calculate_rect(unsigned width, unsigned height, bool keep, float aspect);
      void set_viewport(unsigned x, unsigned y, unsigned width, unsigned height);
      unsigned screen_width;
      unsigned screen_height;
      unsigned rotation;
      D3DVIEWPORT9 final_viewport;

      std::string cg_shader;

      void process();

      void init(const video_info_t &info);
      void init_base(const video_info_t &info);
      void make_d3dpp(const video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp);
      void deinit();
      RECT monitor_rect();
      video_info_t video_info;

      bool needs_restore;
      bool restore();

      CGcontext cgCtx;
      bool init_cg();
      void deinit_cg();

      void init_imports(ConfigFile &conf, const std::string &basedir);
      void init_luts(ConfigFile &conf, const std::string &basedir);
      void init_chain_singlepass(const video_info_t &video_info);
      void init_chain_multipass(const video_info_t &video_info);
      bool init_chain(const video_info_t &video_info);
      std::unique_ptr<RenderChain> chain;
      void deinit_chain();

      bool init_font();
      void deinit_font();
      RECT font_rect;
      RECT font_rect_shifted;
      uint32_t font_color;

      void update_title();
};

#endif

