#include <confuse.h>


#define Joy_Emulation_None 0
#define Joy_Emulation_Keyboard 1
#define Joy_Emulation_Mouse 2
#define Joy_Emulation_Joy1 3
#define Joy_Emulation_Joy2 4

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

    long int joy_emulation_mode[2];
};

extern struct app_settings app_settings;

void settings_init(void);
void settings_save(void);
