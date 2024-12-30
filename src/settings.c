#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <confuse.h>
#include <errno.h>
#include <SDL3/SDL_filesystem.h>
#include "settings.h"

#define ORG_NAME "emu"
#define APP_NAME "emu"

struct app_settings app_settings;
cfg_t *cfg;

void settings_init(void) {
    memset(&app_settings, 0, sizeof(struct app_settings));

    cfg_opt_t opts[] = {
        CFG_SIMPLE_STR("cartridge_path", &app_settings.cartridge_path),
        CFG_SIMPLE_STR("cassette_path", &app_settings.cassette_path),
        CFG_SIMPLE_STR("disks_0_path", &app_settings.disks[0].path),
        CFG_SIMPLE_STR("disks_1_path", &app_settings.disks[1].path),
        CFG_SIMPLE_STR("disks_2_path", &app_settings.disks[2].path),
        CFG_SIMPLE_STR("disks_3_path", &app_settings.disks[3].path),
        CFG_END()
    };

    cfg = cfg_init(opts, 0);

    const char *base_pref_path = SDL_GetPrefPath(ORG_NAME, APP_NAME);
    if (!base_pref_path) {
        fprintf(stderr, "Error initializing configuration: %s\n", SDL_GetError());
        exit(1);
    }

    if (asprintf(&app_settings.config_path, "%sconfig.ini", base_pref_path) == -1) {
        fprintf(stderr, "Error initializing configuration: path allocation error\n");
        exit(1);
    }

    printf("Reading the configuration file %s\n", app_settings.config_path);
    if(cfg_parse(cfg, app_settings.config_path) == CFG_FILE_ERROR) {
        fprintf(stderr, "Error reading the configuration file %s. Ignoring.\n", app_settings.config_path);
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
        fprintf(stderr, "error updating the configuration file %s: %s\n", app_settings.config_path, strerror(errno));
        return;
    }
    cfg_print(cfg, fp);
    fclose(fp);
}
