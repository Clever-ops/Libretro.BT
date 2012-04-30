. qb/qb.params.sh

PACKAGE_NAME=ssnes
PACKAGE_VERSION=0.9.6-beta1

# Adds a command line opt to ./configure --help
# $1: Variable (HAVE_ALSA, HAVE_OSS, etc)   
# $2: Comment                 
# $3: Default arg. auto implies that HAVE_ALSA will be set according to library checks later on.
add_command_line_enable DYNAMIC "Disable dynamic loading of libsnes library" yes
add_command_line_string LIBRETRO "libretro library used" ""
add_command_line_enable THREADS "Threading support" auto
add_command_line_enable FFMPEG "Enable FFmpeg recording support" auto
add_command_line_enable X264RGB "Enable lossless X264 RGB recording" no
add_command_line_enable DYLIB "Enable dynamic loading support" auto
add_command_line_enable NETPLAY "Enable netplay support" auto
add_command_line_enable CONFIGFILE "Disable support for config file" yes
add_command_line_enable OPENGL "Disable OpenGL support" yes
add_command_line_enable CG "Enable Cg shader support" auto
add_command_line_enable XML "Enable bSNES-style XML shader support" auto
add_command_line_enable FBO "Enable render-to-texture (FBO) support" auto
add_command_line_enable ALSA "Enable ALSA support" auto
add_command_line_enable OSS "Enable OSS support" auto
add_command_line_enable RSOUND "Enable RSound support" auto
add_command_line_enable ROAR "Enable RoarAudio support" auto
add_command_line_enable AL "Enable OpenAL support" auto
add_command_line_enable JACK "Enable JACK support" auto
add_command_line_enable COREAUDIO "Enable CoreAudio support" auto
add_command_line_enable PULSE "Enable PulseAudio support" auto
add_command_line_enable FREETYPE "Enable FreeType support" auto
add_command_line_enable XVIDEO "Enable XVideo support" auto
add_command_line_enable SDL_IMAGE "Enable SDL_image support" auto
add_command_line_enable PYTHON "Enable Python 3 support for shaders" auto
add_command_line_enable SINC "Disable SINC resampler" yes
add_command_line_enable BSV_MOVIE "Disable BSV movie support" yes
