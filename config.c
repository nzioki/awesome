/*
 * config.c - configuration management
 *
 * Copyright © 2007 Julien Danjou <julien@danjou.info>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/**
 * \defgroup ui_callback
 */

#include <confuse.h>
#include <X11/keysym.h>

#include "util.h"
#include "awesome.h"
#include "screen.h"
#include "draw.h"
#include "tag.h"
#include "rules.h"
#include "statusbar.h"
#include "layout.h"
#include "mouse.h"
#include "layouts/tile.h"
#include "layouts/floating.h"
#include "layouts/max.h"

#define AWESOME_CONFIG_FILE ".awesomerc" 

static XColor initxcolor(Display *, int, const char *);
static unsigned int get_numlockmask(Display *);

/** Link a name to a key symbol */
typedef struct
{
    const char *name;
    KeySym keysym;
} KeyMod;

/** Link a name to a mouse button symbol */
typedef struct
{
    const char *name;
    unsigned int button;
} MouseButton;

/** List of available UI bindable callbacks and functions */
const NameFuncLink UicbList[] = {
    /* util.c */
    {"spawn", uicb_spawn},
    {"exec", uicb_exec},
    /* client.c */
    {"client_kill", uicb_client_kill},
    {"client_moveresize", uicb_client_moveresize},
    {"client_settrans", uicb_client_settrans},
    {"setborder", uicb_setborder},
    {"client_swapnext", uicb_client_swapnext},
    {"client_swapprev", uicb_client_swapprev},
    /* tag.c */
    {"client_tag", uicb_client_tag},
    {"client_togglefloating", uicb_client_togglefloating},
    {"tag_toggleview", uicb_tag_toggleview},
    {"client_toggletag", uicb_client_toggletag},
    {"tag_view", uicb_tag_view},
    {"tag_view_prev_selected", uicb_tag_prev_selected},
    {"tag_view_prev", uicb_tag_viewprev},
    {"tag_view_next", uicb_tag_viewnext},
    /* layout.c */
    {"setlayout", uicb_setlayout},
    {"client_focusnext", uicb_client_focusnext},
    {"client_focusprev", uicb_client_focusprev}, 
    {"client_togglemax", uicb_client_togglemax},
    {"client_toggleverticalmax", uicb_client_toggleverticalmax},
    {"client_togglehorizontalmax", uicb_client_togglehorizontalmax},
    {"client_zoom", uicb_client_zoom},
    /* layouts/tile.c */
    {"tag_setmwfact", uicb_tag_setmwfact},
    {"tag_setnmaster", uicb_tag_setnmaster},
    {"tag_setncol", uicb_tag_setncol},
    /* screen.c */
    {"screen_focusnext", uicb_screen_focusnext},
    {"screen_focusprev", uicb_screen_focusprev},
    {"client_movetoscreen", uicb_client_movetoscreen},
    /* awesome.c */
    {"quit", uicb_quit},
    /* statusbar.c */
    {"togglebar", uicb_togglebar},
    /* config.c */
    {"reloadconfig", uicb_reloadconfig},
    {"setstatustext", uicb_setstatustext},
    /* mouse.c */
    {"client_movemouse", uicb_client_movemouse},
    {"client_resizemouse", uicb_client_resizemouse},
    {NULL, NULL}
};

/** List of keyname and corresponding X11 mask codes */
static const KeyMod KeyModList[] =
{
    {"Shift", ShiftMask},
    {"Lock", LockMask},
    {"Control", ControlMask},
    {"Mod1", Mod1Mask},
    {"Mod2", Mod2Mask},
    {"Mod3", Mod3Mask},
    {"Mod4", Mod4Mask},
    {"Mod5", Mod5Mask},
    {NULL, NoSymbol}
};

/** List of button name and corresponding X11 mask codes */
static const MouseButton MouseButtonList[] =
{
    {"1", Button1},
    {"2", Button2},
    {"3", Button3},
    {"4", Button4},
    {"5", Button5},
    {NULL, 0}
};
/** List of available layouts and link between name and functions */
static const NameFuncLink LayoutsList[] =
{
    {"tile", layout_tile},
    {"tileleft", layout_tileleft},
    {"max", layout_max},
    {"floating", layout_floating},
    {NULL, NULL}
};

/** Lookup for a key mask from its name
 * \param keyname Key name
 * \return Key mask or 0 if not found
 */
static KeySym
key_mask_lookup(const char *keyname)
{
    int i;

    if(keyname)
        for(i = 0; KeyModList[i].name; i++)
            if(!a_strcmp(keyname, KeyModList[i].name))
                return KeyModList[i].keysym;

    return NoSymbol;
}

/** Lookup for a mouse button from its name
 * \param button Mouse button name
 * \return Mouse button or 0 if not found
 */
static unsigned int
mouse_button_lookup(const char *button)
{
    int i;
    
    if(button)
        for(i = 0; MouseButtonList[i].name; i++)
            if(!a_strcmp(button, MouseButtonList[i].name))
                return MouseButtonList[i].button;

    return 0;
}

static Button *
parse_mouse_bindings(cfg_t * cfg, const char *secname, Bool handle_arg)
{
    unsigned int i, j;
    cfg_t *cfgsectmp;
    Button *b = NULL, *head = NULL;

    /* Mouse: layout click bindings */
    for(i = 0; i < cfg_size(cfg, secname); i++)
    {
        /* init first elem */
        if(i == 0)
            head = b = p_new(Button, 1);

        cfgsectmp = cfg_getnsec(cfg, secname, i);
        for(j = 0; j < cfg_size(cfgsectmp, "modkey"); j++)
            b->mod |= key_mask_lookup(cfg_getnstr(cfgsectmp, "modkey", j));
        b->button = mouse_button_lookup(cfg_getstr(cfgsectmp, "button"));
        b->func = name_func_lookup(cfg_getstr(cfgsectmp, "command"), UicbList);
        if(handle_arg)
            b->arg = a_strdup(cfg_getstr(cfgsectmp, "arg"));
        else
            b->arg = NULL;

        /* switch to next elem or finalize the list */
        if(i < cfg_size(cfg, secname) - 1)
        {
            b->next = p_new(Button, 1);
            b = b->next;
        }
        else
            b->next = NULL;
    }

    return head;
}

/** Parse configuration file and initialize some stuff
 * \param disp Display ref
 * \param scr Screen number
 */
void
parse_config(const char *confpatharg, awesome_config *awesomeconf)
{
    static cfg_opt_t general_opts[] =
    {
        CFG_INT((char *) "border", 1, CFGF_NONE),
        CFG_INT((char *) "snap", 8, CFGF_NONE),
        CFG_BOOL((char *) "resize_hints", cfg_false, CFGF_NONE),
        CFG_INT((char *) "opacity_unfocused", 100, CFGF_NONE),
        CFG_BOOL((char *) "focus_move_pointer", cfg_false, CFGF_NONE),
        CFG_BOOL((char *) "allow_lower_floats", cfg_false, CFGF_NONE),
        CFG_STR((char *) "font", (char *) "mono-12", CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t colors_opts[] =
    {
        CFG_STR((char *) "normal_border", (char *) "#111111", CFGF_NONE),
        CFG_STR((char *) "normal_bg", (char *) "#111111", CFGF_NONE),
        CFG_STR((char *) "normal_fg", (char *) "#eeeeee", CFGF_NONE),
        CFG_STR((char *) "focus_border", (char *) "#6666ff", CFGF_NONE),
        CFG_STR((char *) "focus_bg", (char *) "#6666ff", CFGF_NONE),
        CFG_STR((char *) "focus_fg", (char *) "#ffffff", CFGF_NONE),
        CFG_STR((char *) "tab_border", (char *) "#ff0000", CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t statusbar_opts[] =
    {
        CFG_STR((char *) "position", (char *) "top", CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t tag_opts[] =
    {
        CFG_STR((char *) "layout", (char *) "tile", CFGF_NONE),
        CFG_FLOAT((char *) "mwfact", 0.5, CFGF_NONE),
        CFG_INT((char *) "nmaster", 1, CFGF_NONE),
        CFG_INT((char *) "ncol", 1, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t tags_opts[] =
    {
        CFG_SEC((char *) "tag", tag_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()
    };
    static cfg_opt_t layout_opts[] =
    {
        CFG_STR((char *) "symbol", (char *) "???", CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t layouts_opts[] =
    {
        CFG_SEC((char *) "layout", layout_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()
    };
    static cfg_opt_t screen_opts[] =
    {
        CFG_SEC((char *) "general", general_opts, CFGF_NONE),
        CFG_SEC((char *) "statusbar", statusbar_opts, CFGF_NONE),
        CFG_SEC((char *) "tags", tags_opts, CFGF_NONE),
        CFG_SEC((char *) "colors", colors_opts, CFGF_NONE),
        CFG_SEC((char *) "layouts", layouts_opts, CFGF_NONE),
    };
    static cfg_opt_t rule_opts[] =
    {
        CFG_STR((char *) "name", (char *) "", CFGF_NONE),
        CFG_STR((char *) "tags", (char *) "", CFGF_NONE),
        CFG_BOOL((char *) "float", cfg_false, CFGF_NONE),
        CFG_INT((char *) "screen", RULE_NOSCREEN, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t rules_opts[] =
    {
        CFG_SEC((char *) "rule", rule_opts, CFGF_MULTI),
        CFG_END()
    };
    static cfg_opt_t key_opts[] =
    {
        CFG_STR_LIST((char *) "modkey", (char *) "{Mod4}", CFGF_NONE),
        CFG_STR((char *) "key", (char *) "None", CFGF_NONE),
        CFG_STR((char *) "command", (char *) "", CFGF_NONE),
        CFG_STR((char *) "arg", NULL, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t keys_opts[] =
    {
        CFG_SEC((char *) "key", key_opts, CFGF_MULTI),
        CFG_END()
    };
    static cfg_opt_t mouse_tag_opts[] =
    {
        CFG_STR_LIST((char *) "modkey", (char *) "{}", CFGF_NONE),
        CFG_STR((char *) "button", (char *) "None", CFGF_NONE),
        CFG_STR((char *) "command", (char *) "", CFGF_NONE),
    };
    static cfg_opt_t mouse_generic_opts[] =
    {
        CFG_STR_LIST((char *) "modkey", (char *) "{}", CFGF_NONE),
        CFG_STR((char *) "button", (char *) "None", CFGF_NONE),
        CFG_STR((char *) "command", (char *) "", CFGF_NONE),
        CFG_STR((char *) "arg", NULL, CFGF_NONE),
    };
    static cfg_opt_t mouse_opts[] =
    {
        CFG_STR((char *) "modkey", (char *) "Mod4", CFGF_NONE),
        CFG_SEC((char *) "tag", mouse_tag_opts, CFGF_MULTI),
        CFG_SEC((char *) "layout", mouse_generic_opts, CFGF_MULTI),
        CFG_SEC((char *) "title", mouse_generic_opts, CFGF_MULTI),
        CFG_SEC((char *) "root", mouse_generic_opts, CFGF_MULTI),
        CFG_SEC((char *) "client", mouse_generic_opts, CFGF_MULTI),
        CFG_END()
    };
    static cfg_opt_t opts[] =
    {
        CFG_SEC((char *) "screen", screen_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_SEC((char *) "rules", rules_opts, CFGF_NONE),
        CFG_SEC((char *) "keys", keys_opts, CFGF_NONE),
        CFG_SEC((char *) "mouse", mouse_opts, CFGF_NONE),
        CFG_END()
    };
    cfg_t *cfg, *cfg_general, *cfg_colors, *cfg_screen, *cfg_statusbar,
          *cfg_tags, *cfg_layouts, *cfg_rules, *cfg_keys, *cfg_mouse, *cfgsectmp;
    int i = 0, k = 0, ret;
    unsigned int j = 0, l = 0;
    const char *tmp, *homedir;
    char *confpath, buf[2];
    ssize_t confpath_len;
    Key *key = NULL;
    Rule *rule = NULL;

    if(confpatharg)
        confpath = a_strdup(confpatharg);
    else
    {
        homedir = getenv("HOME");
        confpath_len = a_strlen(homedir) + a_strlen(AWESOME_CONFIG_FILE) + 2;
        confpath = p_new(char, confpath_len);
        a_strcpy(confpath, confpath_len, homedir);
        a_strcat(confpath, confpath_len, "/");
        a_strcat(confpath, confpath_len, AWESOME_CONFIG_FILE);
    }

    awesomeconf->configpath = a_strdup(confpath);

    a_strcpy(awesomeconf->statustext, sizeof(awesomeconf->statustext), "awesome-" VERSION " (" RELEASE ")");

    awesomeconf->phys_screen = get_phys_screen(awesomeconf->display, awesomeconf->screen);

    cfg = cfg_init(opts, CFGF_NONE);

    ret = cfg_parse(cfg, confpath);
    if(ret == CFG_FILE_ERROR)
        perror("awesome: parsing configuration file failed");
    else if(ret == CFG_PARSE_ERROR)
        cfg_error(cfg, "awesome: parsing configuration file %s failed.\n", confpath);

    /* get the right screen section */
    snprintf(buf, sizeof(buf), "%d", awesomeconf->screen);
    cfg_screen = cfg_gettsec(cfg, "screen", buf);
    if(!cfg_screen)
        cfg_screen = cfg_getsec(cfg, "screen");

    /* get screen specific sections */
    cfg_statusbar = cfg_getsec(cfg_screen, "statusbar");
    cfg_tags = cfg_getsec(cfg_screen, "tags");
    cfg_colors = cfg_getsec(cfg_screen, "colors");
    cfg_general = cfg_getsec(cfg_screen, "general");
    cfg_layouts = cfg_getsec(cfg_screen, "layouts");

    /* get general sections */
    cfg_rules = cfg_getsec(cfg, "rules");
    cfg_keys = cfg_getsec(cfg, "keys");
    cfg_mouse = cfg_getsec(cfg, "mouse");

    /* General section */

    awesomeconf->borderpx = cfg_getint(cfg_general, "border");
    awesomeconf->snap = cfg_getint(cfg_general, "snap");
    awesomeconf->resize_hints = cfg_getbool(cfg_general, "resize_hints");
    awesomeconf->opacity_unfocused = cfg_getint(cfg_general, "opacity_unfocused");
    awesomeconf->focus_move_pointer = cfg_getbool(cfg_general, "focus_move_pointer");
    awesomeconf->allow_lower_floats = cfg_getbool(cfg_general, "allow_lower_floats");
    awesomeconf->font = XftFontOpenName(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_general, "font"));
    if(!awesomeconf->font)
        eprint("awesome: cannot init font\n");

    /* Colors */
    awesomeconf->colors_normal[ColBorder] = initxcolor(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_colors, "normal_border"));
    awesomeconf->colors_normal[ColBG] = initxcolor(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_colors, "normal_bg"));
    awesomeconf->colors_normal[ColFG] = initxcolor(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_colors, "normal_fg"));
    awesomeconf->colors_selected[ColBorder] = initxcolor(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_colors, "focus_border"));
    awesomeconf->colors_selected[ColBG] = initxcolor(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_colors, "focus_bg"));
    awesomeconf->colors_selected[ColFG] = initxcolor(awesomeconf->display, awesomeconf->phys_screen, cfg_getstr(cfg_colors, "focus_fg"));

    /* Statusbar */
    tmp = cfg_getstr(cfg_statusbar, "position");

    if(tmp && !a_strncmp(tmp, "off", 6))
        awesomeconf->statusbar.dposition = BarOff;
    else if(tmp && !a_strncmp(tmp, "bottom", 6))
        awesomeconf->statusbar.dposition = BarBot;
    else if(tmp && !a_strncmp(tmp, "right", 5))
        awesomeconf->statusbar.dposition = BarRight;
    else if(tmp && !a_strncmp(tmp, "left", 4))
        awesomeconf->statusbar.dposition = BarLeft;
    else
        awesomeconf->statusbar.dposition = BarTop;

    awesomeconf->statusbar.position = awesomeconf->statusbar.dposition;

    /* Layouts */
    awesomeconf->nlayouts = cfg_size(cfg_layouts, "layout");
    awesomeconf->layouts = p_new(Layout, awesomeconf->nlayouts);
    for(i = 0; i < awesomeconf->nlayouts; i++)
    {
        cfgsectmp = cfg_getnsec(cfg_layouts, "layout", i);
        awesomeconf->layouts[i].arrange = name_func_lookup(cfg_title(cfgsectmp), LayoutsList);
        if(!awesomeconf->layouts[i].arrange)
        {
            fprintf(stderr, "awesome: unknown layout #%d in configuration file\n", i);
            awesomeconf->layouts[i].symbol = NULL;
            continue;
        }
        awesomeconf->layouts[i].symbol = a_strdup(cfg_getstr(cfgsectmp, "symbol"));
    }

    if(!awesomeconf->nlayouts)
        eprint("awesome: fatal: no default layout available\n");

    /* Rules */
    if(cfg_size(cfg_rules, "rule"))
    {
        awesomeconf->rules = rule = p_new(Rule, 1);
        for(j = 0; j < cfg_size(cfg_rules, "rule"); j++)
        {
            cfgsectmp = cfg_getnsec(cfg_rules, "rule", j);
            rule->prop = a_strdup(cfg_getstr(cfgsectmp, "name"));
            rule->tags = a_strdup(cfg_getstr(cfgsectmp, "tags"));
            if(!a_strlen(rule->tags))
                rule->tags = NULL;
            rule->isfloating = cfg_getbool(cfgsectmp, "float");
            rule->screen = cfg_getint(cfgsectmp, "screen");
            if(rule->screen >= get_screen_count(awesomeconf->display))
                rule->screen = 0;

            if(j < cfg_size(cfg_rules, "rule") - 1)
            {
                rule->next = p_new(Rule, 1);
                rule = rule->next;
            }
            else
                rule->next = NULL;
        }
    }
    else
        awesomeconf->rules = NULL;

    compileregs(awesomeconf->rules);

    /* Tags */
    awesomeconf->ntags = cfg_size(cfg_tags, "tag");
    awesomeconf->tags = p_new(Tag, awesomeconf->ntags);
    for(i = 0; i < awesomeconf->ntags; i++)
    {
        cfgsectmp = cfg_getnsec(cfg_tags, "tag", i);
        awesomeconf->tags[i].name = a_strdup(cfg_title(cfgsectmp));
        awesomeconf->tags[i].selected = False;
        awesomeconf->tags[i].was_selected = False;
        tmp = cfg_getstr(cfgsectmp, "layout");
        for(k = 0; k < awesomeconf->nlayouts; k++)
            if(awesomeconf->layouts[k].arrange == name_func_lookup(tmp, LayoutsList))
                break;
        if(k == awesomeconf->nlayouts)
            k = 0;
        awesomeconf->tags[i].layout = &awesomeconf->layouts[k];
        awesomeconf->tags[i].mwfact = cfg_getfloat(cfgsectmp, "mwfact");
        awesomeconf->tags[i].nmaster = cfg_getint(cfgsectmp, "nmaster");
        awesomeconf->tags[i].ncol = cfg_getint(cfgsectmp, "ncol");
    }

    
    if(!awesomeconf->ntags)
        eprint("awesome: fatal: no tags found in configuration file\n");

    /* select first tag by default */
    awesomeconf->tags[0].selected = True;
    awesomeconf->tags[0].was_selected = True;

    /* Mouse */
    if(!(awesomeconf->modkey = key_mask_lookup(cfg_getstr(cfg_mouse, "modkey"))))
       awesomeconf->modkey = Mod4Mask;

    /* Mouse: tags click bindings */
    awesomeconf->buttons.tag = parse_mouse_bindings(cfg_mouse, "tag", False);

    /* Mouse: layout click bindings */
    awesomeconf->buttons.layout = parse_mouse_bindings(cfg_mouse, "layout", True);

    /* Mouse: title click bindings */
    awesomeconf->buttons.title = parse_mouse_bindings(cfg_mouse, "title", True);

    /* Mouse: root window click bindings */
    awesomeconf->buttons.root = parse_mouse_bindings(cfg_mouse, "root", True);

    /* Mouse: client windows click bindings */
    awesomeconf->buttons.client = parse_mouse_bindings(cfg_mouse, "client", True);

    /* Keys */
    awesomeconf->numlockmask = get_numlockmask(awesomeconf->display);

    if(cfg_size(cfg_keys, "key"))
    {
        awesomeconf->keys = key = p_new(Key, 1);
        for(j = 0; j < cfg_size(cfg_keys, "key"); j++)
        {
            cfgsectmp = cfg_getnsec(cfg_keys, "key", j);
            for(l = 0; l < cfg_size(cfgsectmp, "modkey"); l++)
                key->mod |= key_mask_lookup(cfg_getnstr(cfgsectmp, "modkey", l));
            key->keysym = XStringToKeysym(cfg_getstr(cfgsectmp, "key"));
            key->func = name_func_lookup(cfg_getstr(cfgsectmp, "command"), UicbList);
            key->arg = a_strdup(cfg_getstr(cfgsectmp, "arg"));

            if(j < cfg_size(cfg_keys, "key") - 1)
            {
                key->next = p_new(Key, 1);
                key = key->next;
            }
            else
                key->next = NULL;
        }
    }
    else
        awesomeconf->keys = NULL;

    /* Free! Like a river! */
    cfg_free(cfg);
    p_delete(&confpath);
}

static unsigned int
get_numlockmask(Display *disp)
{
    XModifierKeymap *modmap;
    unsigned int mask = 0;
    int i, j;

    modmap = XGetModifierMapping(disp);
    for(i = 0; i < 8; i++)
        for(j = 0; j < modmap->max_keypermod; j++)
            if(modmap->modifiermap[i * modmap->max_keypermod + j]
               == XKeysymToKeycode(disp, XK_Num_Lock))
                mask = (1 << i);

    XFreeModifiermap(modmap);

    return mask;
}

/** Initialize color from X side
 * \param colorstr Color code
 * \param disp Display ref
 * \param scr Screen number
 * \return XColor pixel
 */
static XColor
initxcolor(Display *disp, int scr, const char *colstr)
{
    XColor color;
    if(!XAllocNamedColor(disp, DefaultColormap(disp, scr), colstr, &color, &color))
        die("awesome: error, cannot allocate color '%s'\n", colstr);
    return color;
}

void
uicb_reloadconfig(awesome_config *awesomeconf,
                  const char *arg __attribute__ ((unused)))
{
    int i, j, tag, screen, screen_count = get_screen_count(awesomeconf->display);
    awesome_config *awesomeconf_first = &awesomeconf[-awesomeconf->screen];
    int *old_ntags, old_c_ntags, new_c_ntags, **mapping;
    char ***savetagnames;
    Client ***savetagclientsel;
    char *configpath = a_strdup(awesomeconf_first->configpath);
    Bool ***savetagselected;
    Bool *old_c_tags;
    Client *c, *clients;

    /* Save tag information */
    savetagnames = p_new(char **, screen_count);
    savetagclientsel = p_new(Client **, screen_count);
    savetagselected = p_new(Bool **, screen_count);
    clients = *awesomeconf_first->clients;
    for (screen = 0; screen < screen_count; screen ++)
    {
       savetagnames[screen] = p_new(char *, awesomeconf_first[screen].ntags);
       savetagclientsel[screen] = p_new(Client *, awesomeconf_first[screen].ntags);
       savetagselected[screen] = p_new(Bool *, awesomeconf_first[screen].ntags);
       for (tag = 0; tag < awesomeconf_first[screen].ntags; tag++)
       {
           savetagnames[screen][tag] = a_strdup(awesomeconf_first[screen].tags[tag].name);
           savetagclientsel[screen][tag] = awesomeconf_first[screen].tags[tag].client_sel;
           savetagselected[screen][tag] = p_new(Bool, 2);
           savetagselected[screen][tag][0] = awesomeconf_first[screen].tags[tag].selected;
           savetagselected[screen][tag][1] = awesomeconf_first[screen].tags[tag].was_selected;
       }
    }
    old_ntags = p_new(int, screen_count);
    for (screen = 0; screen < screen_count; screen ++)
       old_ntags[screen] = awesomeconf_first[screen].ntags;

    mapping = p_new(int*, screen_count);
    for(screen = 0; screen < screen_count; screen++)
    {
        /* Cleanup screens and reload their config. */
        cleanup_screen(&awesomeconf_first[screen]);
        setup_screen(&awesomeconf_first[screen], configpath);

        /* Compute a mapping of tags between the old and new config, based on
         * tag names. */
        mapping[screen] = p_new(int, awesomeconf_first[screen].ntags);
        for (i = 0; i < awesomeconf_first[screen].ntags; i ++)
        {
            mapping[screen][i] = -1;
            for (j = 0; j < old_ntags[screen]; j ++)
                if (!a_strcmp(savetagnames[screen][j], awesomeconf_first[screen].tags[i].name))
                {
                    mapping[screen][i] = j;
                    break;
                }
        }

        /* Reinitialize the tags' client lists and selected client. */
        *awesomeconf_first[screen].clients = clients;
        for (tag = 0; tag < awesomeconf_first[screen].ntags; tag++)
            if (mapping[screen][tag] >= 0)
            {
                awesomeconf_first[screen].tags[tag].client_sel = savetagclientsel[screen][mapping[screen][tag]];
                awesomeconf_first[screen].tags[tag].selected = savetagselected[screen][mapping[screen][tag]][0];
                awesomeconf_first[screen].tags[tag].was_selected = savetagselected[screen][mapping[screen][tag]][1];
            }
        drawstatusbar(&awesomeconf_first[screen]);
    }

    /* Reinitialize the 'tags' array of each client.
     * Clients are assigned to the tags of the same name as in the previous
     * awesomerc, or to tag #1 otherwise. */
    for (c = *awesomeconf_first->clients; c; c = c->next)
    {
       old_c_ntags = old_ntags[c->screen];
       new_c_ntags = awesomeconf_first[c->screen].ntags;

       old_c_tags = c->tags;
       c->tags = p_new(Bool, new_c_ntags);
       for (i = 0; i < new_c_ntags; i ++)
          if (mapping[c->screen][i] >= 0)
             c->tags[i] = old_c_tags[mapping[c->screen][i]];
       p_delete(&old_c_tags);

       for (i = 0; i < new_c_ntags && c->tags[i] == 0; i++) {}
       if (i == new_c_ntags)
          c->tags[0] = 1;

       saveprops(c, awesomeconf_first[c->screen].ntags);
    }

    /* Cleanup after ourselves */
    for(screen = 0; screen < screen_count; screen++)
    {
        for(i = 0; i < old_ntags[screen]; i++)
        {
            p_delete(&savetagnames[screen][i]);
            p_delete(&savetagselected[screen][i]);
        }
        p_delete(&savetagselected[screen]);
        p_delete(&savetagnames[screen]);
        p_delete(&mapping[screen]);
        p_delete(&savetagclientsel[screen]);
    }
    p_delete(&mapping);
    p_delete(&savetagselected);
    p_delete(&savetagnames);
    p_delete(&old_ntags);
    p_delete(&savetagclientsel);
    p_delete(&configpath);
    for (screen = 0; screen < screen_count; screen ++)
        arrange(&awesomeconf_first[screen]);
}

// vim: filetype=c:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99
