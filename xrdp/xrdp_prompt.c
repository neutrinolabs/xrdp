
/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * prompt dialog
 */

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "string_calls.h"
#include "scp.h"

#define PROMPT_OK_BUT_ID    1
#define PROMPT_CAN_BUT_ID   2
#define PROMPT_EDIT_ID      3
#define PROMPT_NUM_LABELS   10
/* id 4 to 13 are the labels for text */
#define PROMPT_LABEL0_ID     4
#define PROMPT_LABEL1_ID     5
#define PROMPT_LABEL2_ID     6
#define PROMPT_LABEL3_ID     7
#define PROMPT_LABEL4_ID     8
#define PROMPT_LABEL5_ID     9
#define PROMPT_LABEL6_ID     10
#define PROMPT_LABEL7_ID     11
#define PROMPT_LABEL8_ID     12
#define PROMPT_LABEL9_ID     13

/*****************************************************************************/
static int
xrdp_wm_prompt_notify(struct xrdp_bitmap *wnd,
                      struct xrdp_bitmap *sender,
                      int msg, long param1, long param2)
{
    struct trans *sesman_trans;
    int error;
    struct xrdp_bitmap *edit;

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_wm_prompt_notify:");

    if ((sender == NULL) || (wnd == NULL) || (wnd->owner == NULL))
    {
        return 0;
    }

    if (msg == 1) /* click */
    {
        if (sender->id == PROMPT_OK_BUT_ID) /* ok button */
        {
            sesman_trans = wnd->wm->mm->sesman_trans;
            if (sesman_trans != NULL)
            {
                edit = xrdp_bitmap_get_child_by_id(wnd->wm->prompt_wnd,
                                                   PROMPT_EDIT_ID);
                if (edit != NULL)
                {
                    error = scp_send_prompt_response(sesman_trans,
                                                     edit->caption1);
                    LOG(LOG_LEVEL_INFO, "scp_send_prompt_response rv %d",
                        error);
                    g_memset(edit->caption1, 0, 256);
                    edit->edit_pos = 0;
                }
            }
        }
    }

    return 0;
}

/*****************************************************************************/
int
xrdp_prompt_create(struct xrdp_bitmap *wnd, struct xrdp_bitmap **aprompt_wnd)
{
    const char *ok_string = "OK";
    const char *can_string = "Cancel";
    int but_width;
    int but_height;
    int edit_width;
    int edit_height;
    int index;
    int margin_size;
    int wnd_width;
    int wnd_height;
    struct xrdp_painter *p;
    struct xrdp_bitmap *line_wnd;

    struct xrdp_bitmap *prompt_wnd;
    struct xrdp_bitmap *edit;
    struct xrdp_bitmap *can_but;
    struct xrdp_bitmap *ok_but;

    LOG(LOG_LEVEL_INFO, "xrdp_prompt_create:");

    margin_size = 15;
    wnd_width =  wnd->wm->xrdp_config->cfg_globals.ls_scaled.prompt_wnd_width;
    wnd_height = wnd->wm->xrdp_config->cfg_globals.ls_scaled.prompt_wnd_height;

    prompt_wnd = xrdp_bitmap_create(wnd_width, wnd_height,
                                    wnd->wm->screen->bpp,
                                    WND_TYPE_WND, wnd->wm);
    list_insert_item(wnd->wm->screen->child_list, 0, (long)prompt_wnd);

    prompt_wnd->parent = wnd->wm->screen;
    prompt_wnd->owner = wnd;
    wnd->modal_dialog = prompt_wnd;
    prompt_wnd->bg_color = wnd->wm->grey;
    prompt_wnd->left = wnd->wm->screen->width / 2 - prompt_wnd->width / 2;
    prompt_wnd->top = wnd->wm->screen->height / 2 - prompt_wnd->height / 2;
    prompt_wnd->notify = xrdp_wm_prompt_notify;
    set_string(&prompt_wnd->caption1, "Prompt");

    but_height = wnd->wm->xrdp_config->cfg_globals.ls_scaled.default_btn_height;
    p = xrdp_painter_create(wnd->wm, wnd->wm->session);
    xrdp_painter_font_needed(p);

    edit_width = prompt_wnd->width - margin_size * 2;
    edit_height = wnd->wm->xrdp_config->cfg_globals.ls_scaled.edit_height;

    /* response edit */
    edit = xrdp_bitmap_create(edit_width, edit_height, wnd->wm->screen->bpp,
                              WND_TYPE_EDIT, wnd->wm);
    list_insert_item(prompt_wnd->child_list, 0, (intptr_t)edit);
    edit->parent = prompt_wnd;
    edit->owner = prompt_wnd;
    edit->left = margin_size;
    edit->top = prompt_wnd->height - but_height - edit_height -
                margin_size * 2;
    edit->id = PROMPT_EDIT_ID;
    edit->tab_stop = 1;
    edit->pointer = 1; /* I beam */
    edit->caption1 = (char *)g_malloc(256, 1);

    /* cancel button */
    but_width = xrdp_painter_text_width(p, can_string) + DEFAULT_BUTTON_MARGIN_W;
    can_but = xrdp_bitmap_create(but_width, but_height, wnd->wm->screen->bpp,
                                 WND_TYPE_BUTTON, wnd->wm);
    list_insert_item(prompt_wnd->child_list, 0, (intptr_t)can_but);
    can_but->parent = prompt_wnd;
    can_but->owner = prompt_wnd;
    can_but->left = prompt_wnd->width - but_width - margin_size;
    can_but->top = prompt_wnd->height - but_height - margin_size;
    can_but->id = PROMPT_CAN_BUT_ID;
    can_but->tab_stop = 1;
    set_string(&can_but->caption1, can_string);

    /* ok button */
    but_width = xrdp_painter_text_width(p, ok_string) + DEFAULT_BUTTON_MARGIN_W;
    ok_but = xrdp_bitmap_create(but_width, but_height, wnd->wm->screen->bpp,
                                WND_TYPE_BUTTON, wnd->wm);
    list_insert_item(prompt_wnd->child_list, 0, (intptr_t)ok_but);
    ok_but->parent = prompt_wnd;
    ok_but->owner = prompt_wnd;
    ok_but->left = can_but->left - but_width - margin_size;
    ok_but->top = prompt_wnd->height - but_height - margin_size;
    ok_but->id = PROMPT_OK_BUT_ID;
    ok_but->tab_stop = 1;
    set_string(&ok_but->caption1, ok_string);

    for (index = 0; index < PROMPT_NUM_LABELS; index++)
    {
        line_wnd = xrdp_bitmap_create(edit_width, edit_height,
                                      wnd->wm->screen->bpp,
                                      WND_TYPE_LABEL, wnd->wm);
        list_insert_item(prompt_wnd->child_list, 0, (intptr_t) line_wnd);
        line_wnd->parent = prompt_wnd;
        line_wnd->owner = prompt_wnd;
        line_wnd->left = margin_size;
        line_wnd->top = edit->top - edit_height;
        line_wnd->top -= edit_height * index;
        line_wnd->id = PROMPT_LABEL0_ID + index;
    }

    xrdp_painter_delete(p);

    prompt_wnd->default_button = ok_but;
    prompt_wnd->focused_control = edit;

    xrdp_bitmap_invalidate(prompt_wnd, 0);
    xrdp_wm_set_focused(wnd->wm, prompt_wnd);

    *aprompt_wnd = prompt_wnd;

    return 0;
}

/*****************************************************************************/
static int
xrdp_prompt_add_prompt_scroll(struct xrdp_bitmap *prompt_wnd,
                              const char *line_text)
{
    struct xrdp_bitmap *line_wnd;
    struct xrdp_bitmap *line_wnd1;
    int index;

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt2: line_text [%s]",
              line_text);
    line_wnd1 = NULL;
    if ((line_text != NULL) && (line_text[0] != 0))
    {
        for (index = 0; index < PROMPT_NUM_LABELS - 1; index++)
        {
            line_wnd = xrdp_bitmap_get_child_by_id(prompt_wnd,
                                                   PROMPT_LABEL9_ID - index);
            line_wnd1 = xrdp_bitmap_get_child_by_id(prompt_wnd,
                                                    PROMPT_LABEL8_ID - index);
            LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt2: "
                      "line_wnd %p line_wnd1 %p", line_wnd, line_wnd1);
            if ((line_wnd != NULL) && (line_wnd1 != NULL))
            {
                set_string(&line_wnd->caption1, line_wnd1->caption1);
            }
        }
        line_wnd = xrdp_bitmap_get_child_by_id(prompt_wnd, PROMPT_LABEL0_ID);
        if (line_wnd != NULL)
        {
            set_string(&line_wnd->caption1, line_text);
        }
    }
    return 0;
}

/*****************************************************************************/
static int
xrdp_prompt_add_prompt_wrap(struct xrdp_bitmap *prompt_wnd,
                            const char *line_text,
                            struct xrdp_painter *p)
{
    int text_width;
    int label_width;
    int dest_idx;
    int save_dest_idx;
    int save_count;
    char line[256];
    struct xrdp_bitmap *label_wnd;
    const char *src;
    char *dest;
    char32_t ch;

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt1: line_text [%s]",
              line_text);
    if (line_text == NULL)
    {
        return 0;
    }
    if (line_text[0] == 0)
    {
        return 0;
    }
    label_wnd = xrdp_bitmap_get_child_by_id(prompt_wnd, PROMPT_LABEL0_ID);
    if (label_wnd == NULL)
    {
        return 1;
    }
    label_width = label_wnd->width;
    text_width = xrdp_painter_text_width(p, line_text);
    if (text_width <= label_width)
    {
        return xrdp_prompt_add_prompt_scroll(prompt_wnd, line_text);
    }
    // have to wrap
    dest_idx = 0;
    save_dest_idx = -1;
    dest = line;
    src = line_text;
    while (src[0] != 0)
    {
        ch = utf8_get_next_char(&src, NULL);
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt1: ch [%d] "
                  "src [%s]", ch, src);
        if (ch == ' ')
        {
            /* Record this as a potential break point */
            save_dest_idx = dest_idx;
        }
        dest_idx += utf_char32_to_utf8(ch, dest + dest_idx);
        dest[dest_idx] = 0;
        LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt1: dest_idx [%d] "
                  "dest [%s]", dest_idx, dest);
        if (dest_idx >= sizeof(line) - 6)
        {
            return 1;
        }
        /* check line width */
        if (xrdp_painter_text_width(p, dest) > label_width)
        {
            if (save_dest_idx == -1)
            {
                xrdp_prompt_add_prompt_scroll(prompt_wnd, dest);
                dest[0] = 0;
                dest_idx = 0;
            }
            else
            {
                ch = dest[save_dest_idx];
                dest[save_dest_idx] = 0;
                xrdp_prompt_add_prompt_scroll(prompt_wnd, dest);
                dest[save_dest_idx] = ch;
                /* save_dest_idx should always be < dest_idx */
                save_dest_idx++; /* skip space */
                save_count = dest_idx - save_dest_idx;
                g_memmove(dest, dest + save_dest_idx, save_count);
                dest_idx = save_count;
                dest[dest_idx] = 0;
                save_dest_idx = -1;
            }
        }
    }
    xrdp_prompt_add_prompt_scroll(prompt_wnd, dest);
    return 0;
}

/*****************************************************************************/
int
xrdp_prompt_add_prompt(struct xrdp_bitmap *prompt_wnd, const char *prompt,
                       int hide_chars)
{
    int index;
    int jndex;
    char line[256];
    struct xrdp_painter *p;
    struct xrdp_bitmap *wnd;
    struct xrdp_bitmap *edit;

    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt:");
    LOG_DEVEL(LOG_LEVEL_INFO, "xrdp_prompt_add_prompt: prompt %s", prompt);

    wnd = prompt_wnd;
    p = xrdp_painter_create(wnd->wm, wnd->wm->session);
    xrdp_painter_font_needed(p);

    /* the prompt may contain many lines, here we seperate them out */
    line[0] = 0;
    index = 0;
    jndex = 0;
    while (prompt[jndex] != 0)
    {
        if (prompt[jndex] < 0x20)
        {
            xrdp_prompt_add_prompt_wrap(wnd, line, p);
            line[0] = 0;
            index = 0;
            jndex++;
            continue;
        }
        line[index] = prompt[jndex];
        index = index < (sizeof(line) - 1) ? index + 1 : (sizeof(line) - 1);
        line[index] = 0;
        jndex++;
    }
    xrdp_prompt_add_prompt_wrap(wnd, line, p);
    xrdp_painter_delete(p);

    edit = xrdp_bitmap_get_child_by_id(wnd, PROMPT_EDIT_ID);
    if (edit != 0)
    {
        edit->password_char = hide_chars ? '*' : 0;
    }

    xrdp_bitmap_invalidate(wnd, 0);
    return 0;
}
