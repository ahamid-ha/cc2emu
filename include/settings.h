
struct app_settings {
    struct {
        char *path;
    } disks[4];

    char *cartridge_path;
    char *cassette_path;

    char *config_path;
};

extern struct app_settings app_settings;

void settings_init(void);
void settings_save(void);
