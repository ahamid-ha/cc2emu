#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <confuse.h>
#include <errno.h>
#include <SDL3/SDL_filesystem.h>
#include "settings.h"
#include "utils.h"

#define ORG_NAME "cc2emu"
#define APP_NAME "cc2emu"

#define ROM_BASIC_DEFAULT_PATH "basic.rom"
#define ROM_EXTENDED_BASIC_DEFAULT_PATH "extbasic.rom"
#define ROM_DISK_BASIC_DEFAULT_PATH "dskbasic.rom"

struct app_settings app_settings;
cfg_t *cfg;

void settings_init(void) {
    memset(&app_settings, 0, sizeof(struct app_settings));

    app_settings.joy_emulation_mode[0] = Joy_Emulation_Keyboard;
    app_settings.joy_emulation_mode[1] = Joy_Emulation_None;

    app_settings.rom_basic_path = strdup(ROM_BASIC_DEFAULT_PATH);
    if (is_file_readable(ROM_EXTENDED_BASIC_DEFAULT_PATH))
        app_settings.rom_extended_basic_path = strdup(ROM_EXTENDED_BASIC_DEFAULT_PATH);
    if (is_file_readable(ROM_DISK_BASIC_DEFAULT_PATH))
        app_settings.rom_disc_basic_path = strdup(ROM_DISK_BASIC_DEFAULT_PATH);
    app_settings.artifact_colors = 1;

    cfg_opt_t opts[] = {
        CFG_SIMPLE_STR("rom_basic_path", &app_settings.rom_basic_path),
        CFG_SIMPLE_STR("rom_extended_basic_path", &app_settings.rom_extended_basic_path),
        CFG_SIMPLE_STR("rom_disc_basic_path", &app_settings.rom_disc_basic_path),
        CFG_SIMPLE_STR("cartridge_path", &app_settings.cartridge_path),
        CFG_SIMPLE_STR("cassette_path", &app_settings.cassette_path),
        CFG_SIMPLE_STR("disks_0_path", &app_settings.disks[0].path),
        CFG_SIMPLE_STR("disks_1_path", &app_settings.disks[1].path),
        CFG_SIMPLE_STR("disks_2_path", &app_settings.disks[2].path),
        CFG_SIMPLE_STR("disks_3_path", &app_settings.disks[3].path),
        CFG_SIMPLE_BOOL("video_artifact_colors", &app_settings.artifact_colors),
        CFG_SIMPLE_INT("joy_1_emulation_mode", &app_settings.joy_emulation_mode[0]),
        CFG_SIMPLE_INT("joy_2_emulation_mode", &app_settings.joy_emulation_mode[1]),
        CFG_END()
    };

    cfg = cfg_init(opts, 0);

    const char *base_pref_path = SDL_GetPrefPath(ORG_NAME, APP_NAME);
    if (!base_pref_path) {
        log_message(LOG_ERROR, "Error initializing configuration: %s", SDL_GetError());
        exit(1);
    }

    app_settings.config_path = malloc(strlen("config.ini") + strlen(base_pref_path) + 1);
    if (!app_settings.config_path) {
        log_message(LOG_ERROR, "Error initializing configuration: path allocation error");
        exit(1);
    }
    sprintf(app_settings.config_path, "%sconfig.ini", base_pref_path);

    log_message(LOG_INFO, "Reading the configuration file %s", app_settings.config_path);
    if(cfg_parse(cfg, app_settings.config_path) == CFG_FILE_ERROR) {
        log_message(LOG_INFO, "Error reading the configuration file %s. Ignoring.", app_settings.config_path);
    }
}

void settings_save(void) {
	for (int i = 0; cfg->opts[i].name; i++) {
        // This is a workaround to make sure the option is saved
        cfg_opt_t *opt = &cfg->opts[i];
        if (opt->type == CFGT_STR) {
            if (cfg_opt_getnstr(opt, 0) && *cfg_opt_getnstr(opt, 0))
                opt->nvalues = 1;  // enable the setting only if it was set
            else
                opt->nvalues = 0;
        } else {
            opt->nvalues = 1;
        }
	}

    FILE *fp = fopen(app_settings.config_path, "w");
    if (!fp) {
        log_message(LOG_ERROR, "error updating the configuration file %s: %s", app_settings.config_path, strerror(errno));
        return;
    }
    cfg_print(cfg, fp);
    fclose(fp);
}
