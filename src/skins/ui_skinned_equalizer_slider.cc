/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses>.
 */

#include <libaudcore/audstrings.h>
#include <libaudcore/equalizer.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "drawing.h"
#include "skins_cfg.h"
#include "ui_main.h"
#include "ui_skin.h"
#include "ui_skinned_equalizer_slider.h"

void EqSlider::draw (cairo_t * cr)
{
    int frame = 27 - m_pos * 27 / 50;
    if (frame < 14)
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 13 + 15 * frame, 164, 0, 0, 14, 63);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 13 + 15 * (frame - 14), 229, 0, 0, 14, 63);

    if (m_pressed)
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 176, 1, m_pos, 11, 11);
    else
        skin_draw_pixbuf (cr, SKIN_EQMAIN, 0, 164, 1, m_pos, 11, 11);
}

void EqSlider::moved (int pos)
{
    m_pos = aud::clamp (pos, 0, 50);
    if (m_pos == 24 || m_pos == 26)
        m_pos = 25;

    m_value = (float) (25 - m_pos) * AUD_EQ_MAX_GAIN / 25;

    if (m_band < 0)
        aud_set_double (nullptr, "equalizer_preamp", m_value);
    else
        aud_eq_set_band (m_band, m_value);

    mainwin_show_status_message (str_printf ("%s: %+.1f dB", (const char *) m_name, m_value));
}

bool EqSlider::button_press (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    m_pressed = true;
    moved (event->y / config.scale - 5);
    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

bool EqSlider::button_release (GdkEventButton * event)
{
    if (event->button != 1)
        return false;

    if (! m_pressed)
        return true;

    m_pressed = false;
    moved (event->y / config.scale - 5);
    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

bool EqSlider::motion (GdkEventMotion * event)
{
    if (! m_pressed)
        return true;

    moved (event->y / config.scale - 5);
    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

bool EqSlider::scroll (GdkEventScroll * event)
{
    if (event->direction == GDK_SCROLL_UP)
        moved (m_pos - 2);
    else if (event->direction == GDK_SCROLL_DOWN)
        moved (m_pos + 2);

    gtk_widget_queue_draw (gtk_dr ());
    return true;
}

void EqSlider::set_value (float value)
{
    if (m_pressed)
        return;

    m_value = value;
    m_pos = aud::clamp (25 - (int) (value * 25 / AUD_EQ_MAX_GAIN), 0, 50);
    gtk_widget_queue_draw (gtk_dr ());
}

EqSlider::EqSlider (const char * name, int band) :
    m_name (name),
    m_band (band)
{
    GtkWidget * slider = gtk_event_box_new ();
    gtk_event_box_set_visible_window ((GtkEventBox *) slider, false);
    gtk_widget_set_size_request (slider, 14 * config.scale, 63 * config.scale);
    gtk_widget_add_events (slider, GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
    set_gtk (slider, true);
}
