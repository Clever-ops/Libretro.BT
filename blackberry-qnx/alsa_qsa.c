/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../general.h"
#include "../driver.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <sys/asoundlib.h>

typedef struct alsa
{
   snd_pcm_t *pcm;
   size_t buffer_size;
   bool nonblock;
   bool has_float;
   bool can_pause;
   bool is_paused;
   unsigned frame_bits;
} alsa_t;

#define MAX_FRAG_SIZE 3072
#define DEFAULT_RATE 48000

typedef long snd_pcm_sframes_t;

static void *alsa_qsa_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)rate;
   (void)latency;

   int err, card, dev;
   snd_pcm_channel_params_t params = {0};
   snd_pcm_channel_setup_t setup = {0};
   alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));
   if (!alsa)
      return NULL;

   if ((err = snd_pcm_open_preferred(&alsa->pcm, &card, &dev,
               SND_PCM_OPEN_PLAYBACK)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Audio open error: %s\n", snd_strerror(err));
      goto error;
   }

   if((err = snd_pcm_nonblock_mode(alsa->pcm, 1)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Can't set blocking mode: %s\n", snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_plugin_set_disable (alsa->pcm, PLUGIN_MMAP)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Can't disable MMAP plugin: %s\n", snd_strerror(err));
      goto error;
   }

   if ((err = snd_pcm_plugin_set_enable (alsa->pcm, PLUGIN_ROUTING)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Can't enable routing: %s\n", snd_strerror(err));
      goto error;
   }

   memset(&params, 0, sizeof(params));



   params.mode = SND_PCM_MODE_BLOCK;
   params.channel = SND_PCM_CHANNEL_PLAYBACK;
   params.start_mode = SND_PCM_START_FULL;
   params.stop_mode = SND_PCM_STOP_STOP;

   params.buf.block.frag_size = MAX_FRAG_SIZE;
   params.buf.block.frags_min = 2;
   params.buf.block.frags_max = 1;
   params.format.interleave = 1;
   params.format.rate = DEFAULT_RATE;
   params.format.voices = 2;

   params.format.format = SND_PCM_SFMT_S16_LE;
   alsa->frame_bits = 16 * 2; /* bits * channel */

#if 0
   /* TODO/FIXME - invalid argument? */
   snd_pcm_plugin_set_disable(alsa->pcm, PLUGIN_DISABLE_BUFFER_PARTIAL_BLOCKS);
#endif

   if ((err = snd_pcm_plugin_params(alsa->pcm, &params)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Channel Parameter Error: %s\n", snd_strerror(err));
      goto error;
   }

   setup.channel = SND_PCM_CHANNEL_PLAYBACK;

   if ((err = snd_pcm_plugin_setup(alsa->pcm, &setup)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Channel Parameter Read Back Error: %s\n", snd_strerror(err));
      goto error;
   }

   alsa->buffer_size = setup.buf.block.frag_size * (setup.buf.block.frags_max+1);
   RARCH_LOG("[ALSA QSA]: buffer size: %d bytes\n", alsa->buffer_size);

   if ((err = snd_pcm_plugin_prepare(alsa->pcm, SND_PCM_CHANNEL_PLAYBACK)) < 0)
   {
      RARCH_ERR("[ALSA QSA]: Channel Prepare Error: %s\n", snd_strerror(err));
      goto error;
   }

   alsa->has_float = false;
#ifdef HAVE_BB10
   alsa->can_pause = true;
#endif
   RARCH_LOG("[ALSA QSA]: Can pause: %s.\n", alsa->can_pause ? "yes" : "no");

   return alsa;

error:
   return (void*)-1;
}

static inline snd_pcm_sframes_t snd_pcm_bytes_to_frames(alsa_t *alsa, ssize_t bytes)
{
   return bytes * 8 / alsa->frame_bits;
}

static ssize_t alsa_qsa_write(void *data, const void *buf, size_t size_)
{
   int status;
   alsa_t *alsa = (alsa_t*)data;
   snd_pcm_channel_status_t cstatus = {0};
   snd_pcm_sframes_t written = 0;
   snd_pcm_sframes_t size = snd_pcm_bytes_to_frames(alsa, size_);
   bool eagain_retry = true;

   while (size)
   {
      snd_pcm_sframes_t frames = snd_pcm_plugin_write(alsa->pcm, buf, size);

      if (frames <= 0)
      {
         if (frames == -EAGAIN && !alsa->nonblock) // Definitely not supposed to happen.
         {
            RARCH_WARN("[ALSA]: poll() was signaled, but EAGAIN returned from write.\n"
                  "Your ALSA driver might be subtly broken.\n");

            if (eagain_retry)
            {
               eagain_retry = false;
               continue;
            }
            else
               return written;
         }
         else if (frames == -EAGAIN) // Expected if we're running nonblock.
         {
            return written;
         }
         /* TODO/FIXME - implement check_pcm_status for checking against more errors? */
         else if (frames < 0)
         {
            RARCH_ERR("[ALSA]: Unknown error occured (%s).\n", snd_strerror(frames));
            return -1;
         }
      }
      else
      {
         written += frames;
         buf     += (frames << 1) * (alsa->has_float ? sizeof(float) : sizeof(int16_t));
         size    -= frames;
      }
   }

   return written;
}

static bool alsa_qsa_stop(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa->can_pause && !alsa->is_paused)
   {
#ifdef HAVE_BB10
      if (snd_pcm_playback_pause(alsa->pcm) == 0)
#else
      if (snd_pcm_plugin_flush(alsa->pcm, SND_PCM_CHANNEL_PLAYBACK) == 0)
#endif
      {
         alsa->is_paused = true;
         return true;
      }
      else
         return false;
   }
   else
      return true;
}

static void alsa_qsa_set_nonblock_state(void *data, bool state)
{
   alsa_t *alsa = (alsa_t*)data;

   int err;

   if((err = snd_pcm_nonblock_mode(alsa->pcm, state)) < 0)
   {
      RARCH_ERR("Can't set blocking mode to %d: %s\n", state,
            snd_strerror(err));
      return;
   }

   alsa->nonblock = state;
}

static bool alsa_qsa_start(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa->can_pause && alsa->is_paused)
   {
#ifdef HAVE_BB10
      int ret = snd_pcm_playback_resume(alsa->pcm);
#else
      int ret = 0;
#endif
      if (ret < 0)
      {
         RARCH_ERR("[ALSA QSA]: Failed to unpause: %s.\n", snd_strerror(ret));
         return false;
      }
      else
      {
         alsa->is_paused = false;
         return true;
      }
   }
   else
      return true;

}

static bool alsa_qsa_use_float(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->has_float;
}

static void alsa_qsa_free(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa)
   {
      if (alsa->pcm)
      {
         snd_pcm_close(alsa->pcm);
         alsa->pcm = NULL;
      }
      free(alsa);
   }
}

static size_t alsa_qsa_write_avail(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   snd_pcm_channel_status_t status = {0};

   status.channel = SND_PCM_CHANNEL_PLAYBACK;
   snd_pcm_plugin_status(alsa->pcm, &status);

   return status.free;
}

static size_t alsa_qsa_buffer_size(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->buffer_size;
}

const audio_driver_t audio_alsa = {
   alsa_qsa_init,
   alsa_qsa_write,
   alsa_qsa_stop,
   alsa_qsa_start,
   alsa_qsa_set_nonblock_state,
   alsa_qsa_free,
   alsa_qsa_use_float,
   "alsa",
   alsa_qsa_write_avail,
   alsa_qsa_buffer_size,
};

