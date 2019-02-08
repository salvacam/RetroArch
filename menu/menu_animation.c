/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Jean-André Santoni
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <encodings/utf.h>
#include <retro_math.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

#define DG_DYNARR_IMPLEMENTATION
#include <stdio.h>
#include <retro_assert.h>
#define DG_DYNARR_ASSERT(cond, msg)  (void)0
#include <array/dynarray.h>
#undef DG_DYNARR_IMPLEMENTATION

#include "menu_animation.h"
#include "../configuration.h"
#include "../performance_counters.h"

struct tween
{
   float       duration;
   float       running_since;
   float       initial_value;
   float       target_value;
   float       *subject;
   uintptr_t   tag;
   easing_cb   easing;
   tween_cb    cb;
   void        *userdata;
   bool        deleted;
};

DA_TYPEDEF(struct tween, tween_array_t)

struct menu_animation
{
   tween_array_t list;
   tween_array_t pending;
   bool pending_deletes;
   bool in_update;
};

typedef struct menu_animation menu_animation_t;

static menu_animation_t anim;
static retro_time_t cur_time    = 0;
static retro_time_t old_time    = 0;
static float delta_time         = 0.0f;
static float ticker_time        = 0.0f;
static bool animation_is_active = false;

/* from https://github.com/kikito/tween.lua/blob/master/tween.lua */

static float easing_linear(float t, float b, float c, float d)
{
   return c * t / d + b;
}

static float easing_in_out_quad(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 2) + b;
   return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

static float easing_in_quad(float t, float b, float c, float d)
{
   return c * pow(t / d, 2) + b;
}

static float easing_out_quad(float t, float b, float c, float d)
{
   t = t / d;
   return -c * t * (t - 2) + b;
}

static float easing_out_in_quad(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quad(t * 2, b, c / 2, d);
   return easing_in_quad((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_cubic(float t, float b, float c, float d)
{
   return c * pow(t / d, 3) + b;
}

static float easing_out_cubic(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 3) + 1) + b;
}

static float easing_in_out_cubic(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * t * t * t + b;
   t = t - 2;
   return c / 2 * (t * t * t + 2) + b;
}

static float easing_out_in_cubic(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_cubic(t * 2, b, c / 2, d);
   return easing_in_cubic((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_quart(float t, float b, float c, float d)
{
   return c * pow(t / d, 4) + b;
}

static float easing_out_quart(float t, float b, float c, float d)
{
   return -c * (pow(t / d - 1, 4) - 1) + b;
}

static float easing_in_out_quart(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 4) + b;
   return -c / 2 * (pow(t - 2, 4) - 2) + b;
}

static float easing_out_in_quart(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quart(t * 2, b, c / 2, d);
   return easing_in_quart((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_quint(float t, float b, float c, float d)
{
   return c * pow(t / d, 5) + b;
}

static float easing_out_quint(float t, float b, float c, float d)
{
   return c * (pow(t / d - 1, 5) + 1) + b;
}

static float easing_in_out_quint(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return c / 2 * pow(t, 5) + b;
   return c / 2 * (pow(t - 2, 5) + 2) + b;
}

static float easing_out_in_quint(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_quint(t * 2, b, c / 2, d);
   return easing_in_quint((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_sine(float t, float b, float c, float d)
{
   return -c * cos(t / d * (M_PI / 2)) + c + b;
}

static float easing_out_sine(float t, float b, float c, float d)
{
   return c * sin(t / d * (M_PI / 2)) + b;
}

static float easing_in_out_sine(float t, float b, float c, float d)
{
   return -c / 2 * (cos(M_PI * t / d) - 1) + b;
}

static float easing_out_in_sine(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_sine(t * 2, b, c / 2, d);
   return easing_in_sine((t * 2) -d, b + c / 2, c / 2, d);
}

static float easing_in_expo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   return c * powf(2, 10 * (t / d - 1)) + b - c * 0.001;
}

static float easing_out_expo(float t, float b, float c, float d)
{
   if (t == d)
      return b + c;
   return c * 1.001 * (-powf(2, -10 * t / d) + 1) + b;
}

static float easing_in_out_expo(float t, float b, float c, float d)
{
   if (t == 0)
      return b;
   if (t == d)
      return b + c;
   t = t / d * 2;
   if (t < 1)
      return c / 2 * powf(2, 10 * (t - 1)) + b - c * 0.0005;
   return c / 2 * 1.0005 * (-powf(2, -10 * (t - 1)) + 2) + b;
}

static float easing_out_in_expo(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_expo(t * 2, b, c / 2, d);
   return easing_in_expo((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_in_circ(float t, float b, float c, float d)
{
   return(-c * (sqrt(1 - powf(t / d, 2)) - 1) + b);
}

static float easing_out_circ(float t, float b, float c, float d)
{
   return(c * sqrt(1 - powf(t / d - 1, 2)) + b);
}

static float easing_in_out_circ(float t, float b, float c, float d)
{
   t = t / d * 2;
   if (t < 1)
      return -c / 2 * (sqrt(1 - t * t) - 1) + b;
   t = t - 2;
   return c / 2 * (sqrt(1 - t * t) + 1) + b;
}

static float easing_out_in_circ(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_circ(t * 2, b, c / 2, d);
   return easing_in_circ((t * 2) - d, b + c / 2, c / 2, d);
}

static float easing_out_bounce(float t, float b, float c, float d)
{
   t = t / d;
   if (t < 1 / 2.75)
      return c * (7.5625 * t * t) + b;
   if (t < 2 / 2.75)
   {
      t = t - (1.5 / 2.75);
      return c * (7.5625 * t * t + 0.75) + b;
   }
   else if (t < 2.5 / 2.75)
   {
      t = t - (2.25 / 2.75);
      return c * (7.5625 * t * t + 0.9375) + b;
   }
   t = t - (2.625 / 2.75);
   return c * (7.5625 * t * t + 0.984375) + b;
}

static float easing_in_bounce(float t, float b, float c, float d)
{
   return c - easing_out_bounce(d - t, 0, c, d) + b;
}

static float easing_in_out_bounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_in_bounce(t * 2, 0, c, d) * 0.5 + b;
   return easing_out_bounce(t * 2 - d, 0, c, d) * 0.5 + c * .5 + b;
}

static float easing_out_in_bounce(float t, float b, float c, float d)
{
   if (t < d / 2)
      return easing_out_bounce(t * 2, b, c / 2, d);
   return easing_in_bounce((t * 2) - d, b + c / 2, c / 2, d);
}

static void menu_animation_ticker_generic(uint64_t idx,
      size_t max_width, size_t *offset, size_t *width)
{
   int ticker_period     = (int)(2 * (*width - max_width) + 4);
   int phase             = idx % ticker_period;

   int phase_left_stop   = 2;
   int phase_left_moving = (int)(phase_left_stop + (*width - max_width));
   int phase_right_stop  = phase_left_moving + 2;

   int left_offset       = phase - phase_left_stop;
   int right_offset      = (int)((*width - max_width) - (phase - phase_right_stop));

   if (phase < phase_left_stop)
      *offset = 0;
   else if (phase < phase_left_moving)
      *offset = left_offset;
   else if (phase < phase_right_stop)
      *offset = *width - max_width;
   else
      *offset = right_offset;

   *width = max_width;
}

void menu_animation_init(void)
{
   da_init(anim.list);
   da_init(anim.pending);
}

void menu_animation_free(void)
{
   da_free(anim.list);
   da_free(anim.pending);
}

static void menu_delayed_animation_cb(void *userdata)
{
   menu_delayed_animation_t *delayed_animation = (menu_delayed_animation_t*) userdata;

   menu_animation_push(&delayed_animation->entry);

   free(delayed_animation);
}

void menu_animation_push_delayed(unsigned delay, menu_animation_ctx_entry_t *entry)
{
   menu_timer_ctx_entry_t timer_entry;
   menu_delayed_animation_t *delayed_animation  = (menu_delayed_animation_t*) malloc(sizeof(menu_delayed_animation_t));

   memcpy(&delayed_animation->entry, entry, sizeof(menu_animation_ctx_entry_t));

   timer_entry.cb       = menu_delayed_animation_cb;
   timer_entry.duration = delay;
   timer_entry.userdata = delayed_animation;

   menu_timer_start(&delayed_animation->timer, &timer_entry);
}

bool menu_animation_push(menu_animation_ctx_entry_t *entry)
{
   struct tween t;

   t.duration           = entry->duration;
   t.running_since      = 0;
   t.initial_value      = *entry->subject;
   t.target_value       = entry->target_value;
   t.subject            = entry->subject;
   t.tag                = entry->tag;
   t.cb                 = entry->cb;
   t.userdata           = entry->userdata;
   t.easing             = NULL;
   t.deleted            = false;

   switch (entry->easing_enum)
   {
      case EASING_LINEAR:
         t.easing       = &easing_linear;
         break;
         /* Quad */
      case EASING_IN_QUAD:
         t.easing       = &easing_in_quad;
         break;
      case EASING_OUT_QUAD:
         t.easing       = &easing_out_quad;
         break;
      case EASING_IN_OUT_QUAD:
         t.easing       = &easing_in_out_quad;
         break;
      case EASING_OUT_IN_QUAD:
         t.easing       = &easing_out_in_quad;
         break;
         /* Cubic */
      case EASING_IN_CUBIC:
         t.easing       = &easing_in_cubic;
         break;
      case EASING_OUT_CUBIC:
         t.easing       = &easing_out_cubic;
         break;
      case EASING_IN_OUT_CUBIC:
         t.easing       = &easing_in_out_cubic;
         break;
      case EASING_OUT_IN_CUBIC:
         t.easing       = &easing_out_in_cubic;
         break;
         /* Quart */
      case EASING_IN_QUART:
         t.easing       = &easing_in_quart;
         break;
      case EASING_OUT_QUART:
         t.easing       = &easing_out_quart;
         break;
      case EASING_IN_OUT_QUART:
         t.easing       = &easing_in_out_quart;
         break;
      case EASING_OUT_IN_QUART:
         t.easing       = &easing_out_in_quart;
         break;
         /* Quint */
      case EASING_IN_QUINT:
         t.easing       = &easing_in_quint;
         break;
      case EASING_OUT_QUINT:
         t.easing       = &easing_out_quint;
         break;
      case EASING_IN_OUT_QUINT:
         t.easing       = &easing_in_out_quint;
         break;
      case EASING_OUT_IN_QUINT:
         t.easing       = &easing_out_in_quint;
         break;
         /* Sine */
      case EASING_IN_SINE:
         t.easing       = &easing_in_sine;
         break;
      case EASING_OUT_SINE:
         t.easing       = &easing_out_sine;
         break;
      case EASING_IN_OUT_SINE:
         t.easing       = &easing_in_out_sine;
         break;
      case EASING_OUT_IN_SINE:
         t.easing       = &easing_out_in_sine;
         break;
         /* Expo */
      case EASING_IN_EXPO:
         t.easing       = &easing_in_expo;
         break;
      case EASING_OUT_EXPO:
         t.easing       = &easing_out_expo;
         break;
      case EASING_IN_OUT_EXPO:
         t.easing       = &easing_in_out_expo;
         break;
      case EASING_OUT_IN_EXPO:
         t.easing       = &easing_out_in_expo;
         break;
         /* Circ */
      case EASING_IN_CIRC:
         t.easing       = &easing_in_circ;
         break;
      case EASING_OUT_CIRC:
         t.easing       = &easing_out_circ;
         break;
      case EASING_IN_OUT_CIRC:
         t.easing       = &easing_in_out_circ;
         break;
      case EASING_OUT_IN_CIRC:
         t.easing       = &easing_out_in_circ;
         break;
         /* Bounce */
      case EASING_IN_BOUNCE:
         t.easing       = &easing_in_bounce;
         break;
      case EASING_OUT_BOUNCE:
         t.easing       = &easing_out_bounce;
         break;
      case EASING_IN_OUT_BOUNCE:
         t.easing       = &easing_in_out_bounce;
         break;
      case EASING_OUT_IN_BOUNCE:
         t.easing       = &easing_out_in_bounce;
         break;
      default:
         break;
   }

   /* ignore born dead tweens */
   if (!t.easing || t.duration == 0 || t.initial_value == t.target_value)
      return false;

   if (anim.in_update)
      da_push(anim.pending, t);
   else
      da_push(anim.list, t);

   return true;
}

bool menu_animation_update(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   menu_animation_update_time(settings->bools.menu_timedate_enable);

   anim.in_update       = true;
   anim.pending_deletes = false;

   for(i = 0; i < da_count(anim.list); i++)
   {
      struct tween *tween   = da_getptr(anim.list, i);
      tween->running_since += delta_time;

      *tween->subject = tween->easing(
            tween->running_since,
            tween->initial_value,
            tween->target_value - tween->initial_value,
            tween->duration);

      if (tween->running_since >= tween->duration)
      {
         *tween->subject = tween->target_value;

         if (tween->cb)
            tween->cb(tween->userdata);

         da_delete(anim.list, i);
         i--;
      }
   }

   if (anim.pending_deletes)
   {
      for(i = 0; i < da_count(anim.list); i++)
      {
         struct tween *tween = da_getptr(anim.list, i);
         if (tween->deleted)
         {
            da_delete(anim.list, i);
            i--;
         }
      }
      anim.pending_deletes = false;
   }

   if (da_count(anim.pending) > 0)
   {
      da_addn(anim.list, anim.pending.p, da_count(anim.pending));
      da_clear(anim.pending);
   }

   anim.in_update      = false;
   animation_is_active = da_count(anim.list) > 0;

   return animation_is_active;
}

bool menu_animation_ticker(const menu_animation_ctx_ticker_t *ticker)
{
   size_t str_len = utf8len(ticker->str);
   size_t offset  = 0;

   if ((size_t)str_len <= ticker->len)
   {
      utf8cpy(ticker->s,
            PATH_MAX_LENGTH,
            ticker->str,
            ticker->len);
      return false;
   }

   if (!ticker->selected)
   {
      utf8cpy(ticker->s, PATH_MAX_LENGTH, ticker->str, ticker->len - 3);
      strlcat(ticker->s, "...", PATH_MAX_LENGTH);
      return false;
   }

   if (str_len > ticker->len)
      menu_animation_ticker_generic(
            ticker->idx,
            ticker->len,
            &offset,
            &str_len);

   utf8cpy(
         ticker->s,
         PATH_MAX_LENGTH,
         utf8skip(ticker->str, offset),
         str_len);

   animation_is_active = true;

   return true;
}

void menu_animation_update_time(bool timedate_enable)
{
   static retro_time_t
      last_clock_update     = 0;

   cur_time                 = cpu_features_get_time_usec() / 1000.0f;
   delta_time               = old_time == 0 ? 0 : cur_time - old_time;
   old_time                 = cur_time;
   ticker_time              = (cur_time / 1000.0f) * 3.0f;

   if (((cur_time - last_clock_update) > 1000)
         && timedate_enable)
   {
      animation_is_active   = true;
      last_clock_update     = cur_time;
   }
}

bool menu_animation_is_active(void)
{
   return animation_is_active;
}

bool menu_animation_kill_by_tag(menu_animation_ctx_tag *tag)
{
   unsigned i;

   if (!tag || *tag == (uintptr_t)-1)
      return false;

   for (i = 0; i < da_count(anim.list); ++i)
   {
      struct tween *t = da_getptr(anim.list, i);
      if (t->tag != *tag)
         continue;

      if (anim.in_update)
      {
         t->deleted = true;
         anim.pending_deletes = true;
      }
      else
      {
         da_delete(anim.list, i);
         --i;
      }
   }

   return true;
}

void menu_animation_kill_by_subject(menu_animation_ctx_subject_t *subject)
{
   unsigned i, j,  killed = 0;
   float            **sub = (float**)subject->data;

   for (i = 0; i < da_count(anim.list) && killed < subject->count; ++i)
   {
      struct tween *t = da_getptr(anim.list, i);

      for (j = 0; j < subject->count; ++j)
      {
         if (t->subject != sub[j])
            continue;

         if (anim.in_update)
         {
            t->deleted = true;
            anim.pending_deletes = true;
         }
         else
         {
            da_delete(anim.list, i);
            --i;
         }

         killed++;
         break;
      }
   }
}

float menu_animation_get_delta_time(void)
{
   return delta_time;
}

bool menu_animation_ctl(enum menu_animation_ctl_state state, void *data)
{
   switch (state)
   {
      case MENU_ANIMATION_CTL_DEINIT:
         {
            size_t i;

            for (i = 0; i < da_count(anim.list); i++)
            {
               struct tween *t = da_getptr(anim.list, i);
               if (t->subject)
                  t->subject = NULL;
            }

            da_free(anim.list);

            memset(&anim, 0, sizeof(menu_animation_t));
         }
         cur_time                  = 0;
         old_time                  = 0;
         delta_time                = 0.0f;
         break;
      case MENU_ANIMATION_CTL_CLEAR_ACTIVE:
         animation_is_active       = false;
         break;
      case MENU_ANIMATION_CTL_SET_ACTIVE:
         animation_is_active       = true;
         break;
      case MENU_ANIMATION_CTL_NONE:
      default:
         break;
   }

   return true;
}

void menu_timer_start(menu_timer_t *timer, menu_timer_ctx_entry_t *timer_entry)
{
   menu_animation_ctx_entry_t entry;
   menu_animation_ctx_tag tag = (uintptr_t) timer;

   menu_timer_kill(timer);

   *timer = 0.0f;

   entry.easing_enum    = EASING_LINEAR;
   entry.tag            = tag;
   entry.duration       = timer_entry->duration;
   entry.target_value   = 1.0f;
   entry.subject        = timer;
   entry.cb             = timer_entry->cb;
   entry.userdata       = timer_entry->userdata;

   menu_animation_push(&entry);
}

void menu_timer_kill(menu_timer_t *timer)
{
   menu_animation_ctx_tag tag = (uintptr_t) timer;
   menu_animation_kill_by_tag(&tag);
}

float menu_animation_get_ticker_time(void)
{
   return ticker_time;
}
