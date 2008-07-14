/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef AUD_MAD_H
#define AUD_MAD_H

/* #define AUD_DEBUG 1 */
/* #define DEBUG_INTENSIVELY 1 */
/* #define DEBUG_DITHER 1 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "MADPlug"

#undef PACKAGE
#define PACKAGE "audacious-plugins"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <audacious/id3tag.h>
#include <mad.h>

#include "xing.h"

struct mad_info_t
{
    /* InputPlayback */
    InputPlayback *playback;

    /* seek time */
    gulong seek;      /**< seek time in milliseconds */

    /* state */
    guint current_frame;/**< current mp3 frame */
    mad_timer_t pos;    /**< current play position */

    /* song info */
    guint vbr;      /**< bool: is vbr? */
    guint bitrate;  /**< avg. bitrate */
    guint freq;     /**< sample freq. */
    guint mpeg_layer;   /**< mpeg layer */
    guint mode;     /**< mpeg stereo mode */
    guint channels;
    gint frames;    /**< total mp3 frames or -1 */
    gint fmt;       /**< sample format */
    gint size;      /**< file size in bytes or -1 */
    gchar *title;   /**< title for xmms */
    mad_timer_t duration;   /**< total play time */
    struct id3_tag *tag;
    struct id3_file *id3file;
    struct xing xing;
    Tuple *tuple;          /* audacious tuple data */
    gchar *prev_title;           /* used to optimize set_info calls */

    /* replay parameters */
    double replaygain_album_scale;
    double replaygain_track_scale;
    gchar *replaygain_album_str;
    gchar *replaygain_track_str;
    double replaygain_album_peak; /* 0 if gain/peak pair not set */
    double replaygain_track_peak; /* 0 if gain/peak pair not set */
    gchar *replaygain_album_peak_str;
    gchar *replaygain_track_peak_str;
    double mp3gain_undo;        // -1 if not set
    double mp3gain_minmax;
    gchar *mp3gain_undo_str;
    gchar *mp3gain_minmax_str;

    /* data access */
    gchar *url;
    gchar *filename;
    VFSFile *infile;
    gint offset;

    /* flags */
    gboolean remote;
    gboolean fileinfo_request;

};

typedef struct audmad_config_t
{
    gboolean fast_play_time_calc;
    gboolean use_xing;
    gboolean dither;
    gboolean sjis;
    gboolean title_override;
    gchar *id3_format;
    gboolean show_avg_vbr_bitrate;
    gboolean force_reopen_audio;
} audmad_config_t;

// global variables
extern InputPlugin *mad_plugin;
extern audmad_config_t *audmad_config;

// gcond
extern GMutex *mad_mutex;
extern GMutex *pb_mutex;
extern GCond *mad_cond;

// prototypes
void audmad_config_compute(struct audmad_config_t *config);
void input_process_remote_metadata(struct mad_info_t *info);
gpointer decode_loop(gpointer arg);
void audmad_error(gchar * fmt, ...);
void audmad_configure(void);

#endif                          /* !AUD_MAD_H */
