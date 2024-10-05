#include <inttypes.h>
#include <stdbool.h>


struct disk_drive_status {
    union {
        struct {
            unsigned BUSY:1;
            unsigned INDEX:1;
            unsigned TRACK00:1;
            unsigned CRC:1;
            unsigned SEEK_ERROR:1;
            unsigned HEAD_LOADED:1;
            unsigned PROTECTED:1;
            unsigned NOT_READY:1;
        }status_1;
        struct {
            unsigned BUSY:1;
            unsigned DATA_REQUEST:1;
            unsigned LOST_DATA:1;
            unsigned CRC:1;
            unsigned RNF:1;
            unsigned RECORD_TYPE__FAULT:1;
            unsigned PROTECTED:1;
            unsigned NOT_READY:1;
        }status_2_3;
        uint8_t status;
    };
    uint8_t command;
    uint8_t track;
    uint8_t sector;
    uint8_t data;

    union {
        struct {
            unsigned DRIVE_SELECT_0:1;
            unsigned DRIVE_SELECT_1:1;
            unsigned DRIVE_SELECT_2:1;
            unsigned MOTOR_ON:1;
            unsigned PRECOMP_ENABLED:1;
            unsigned DDEN:1;  // double density enable
            unsigned DRIVE_SELECT_3:1;
            unsigned HALT:1;
        };
        uint8_t drive_select_ff;
    };

    uint64_t next_command_after_nano;
    void (*_next_command)(struct disk_drive_status *drive);

    uint8_t _seek_track_target;

    uint8_t *_drive_data[4];
    size_t _drive_data_length[4];
    bool is_write_protect[4];
    int step_direction;
    int tracks;
    int sectors;
    int sector_length;
    int sector_data_pos;

    bool irq;
};

struct disk_drive_status *disk_drive_create(void);
uint8_t disk_drive_read_register(void *drive, uint16_t address);
void disk_drive_write_register(void *drive, uint16_t address, uint8_t value);
void disk_drive_process_next(struct disk_drive_status *drive);
int disk_drive_load_disk(struct disk_drive_status *drive, int drive_no, const char *path);
