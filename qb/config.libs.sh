check_switch '' C99 -std=gnu99 "Cannot find C99 compatible compiler."
check_switch '' NOUNUSED -Wno-unused-result
add_define MAKEFILE NOUNUSED "$HAVE_NOUNUSED"
check_switch '' NOUNUSED_VARIABLE -Wno-unused-variable
add_define MAKEFILE NOUNUSED_VARIABLE "$HAVE_NOUNUSED_VARIABLE"

# There are still broken 64-bit Linux distros out there. :)
[ -z "$CROSS_COMPILE" ] && [ -d /usr/lib64 ] && add_dirs LIBRARY /usr/lib64

[ -z "$CROSS_COMPILE" ] && [ -d /opt/local/lib ] && add_dirs LIBRARY /opt/local/lib

[ "${GLOBAL_CONFIG_DIR:-}" ] ||
{	case "$PREFIX" in
		/usr*) GLOBAL_CONFIG_DIR=/etc ;;
		*) GLOBAL_CONFIG_DIR="$PREFIX"/etc ;;
	esac
}

DYLIB=-ldl
CLIB=-lc
PTHREADLIB=-lpthread
SOCKETLIB=-lc
SOCKETHEADER=
INCLUDES='usr/include usr/local/include'
SORT='sort'
EXTRA_GL_LIBS=''
VC_PREFIX=''

if [ "$OS" = 'BSD' ]; then
   [ -d /usr/local/include ] && add_dirs INCLUDE /usr/local/include
   [ -d /usr/local/lib ] && add_dirs LIBRARY /usr/local/lib
   DYLIB=-lc;
elif [ "$OS" = 'Haiku' ]; then
   DYLIB=""
   CLIB=-lroot
   PTHREADLIB=-lroot
   SOCKETLIB=-lnetwork
   CFLAGS="$CFLAGS -D_BSD_SOURCE"
elif [ "$OS" = 'Win32' ]; then
   SOCKETLIB=-lws2_32
   SOCKETHEADER="#include <winsock2.h>"
   DYLIB=
elif [ "$OS" = 'Cygwin' ]; then
   die 1 'Error: Cygwin is not a supported platform. See https://bot.libretro.com/docs/compilation/windows/'
elif [ "$OS" = 'SunOS' ]; then
   SORT='gsort'
fi

add_define MAKEFILE DATA_DIR "$SHARE_DIR"
add_define MAKEFILE DYLIB_LIB "$DYLIB"

check_lib '' SYSTEMD -lsystemd sd_get_machine_names

if [ "$HAVE_VIDEOCORE" != "no" ]; then
   check_pkgconf VC_TEST bcm_host

   # use fallback if pkgconfig is not available
   if [ -z "$VC_TEST_LIBS" ]; then
      [ -d /opt/vc/lib ] && add_dirs LIBRARY /opt/vc/lib /opt/vc/lib/GL
      check_lib '' VIDEOCORE -lbcm_host bcm_host_init "-lvcos -lvchiq_arm"
   else
      HAVE_VIDEOCORE="$HAVE_VC_TEST"
   fi
fi

if [ "$HAVE_VIDEOCORE" = 'yes' ]; then
   HAVE_OPENGLES='auto'
   VC_PREFIX='brcm'
   INCLUDES="${INCLUDES} opt/vc/include"

   # use fallback if pkgconfig is not available
   if [ -z "$VC_TEST_LIBS" ]; then
      [ -d /opt/vc/include ] && add_dirs INCLUDE /opt/vc/include
      [ -d /opt/vc/include/interface/vcos/pthreads ] && add_dirs INCLUDE /opt/vc/include/interface/vcos/pthreads
      [ -d /opt/vc/include/interface/vmcs_host/linux ] && add_dirs INCLUDE /opt/vc/include/interface/vmcs_host/linux
      EXTRA_GL_LIBS="-lbrcmEGL -lbrcmGLESv2 -lbcm_host -lvcos -lvchiq_arm"
   fi
fi

if [ "$HAVE_7ZIP" = "yes" ]; then
   add_dirs INCLUDE ./deps/7zip
fi

if [ "$HAVE_PRESERVE_DYLIB" = "yes" ]; then
   die : 'Notice: Disabling dlclose() of shared objects for Valgrind support.'
fi

if [ "$HAVE_NEON" = "yes" ]; then
   add_define MAKEFILE NEON_CFLAGS '-mfpu=neon -marm'
   add_define MAKEFILE NEON_ASFLAGS -mfpu=neon
fi

if [ "$HAVE_FLOATHARD" = "yes" ]; then
   add_define MAKEFILE FLOATHARD_CFLAGS -mfloat-abi=hard
fi

if [ "$HAVE_FLOATSOFTFP" = "yes" ]; then
   add_define MAKEFILE FLOATSOFTFP_CFLAGS -mfloat-abi=softfp
fi

check_platform Win32 EGL 'EGL is' false

check_header EGL EGL/egl.h EGL/eglext.h
# some systems have EGL libs, but no pkgconfig
# https://github.com/linux-sunxi/sunxi-mali/pull/8
check_val '' EGL "-l${VC_PREFIX}EGL $EXTRA_GL_LIBS" '' "${VC_PREFIX}egl" '' '' true

if [ "$HAVE_EGL" = 'yes' ]; then
   EGL_LIBS="$EGL_LIBS $EXTRA_GL_LIBS"
fi

check_lib '' SSA -lass ass_library_init
check_lib '' SSE '-msse -msse2'
check_pkgconf EXYNOS libdrm_exynos

if [ "$LIBRETRO" ]; then
   die : 'Notice: Explicit libretro used, disabling dynamic libretro loading ...'
   HAVE_DYNAMIC='no'
else
   LIBRETRO="-lretro"
fi

[ "$HAVE_DYNAMIC" = 'yes' ] || {
   check_lib '' RETRO "$LIBRETRO" retro_init "$DYLIB" '' '' 'Cannot find libretro, did you forget --with-libretro="-lretro"?'
   add_define MAKEFILE libretro "$LIBRETRO"
}

add_define MAKEFILE ASSETS_DIR "${ASSETS_DIR:-$SHARE_DIR}/retroarch"
add_define MAKEFILE BIN_DIR "${BIN_DIR:-${PREFIX}/bin}"
add_define MAKEFILE DOC_DIR "${DOC_DIR:-${SHARE_DIR}/doc/retroarch}"
add_define MAKEFILE MAN_DIR "${MAN_DIR:-${SHARE_DIR}/man}"

check_platform DOS SHADERPIPELINE 'Shader-based pipelines are' false
check_platform DOS LANGEXTRA 'Extra languages are' false

check_lib '' THREADS "$PTHREADLIB" pthread_create
check_enabled THREADS THREAD_STORAGE 'Thread Local Storage' 'Threads are' false
check_lib '' THREAD_STORAGE "$PTHREADLIB" pthread_key_create

if [ "$OS" = 'Win32' ]; then
   HAVE_DYLIB=yes
else
   check_lib '' DYLIB "$DYLIB" dlopen
fi

check_lib '' NETWORKING "$SOCKETLIB" socket "" "$SOCKETHEADER"

if [ "$HAVE_NETWORKING" != 'no' ]; then
   HAVE_GETADDRINFO=auto
   HAVE_SOCKET_LEGACY=no

   # WinXP+ implements getaddrinfo()
   if [ "$OS" = 'Win32' ]; then
      HAVE_GETADDRINFO=yes
   else
      check_lib '' GETADDRINFO "$SOCKETLIB" getaddrinfo
      if [ "$HAVE_GETADDRINFO" != 'yes' ]; then
         HAVE_SOCKET_LEGACY=yes
         die : 'Notice: RetroArch will use legacy socket support'
      fi
   fi

   HAVE_NETWORK_CMD=yes
else
   HAVE_NETWORK_CMD=no
fi

check_enabled NETWORKING CHEEVOS cheevos 'Networking is' false
check_enabled NETWORKING DISCORD discord 'Networking is' false
check_enabled NETWORKING MINIUPNPC miniupnpc 'Networking is' false
check_enabled NETWORKING SSL ssl 'Networking is' false
check_enabled NETWORKING TRANSLATE OCR 'Networking is' false

check_enabled NETWORKING NETWORKGAMEPAD 'the networked game pad' 'Networking is' true
check_enabled MINIUPNPC BUILTINMINIUPNPC 'builtin miniupnpc' 'miniupnpc is' true

check_lib '' MINIUPNPC '-lminiupnpc'
check_lib '' STDIN_CMD "$CLIB" fcntl

if [ "$HAVE_NETWORK_CMD" = "yes" ] || [ "$HAVE_STDIN_CMD" = "yes" ]; then
   HAVE_COMMAND='yes'
else
   HAVE_COMMAND='no'
fi

check_lib '' GETOPT_LONG "$CLIB" getopt_long

if [ "$HAVE_DYLIB" = 'no' ] && [ "$HAVE_DYNAMIC" = 'yes' ]; then
   die 1 'Error: Dynamic loading of libretro is enabled, but your platform does not appear to have dlopen(), use --disable-dynamic or --with-libretro="-lretro".'
fi

check_val '' ALSA -lasound alsa alsa '' '' false
check_lib '' CACA -lcaca
check_lib '' SIXEL -lsixel

check_macro AUDIOIO AUDIO_SETINFO sys/audioio.h

if [ "$HAVE_OSS" != 'no' ]; then
   check_header OSS sys/soundcard.h
   check_header OSS_BSD soundcard.h
   check_lib '' OSS_LIB -lossaudio
fi

check_platform Linux TINYALSA 'Tinyalsa is' true
check_platform Linux RPILED 'The RPI led driver is' true

if [ "$OS" = 'Darwin' ]; then
   check_lib '' COREAUDIO "-framework AudioUnit" AudioUnitInitialize
   check_lib '' CORETEXT "-framework CoreText" CTFontCreateWithName
   check_lib '' COCOA "-framework AppKit" NSApplicationMain
   check_lib '' AVFOUNDATION "-framework AVFoundation"
   check_lib '' CORELOCATION "-framework CoreLocation"
   check_lib '' IOHIDMANAGER "-framework IOKit" IOHIDManagerCreate
   check_lib '' AL "-framework OpenAL" alcOpenDevice
   HAVE_X11=no # X11 breaks on recent OSXes even if present.
   HAVE_SDL=no
else
   check_lib '' AL -lopenal alcOpenDevice
fi

check_pkgconf RSOUND rsound 1.1
check_pkgconf ROAR libroar 1.0.12
check_val '' JACK -ljack '' jack 0.120.1 '' false
check_val '' PULSE -lpulse '' libpulse '' '' false
check_val '' SDL -lSDL SDL sdl 1.2.10 '' false
check_val '' SDL2 -lSDL2 SDL2 sdl2 2.0.0 '' false

if [ "$HAVE_SDL2" = 'yes' ] && [ "$HAVE_SDL" = 'yes' ]; then
   die : 'Notice: SDL drivers will be replaced by SDL2 ones.'
   HAVE_SDL=no
fi

check_platform Haiku DISCORD 'Discord is' false
check_enabled CXX DISCORD discord 'The C++ compiler is' false
check_enabled CXX QT 'Qt companion' 'The C++ compiler is' false

if [ "$HAVE_QT" != 'no' ]; then
   check_pkgconf QT5CORE Qt5Core 5.2
   check_pkgconf QT5GUI Qt5Gui 5.2
   check_pkgconf QT5WIDGETS Qt5Widgets 5.2
   check_pkgconf QT5CONCURRENT Qt5Concurrent 5.2
   check_pkgconf QT5NETWORK Qt5Network 5.2
   #check_pkgconf QT5WEBENGINE Qt5WebEngine 5.4

   # pkg-config is needed to reliably find Qt5 libraries.

   check_enabled QT5CORE QT Qt 'Qt5Core is' true
   check_enabled QT5GUI QT Qt 'Qt5GUI is' true
   check_enabled QT5WIDGETS QT Qt 'Qt5Widgets is' true
   check_enabled QT5CONCURRENT QT Qt 'Qt5Concurrent is' true
   check_enabled QT5NETWORK QT Qt 'Qt5Network is' true
   #check_enabled QT5WEBENGINE QT Qt 'Qt5Webengine is' true

   if [ "$HAVE_QT" != yes ]; then
      die : 'Notice: Qt support disabled, required libraries were not found.'
   fi

   check_pkgconf OPENSSL openssl 1.0.0
fi

check_enabled FLAC BUILTINFLAC 'builtin flac' 'flac is' true

check_val '' FLAC '-lFLAC' '' flac '' '' false

check_enabled SSL BUILTINMBEDTLS 'builtin mbedtls' 'ssl is' true

if [ "$HAVE_SSL" != 'no' ]; then
   check_header MBEDTLS \
      mbedtls/config.h \
      mbedtls/certs.h \
      mbedtls/debug.h \
      mbedtls/platform.h \
      mbedtls/net_sockets.h \
      mbedtls/ssl.h \
      mbedtls/ctr_drbg.h \
      mbedtls/entropy.h

   check_lib '' MBEDTLS -lmbedtls
   check_lib '' MBEDX509 -lmbedx509
   check_lib '' MBEDCRYPTO -lmbedcrypto

   if [ "$HAVE_MBEDTLS" = 'no' ] ||
      [ "$HAVE_MBEDX509" = 'no' ] ||
      [ "$HAVE_MBEDCRYPTO" = 'no' ]; then
      if [ "$HAVE_BUILTINMBEDTLS" != 'yes' ]; then
         die : 'Notice: System mbedtls libraries not found, disabling SSL support.'
         HAVE_SSL=no
      fi
   else
      HAVE_SSL=yes
   fi
fi

check_enabled THREADS LIBUSB libusb 'Threads are' false
check_val '' LIBUSB -lusb-1.0 libusb-1.0 libusb-1.0 1.0.13 '' false

if [ "$OS" = 'Win32' ]; then
   check_lib '' DINPUT -ldinput8
   check_lib '' D3D8 -ld3d8
   check_lib '' D3D9 -ld3d9
   check_lib '' DSOUND -ldsound

   if [ "$HAVE_D3DX" != 'no' ]; then
      check_lib '' D3DX8 -ld3dx8
      check_lib '' D3DX9 -ld3dx9
   fi

   if [ "$HAVE_DINPUT" != 'no' ]; then
      HAVE_XINPUT=yes
   fi

   HAVE_WASAPI=yes
   HAVE_XAUDIO=yes
   HAVE_WINMM=yes
fi

check_platform Win32 D3D8 'Direct3D 8 is' true
check_platform Win32 D3D9 'Direct3D 9 is' true
check_platform Win32 D3D10 'Direct3D 10 is' true
check_platform Win32 D3D11 'Direct3D 11 is' true
check_platform Win32 D3D12 'Direct3D 12 is' true
check_platform Win32 D3DX 'Direct3DX is' true

if [ "$HAVE_OPENGL" != 'no' ] && [ "$HAVE_OPENGLES" != 'yes' ]; then
   if [ "$OS" = 'Darwin' ]; then
      check_header OPENGL "OpenGL/gl.h"
      check_lib '' OPENGL "-framework OpenGL"
   elif [ "$OS" = 'Win32' ]; then
      check_header OPENGL "GL/gl.h"
      check_lib '' OPENGL -lopengl32
   else
      check_header OPENGL "GL/gl.h"
      check_lib '' OPENGL -lGL
   fi

   if [ "$HAVE_OPENGL" = 'yes' ]; then
      if [ "$OS" = 'Darwin' ]; then
         check_lib '' CG '-framework Cg' cgCreateContext
      elif [ "$OS" = 'Win32' ]; then
         check_lib cxx CG '-lcg -lcgGL' cgCreateContext
      else
         # On some distros, -lCg doesn't link against -lstdc++ it seems ...
         check_lib cxx CG '-lCg -lCgGL' cgCreateContext
      fi

      check_pkgconf OSMESA osmesa
   fi
else
   HAVE_OPENGL='no'
fi

check_enabled EGL OPENGLES OpenGLES 'EGL is' false
check_enabled EGL OPENGLES3 OpenGLES3 'EGL is' false
check_enabled EGL VG OpenVG 'EGL is' false
check_enabled OPENGL CG Cg 'OpenGL is' false
check_enabled OPENGL OSMESA osmesa 'OpenGL is' false
check_enabled OPENGL OPENGL1 OpenGL1 'OpenGL is' false

if [ "$HAVE_OPENGL" = 'no' ] && [ "$HAVE_OPENGLES3" = 'no' ]; then
   die : 'Notice: OpenGL and OpenGLES3 are disabled. Disabling OpenGL core.'
   HAVE_OPENGL_CORE='no'
elif [ "$HAVE_OPENGLES" != 'no' ] && [ "$HAVE_OPENGLES3" != 'yes' ]; then
   die : 'Notice: OpenGLES2 is enabled. Disabling the OpenGL core driver.'
   HAVE_OPENGL_CORE='no'
fi

check_enabled ZLIB BUILTINZLIB 'builtin zlib' 'zlib is' true

check_val '' ZLIB '-lz' '' zlib '' '' false
check_val '' MPV -lmpv '' mpv '' '' false

check_header DRMINGW exchndl.h
check_lib '' DRMINGW -lexchndl

check_enabled THREADS FFMPEG FFmpeg 'Threads are' false

if [ "$HAVE_FFMPEG" != 'no' ]; then
   check_val '' AVCODEC -lavcodec '' libavcodec 54 '' false
   check_val '' AVFORMAT -lavformat '' libavformat 54 '' false
   check_val '' AVDEVICE -lavdevice '' libavdevice '' '' false
   check_val '' SWRESAMPLE -lswresample '' libswresample '' '' false
   check_val '' AVRESAMPLE -lavresample '' libavresample '' '' false
   check_val '' AVUTIL -lavutil '' libavutil 51 '' false
   check_val '' SWSCALE -lswscale '' libswscale 2.1 '' false

   check_header AV_CHANNEL_LAYOUT libavutil/channel_layout.h

   HAVE_FFMPEG='yes'
   if [ "$HAVE_AVCODEC" = 'no' ] || [ "$HAVE_SWRESAMPLE" = 'no' ] || [ "$HAVE_AVFORMAT" = 'no' ] || [ "$HAVE_AVUTIL" = 'no' ] || [ "$HAVE_SWSCALE" = 'no' ]; then
      HAVE_FFMPEG='no'
      die : 'Notice: FFmpeg built-in support disabled due to missing or unsuitable packages.'
   fi
else
   HAVE_FFMPEG='no'
fi

if [ "$OS" != 'Win32' ]; then
   check_lib '' DYNAMIC "$DYLIB" dlopen
fi

if [ "$HAVE_KMS" != "no" ]; then
   check_val '' GBM -lgbm '' gbm 9.0 '' false
   check_val '' DRM -ldrm libdrm libdrm '' '' false
fi

check_enabled DRM KMS KMS 'DRM is' true
check_enabled GBM KMS KMS 'GBM is' true
check_enabled EGL KMS KMS 'EGL is' true

if [ "$HAVE_EGL" = "yes" ]; then
   if [ "$HAVE_OPENGLES" != "no" ]; then
      if [ "$OPENGLES_LIBS" ] || [ "$OPENGLES_CFLAGS" ]; then
         die : "Notice: Using custom OpenGLES CFLAGS ($OPENGLES_CFLAGS) and LDFLAGS ($OPENGLES_LIBS)."
         add_define MAKEFILE OPENGLES_LIBS "$OPENGLES_LIBS"
         add_define MAKEFILE OPENGLES_CFLAGS "$OPENGLES_CFLAGS"
      else
         check_val '' OPENGLES "-l${VC_PREFIX}GLESv2 $EXTRA_GL_LIBS" '' "${VC_PREFIX}glesv2" '' '' true
      fi
   fi
   check_val '' VG "-l${VC_PREFIX}OpenVG $EXTRA_GL_LIBS" '' "${VC_PREFIX}vg" '' '' false
fi

check_pkgconf DBUS dbus-1
check_val '' UDEV "-ludev" '' libudev '' '' false
check_val '' V4L2 -lv4l2 '' libv4l2 '' '' false
check_val '' FREETYPE -lfreetype freetype2 freetype2 '' '' false
check_val '' X11 -lX11 '' x11 '' '' false
check_val '' XCB -lxcb '' xcb '' '' false

if [ "$HAVE_X11" != 'no' ]; then
   check_val '' XEXT -lXext '' xext '' '' false
   check_val '' XF86VM -lXxf86vm '' xxf86vm '' '' false
else
   die : 'Notice: X11 not present. Skipping X11 code paths.'
fi

check_enabled X11 XINERAMA Xinerama 'X11 is' false
check_enabled X11 XSHM XShm 'X11 is' false
check_enabled X11 XRANDR Xrandr 'X11 is' false
check_enabled X11 XVIDEO XVideo 'X11 is' false
check_enabled XEXT XVIDEO XVideo 'Xext is' false
check_enabled XF86VM XVIDEO XVideo 'XF86vm is' false

check_val '' XVIDEO -lXv '' xv '' '' false
check_val '' XINERAMA -lXinerama '' xinerama '' '' false
check_lib '' XRANDR -lXrandr
check_header XSHM X11/Xlib.h X11/extensions/XShm.h
check_val '' XKBCOMMON -lxkbcommon '' xkbcommon 0.3.2 '' false
check_val '' WAYLAND '-lwayland-egl -lwayland-client' '' wayland-egl 10.1.0 '' false
check_val '' WAYLAND_CURSOR -lwayland-cursor '' wayland-cursor 1.12 '' false
check_pkgconf WAYLAND_PROTOS wayland-protocols 1.15
check_pkgconf WAYLAND_SCANNER wayland-scanner '1.15 1.12'

if [ "$HAVE_WAYLAND_SCANNER" = yes ] &&
   [ "$HAVE_WAYLAND_CURSOR" = yes ] &&
   [ "$HAVE_WAYLAND" = yes ]; then
      ./gfx/common/wayland/generate_wayland_protos.sh \
         -c "$WAYLAND_SCANNER_VERSION" \
         -p "$HAVE_WAYLAND_PROTOS" \
         -s "$SHARE_DIR" ||
         die 1 'Error: Failed generating wayland protocols.'
else
    die : 'Notice: wayland libraries not found, disabling wayland support.'
    HAVE_WAYLAND='no'
fi

check_header PARPORT linux/parport.h
check_header PARPORT linux/ppdev.h

if [ "$OS" != 'Win32' ] && [ "$OS" != 'Linux' ]; then
   check_lib '' STRL "$CLIB" strlcpy
fi

check_lib '' STRCASESTR "$CLIB" strcasestr
check_lib '' MMAP "$CLIB" mmap

check_enabled CXX VULKAN vulkan 'The C++ compiler is' false
check_enabled CXX OPENGL_CORE 'OpenGL core' 'The C++ compiler is' false
check_enabled THREADS VULKAN vulkan 'Threads are' false

if [ "$HAVE_VULKAN" != "no" ] && [ "$OS" = 'Win32' ]; then
   HAVE_VULKAN=yes
else
   check_lib '' VULKAN -lvulkan vkCreateInstance
fi

check_pkgconf PYTHON 'python3 python3 python-3.7 python-3.6 python-3.5 python-3.4 python-3.3 python-3.2'

if [ "$HAVE_MENU" != 'no' ]; then
   if [ "$HAVE_OPENGL" = 'no' ] && [ "$HAVE_OPENGLES" = 'no' ] && [ "$HAVE_VULKAN" = 'no' ]; then
      if [ "$OS" = 'Win32' ]; then
         HAVE_SHADERPIPELINE=no
         HAVE_VULKAN=no
      else
         if [ "$HAVE_CACA" != 'yes' ] && [ "$HAVE_SIXEL" != 'yes' ] &&
            [ "$HAVE_SDL" != 'yes' ] && [ "$HAVE_SDL2" != 'yes' ]; then
            HAVE_RGUI=no
         fi
         HAVE_MATERIALUI=no
         HAVE_OZONE=no
         HAVE_XMB=no
         HAVE_STRIPES=no
         HAVE_MENU_WIDGETS=no
      fi
      die : 'Notice: Hardware rendering context not available.'
   fi
fi

check_macro NEON __ARM_NEON__

add_define MAKEFILE OS "$OS"

if [ "$HAVE_DEBUG" = 'yes' ]; then
   add_define MAKEFILE DEBUG 1
   if [ "$HAVE_OPENGL" = 'yes' ] ||
      [ "$HAVE_OPENGL1" = 'yes' ] ||
      [ "$HAVE_OPENGLES" = 'yes' ] ||
      [ "$HAVE_OPENGLES3" = 'yes' ]; then
      add_define MAKEFILE GL_DEBUG 1
   fi
   if [ "$HAVE_VULKAN" = 'yes' ]; then
      add_define MAKEFILE VULKAN_DEBUG 1
   fi
fi

check_enabled MENU MENU_WIDGETS 'menu widgets' 'The menu is' false
check_enabled ZLIB RPNG RPNG 'zlib is' false
check_enabled V4L2 VIDEOPROCESSOR 'video processor' 'Video4linux2 is' true
