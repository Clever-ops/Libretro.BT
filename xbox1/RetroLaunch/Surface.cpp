/**
 * Surreal 64 Launcher (C) 2003
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version. This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. To contact the
 * authors: email: buttza@hotmail.com, lantus@lantus-x.com
 *
 * Additional code and cleanups: Surreal64 CE Team (http://www.emuxtras.net)
 */

#include "Surface.h"
#include "Debug.h"

CSurface::CSurface()
{
	m_pTexture              = NULL;
	m_pVertexBuffer         = NULL;
	m_byOpacity             = 255;
	m_byR                   = 255;
	m_byG                   = 255;
	m_byB                   = 255;
	m_bLoaded               = false;
	m_x                     = 0;
	m_y                     = 0;
}

CSurface::CSurface(const string &szFilename)
{
	CSurface();
	Create(szFilename);
}

CSurface::~CSurface()
{
	Destroy();
}

bool CSurface::Create(const string &szFilename)
{
	if (m_bLoaded)
		Destroy();

	g_hResult = D3DXCreateTextureFromFileExA(g_video.m_pD3DDevice,                    // d3d device
	                                         ("D:\\" +  szFilename).c_str(),          // filename
	                                         D3DX_DEFAULT, D3DX_DEFAULT,              // width/height
	                                         D3DX_DEFAULT,                            // mipmaps
	                                         0,                                       // usage
	                                         D3DFMT_A8R8G8B8,                         // format
	                                         D3DPOOL_MANAGED,                         // memory class
	                                         D3DX_DEFAULT,                            // texture filter
	                                         D3DX_DEFAULT,                            // mipmapping
	                                         0,                                       // colorkey
	                                         &m_imageInfo,                            // image info
	                                         NULL,                                    // pallete
	                                         &m_pTexture);                            // texture

	if (FAILED(g_hResult))
	{
		g_debug.Print("Failed: D3DXCreateTextureFromFileExA()\n");
		return false;
	}

	// create a vertex buffer for the quad that will display the texture
	g_hResult = g_video.m_pD3DDevice->CreateVertexBuffer(4 * sizeof(CustomVertex),
	                                                   D3DUSAGE_WRITEONLY,
	                                                   D3DFVF_CUSTOMVERTEX,
	                                                   D3DPOOL_MANAGED, &m_pVertexBuffer);
	if (FAILED(g_hResult))
	{
		g_debug.Print("Failed: CreateVertexBuffer()\n");
		m_pTexture->Release();
		return false;
	}

	m_bLoaded = true;

	return true;
}

bool CSurface::Create(dword width, dword height)
{
	if (m_bLoaded)
		Destroy();

	g_hResult = g_video.m_pD3DDevice->CreateTexture(width, height, 1, 0,
	                                              D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
	                                              &m_pTexture);

	if (FAILED(g_hResult))
	{
		g_debug.Print("Failed: CreateTexture()\n");
		return false;
	}

	m_imageInfo.Width = width;
	m_imageInfo.Height = height;
	m_imageInfo.Format = D3DFMT_A8R8G8B8;

	// create a vertex buffer for the quad that will display the texture
	g_hResult = g_video.m_pD3DDevice->CreateVertexBuffer(4 * sizeof(CustomVertex),
	                                                   D3DUSAGE_WRITEONLY,
	                                                   D3DFVF_CUSTOMVERTEX,
	                                                   D3DPOOL_MANAGED, &m_pVertexBuffer);
	if (FAILED(g_hResult))
	{
		g_debug.Print("Failed: CreateVertexBuffer()\n");
		m_pTexture->Release();
		return false;
	}

	m_bLoaded = true;

	return true;
}

void CSurface::Destroy()
{
	// free the vertex buffer
	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = NULL;
	}

	// free the texture
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = NULL;
	}

	m_bLoaded = false;
}

bool CSurface::IsLoaded()
{
	return m_bLoaded;
}

bool CSurface::Render()
{
	return Render(m_x, m_y);
}

bool CSurface::Render(int x, int y)
{
	return Render(x, y, m_imageInfo.Width, m_imageInfo.Height);
}

bool CSurface::Render(int x, int y, dword w, dword h)
{
	if (m_pTexture == NULL || m_pVertexBuffer == NULL || m_bLoaded == false)
		return false;

	float fX = static_cast<float>(x);
	float fY = static_cast<float>(y);

	// create the new vertices
	CustomVertex newVerts[] =
	{
		// x,		y,			z,	  color,					    u ,v
		{fX,            fY,                     0.0f, D3DCOLOR_ARGB(m_byOpacity, m_byR, m_byG, m_byB), 0, 0},
		{fX + w,        fY,                     0.0f, D3DCOLOR_ARGB(m_byOpacity, m_byR, m_byG, m_byB), 1, 0},
		{fX + w,        fY + h,         0.0f, D3DCOLOR_ARGB(m_byOpacity, m_byR, m_byG, m_byB), 1, 1},
		{fX,            fY + h,         0.0f, D3DCOLOR_ARGB(m_byOpacity, m_byR, m_byG, m_byB), 0, 1}
	};

	// load the existing vertices
	CustomVertex *pCurVerts;

	g_hResult = m_pVertexBuffer->Lock(0, 0, (byte **)&pCurVerts, 0);

	if (FAILED(g_hResult))
	{
		g_debug.Print("Failed: m_pVertexBuffer->Lock()\n");
		return false;
	}
	// copy the new verts over the old verts
	memcpy(pCurVerts, newVerts, 4 * sizeof(CustomVertex));

	m_pVertexBuffer->Unlock();


	g_video.m_pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_video.m_pD3DDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
	g_video.m_pD3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	// also blend the texture with the set alpha value
	g_video.m_pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	g_video.m_pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	g_video.m_pD3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

	// draw the quad
	g_video.m_pD3DDevice->SetTexture(0, m_pTexture);
	g_video.m_pD3DDevice->SetStreamSource(0, m_pVertexBuffer, sizeof(CustomVertex));
	g_video.m_pD3DDevice->SetVertexShader(D3DFVF_CUSTOMVERTEX);
#ifdef _XBOX
	g_video.m_pD3DDevice->DrawPrimitive(D3DPT_QUADLIST, 0, 1);
#else
	//FIXME: vertices order !
	g_video.m_pD3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 2);
#endif
	return true;
}

void CSurface::SetOpacity(byte opacity)
{
	m_byOpacity = opacity;
}

void CSurface::SetTint(byte r, byte g, byte b)
{
	m_byR = r;
	m_byG = g;
	m_byB = b;
}

void CSurface::MoveTo(int x, int y)
{
	m_x = x;
	m_y = y;
}

dword CSurface::GetWidth()
{
	if (m_pTexture == NULL || m_pVertexBuffer == NULL)
		return 0;

	return m_imageInfo.Width;
}

dword CSurface::GetHeight()
{
	if (m_pTexture == NULL || m_pVertexBuffer == NULL)
		return 0;

	return m_imageInfo.Height;
}

byte CSurface::GetOpacity()
{
	return m_byOpacity;
}

IDirect3DTexture8 *CSurface::GetTexture()
{
	return m_pTexture;
}
