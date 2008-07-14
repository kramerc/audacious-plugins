/*  FileWriter Vorbis Plugin
 *  Copyright (c) 2007 William Pitcock <nenolod@sacredspiral.co.uk>
 *
 *  Partially derived from Og(g)re - Ogg-Output-Plugin:
 *  Copyright (c) 2002 Lars Siebold <khandha5@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "plugins.h"

#ifdef FILEWRITER_VORBIS

#include <vorbis/vorbisenc.h>
#include <stdlib.h>

static void vorbis_init(write_output_callback write_output_func);
static void vorbis_configure(void);
static gint vorbis_open(void);
static void vorbis_write(gpointer data, gint length);
static void vorbis_flush(void);
static void vorbis_close(void);
static gint vorbis_free(void);
static gint vorbis_playing(void);
static gint vorbis_get_written_time(void);
static gint (*write_output)(void *ptr, gint length);

FileWriter vorbis_plugin =
{
    vorbis_init,
    vorbis_configure,
    vorbis_open,
    vorbis_write,
    vorbis_flush,
    vorbis_close,
    vorbis_free,
    vorbis_playing,
    vorbis_get_written_time
};

static float v_base_quality = 0.5;

static ogg_stream_state os;
static ogg_page og;
static ogg_packet op;

static vorbis_dsp_state vd;
static vorbis_block vb;
static vorbis_info vi;
static vorbis_comment vc;

static float **encbuffer;
static guint64 olen = 0;

static void vorbis_init(write_output_callback write_output_func)
{
    ConfigDb *db = aud_cfg_db_open();

    aud_cfg_db_get_float(db, "filewriter_vorbis", "base_quality", &v_base_quality);

    aud_cfg_db_close(db);

    if (write_output_func)
        write_output=write_output_func;
}

static gint vorbis_open(void)
{
    gint result;
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_init(NULL);

    written = 0;
    olen = 0;

    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);

    if (tuple)
    {
        const gchar *scratch;
        gchar tmpstr[32];
        gint scrint;

        if ((scratch = aud_tuple_get_string(tuple, FIELD_TITLE, NULL)))
            vorbis_comment_add_tag(&vc, "title", (gchar *) scratch);
        if ((scratch = aud_tuple_get_string(tuple, FIELD_ARTIST, NULL)))
            vorbis_comment_add_tag(&vc, "artist", (gchar *) scratch);
        if ((scratch = aud_tuple_get_string(tuple, FIELD_ALBUM, NULL)))
            vorbis_comment_add_tag(&vc, "album", (gchar *) scratch);
        if ((scratch = aud_tuple_get_string(tuple, FIELD_GENRE, NULL)))
            vorbis_comment_add_tag(&vc, "genre", (gchar *) scratch);
        if ((scratch = aud_tuple_get_string(tuple, FIELD_DATE, NULL)))
            vorbis_comment_add_tag(&vc, "date", (gchar *) scratch);
        if ((scratch = aud_tuple_get_string(tuple, FIELD_COMMENT, NULL)))
            vorbis_comment_add_tag(&vc, "comment", (gchar *) scratch);

        if ((scrint = aud_tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL)))
        {
            g_snprintf(tmpstr, sizeof(tmpstr), "%d", scrint);
            vorbis_comment_add_tag(&vc, "tracknumber", tmpstr);
        }

        if ((scrint = aud_tuple_get_int(tuple, FIELD_YEAR, NULL)))
        {
            g_snprintf(tmpstr, sizeof(tmpstr), "%d", scrint);
            vorbis_comment_add_tag(&vc, "year", tmpstr);
        }
    }

    if ((result = vorbis_encode_init_vbr(&vi, (long)input.channels, (long)input.frequency, v_base_quality)) != 0)
    {
        vorbis_info_clear(&vi);
        return 0;
    }

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    srand(time(NULL));
    ogg_stream_init(&os, rand());

    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    while((result = ogg_stream_flush(&os, &og)))
    {
        if (result == 0)
            break;

        written += write_output(og.header, og.header_len);
        written += write_output(og.body, og.body_len);
    }

    return 1;
}

static void vorbis_write(gpointer data, gint length)
{
    int i;
    int result;
    short int *tmpdata;

    /* ask vorbisenc for a buffer to fill with pcm data */
    encbuffer = vorbis_analysis_buffer(&vd, length);
    tmpdata = data;

    /*
     * deinterleave the audio signal, 32768.0 is the highest peak level allowed
     * in a 16-bit PCM signal.
     */
    if (input.channels == 1)
    {
        for (i = 0; i < (length / 2); i++)
        {
            encbuffer[0][i] = tmpdata[i] / 32768.0;
            encbuffer[1][i] = tmpdata[i] / 32768.0;
        }
    }
    else
    {
        for (i = 0; i < (length / 4); i++)
        {
            encbuffer[0][i] = tmpdata[2 * i] / 32768.0;
            encbuffer[1][i] = tmpdata[2 * i + 1] / 32768.0;
        }
    }

    vorbis_analysis_wrote(&vd, i);

    while(vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, &op);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op))
        {
            ogg_stream_packetin(&os, &op);

            while ((result = ogg_stream_pageout(&os, &og)))
            {
                if (result == 0)
                    break;

                written += write_output(og.header, og.header_len);
                written += write_output(og.body, og.body_len);
            }
        }
    }

    olen += length;
}

static void vorbis_flush(void)
{
    //nothing to do here yet. --AOS
}

static void vorbis_close(void)
{
    ogg_stream_clear(&os);

    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_info_clear(&vi);
}

static gint vorbis_free(void)
{
    return 1000000;
}

static gint vorbis_playing(void)
{
    return 0;
}

static gint vorbis_get_written_time(void)
{
    if (input.frequency && input.channels)
        return (gint) ((olen * 1000) / (input.frequency * 2 * input.channels) + offset);

    return 0;
}

/* configuration stuff */
static GtkWidget *configure_win = NULL;
static GtkWidget *quality_frame, *quality_vbox, *quality_hbox1, *quality_spin, *quality_label;
static GtkObject *quality_adj;

static void quality_change(GtkAdjustment *adjustment, gpointer user_data)
{
    if (gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(quality_spin)))
        v_base_quality = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(quality_spin)) / 10;
    else
        v_base_quality = 0.0;
}

static void configure_ok_cb(gpointer data)
{
    ConfigDb *db = aud_cfg_db_open();

    aud_cfg_db_set_float(db, "filewrite_vorbis", "base_quality", v_base_quality);

    aud_cfg_db_close(db);

    gtk_widget_hide(configure_win);
}

static void vorbis_configure(void)
{
    GtkWidget *vbox, *bbox;
    GtkWidget *button;

    if (configure_win == NULL)
    {
        configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        g_signal_connect(G_OBJECT(configure_win), "destroy", G_CALLBACK(gtk_widget_destroyed), NULL);

        gtk_window_set_title(GTK_WINDOW(configure_win), _("Vorbis Encoder Configuration"));
        gtk_container_set_border_width(GTK_CONTAINER(configure_win), 5);

        vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_win), vbox);

        /* quality options */
        quality_frame = gtk_frame_new(_("Quality"));
        gtk_container_set_border_width(GTK_CONTAINER(quality_frame), 5);
        gtk_box_pack_start(GTK_BOX(vbox), quality_frame, FALSE, FALSE, 2);

        quality_vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(quality_vbox), 10);
        gtk_container_add(GTK_CONTAINER(quality_frame), quality_vbox);

        /* quality option: vbr level */
        quality_hbox1 = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(quality_hbox1), 10);
        gtk_container_add(GTK_CONTAINER(quality_vbox), quality_hbox1);

        quality_label = gtk_label_new(_("Quality level (0 - 10):"));
        gtk_misc_set_alignment(GTK_MISC(quality_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(quality_hbox1), quality_label, TRUE, TRUE, 0);

        quality_adj = gtk_adjustment_new(5, 0, 10, 0.1, 1, 1);
        quality_spin = gtk_spin_button_new(GTK_ADJUSTMENT(quality_adj), 1, 2);
        gtk_box_pack_start(GTK_BOX(quality_hbox1), quality_spin, TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(quality_adj), "value-changed", G_CALLBACK(quality_change), NULL);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(quality_spin), (v_base_quality * 10));

        /* buttons */
        bbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
        gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gtk_widget_hide), GTK_OBJECT(configure_win));
        gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);

        button = gtk_button_new_from_stock(GTK_STOCK_OK);
        g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(configure_ok_cb), NULL);
        gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
        gtk_widget_grab_default(button);
    }

    gtk_widget_show_all(configure_win);
}

#endif
