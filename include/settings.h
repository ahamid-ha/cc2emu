#include <confuse.h>


struct app_settings {
    char *rom_basic_path;
    char *rom_extended_basic_path;
    char *rom_disc_basic_path;

    struct {
        char *path;
    } disks[4];

    char *cartridge_path;
    char *cassette_path;

    char *config_path;

    cfg_bool_t artifact_colors;
};

extern struct app_settings app_settings;

void settings_init(void);
void settings_save(void);
