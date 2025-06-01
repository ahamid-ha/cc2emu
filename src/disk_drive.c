#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <fileapi.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include "disk_drive.h"
#include "controls.h"
#include "utils.h"

#define BYTE_RW_DELAY_NS 32000

// emulating WD 1793


struct disk_drive_status *disk_drive_create(void) {
    struct disk_drive_status *drive = malloc(sizeof(struct disk_drive_status));
    memset(drive, 0, sizeof(struct disk_drive_status));

    return drive;
}

void disk_drive_process_next(struct disk_drive_status *drive) {
    drive->next_command_after_nano = 0;
    if (!drive->_next_command) {
        return;
    }
    void (*next_command)(struct disk_drive_status *drive) = drive->_next_command;
    drive->_next_command = NULL;

    next_command(drive);

    if (drive->irq) drive->HALT = 0;
}

void _end_command(struct disk_drive_status *drive) {
    drive->status_1.BUSY = 0;
    drive->next_command_after_nano = 0;
    drive->_next_command = NULL;
    drive->irq = 1;
    // log_message(LOG_INFO, "-- Command end");
}

void _schedule_next(struct disk_drive_status *drive, uint64_t next_command_after_nano, void (*next_command)(struct disk_drive_status *drive)) {
    drive->status_1.BUSY = 1;
    drive->next_command_after_nano = next_command_after_nano;
    drive->_next_command = next_command;
}

void _clear_status_1(struct disk_drive_status *drive) {
    drive->status_1.BUSY = 1;
    drive->status_1.INDEX = 0;
    drive->status_1.CRC = 0;
    drive->status_1.SEEK_ERROR = 0;
    drive->status_1.HEAD_LOADED = drive->command & 0x8 ? 1 : 0;
    drive->status_1.NOT_READY = 0;
    drive->status_1.TRACK00 = drive->track == 0 ? 1 : 0;
}

void _clear_status_2(struct disk_drive_status *drive) {
    drive->status_2_3.BUSY = 1;
    drive->status_2_3.DATA_REQUEST = 0;
    drive->status_2_3.LOST_DATA = 0;
    drive->status_2_3.CRC = 0;
    drive->status_2_3.RNF = 0;
    drive->status_2_3.RECORD_TYPE__FAULT = 0;
    drive->status_2_3.NOT_READY = 0;
    drive->status_2_3.PROTECTED = 0;
}

uint64_t _get_stepping(struct disk_drive_status *drive) {
    switch (drive->command & 3) {
        case 0: return 6000000;
        case 1: return 12000000;
        case 2: return 20000000;
        default: return 30000000;
    }
}

void _command_seek(struct disk_drive_status *drive) {
    // log_message(LOG_INFO, "Drive command seek, target=%d, current=%d, stepping=%ld", drive->_seek_track_target, drive->track, _get_stepping(drive));
    if (drive->track >= drive->tracks) {
        drive->track = drive->tracks;
    }

    if (drive->track == drive->_seek_track_target) {
        _end_command(drive);
        return;
    }

    if (drive->track > drive->_seek_track_target) {
        drive->step_direction=-1;
    } else {
        drive->step_direction=1;
    }

    if (drive->track == 0 && drive->step_direction == -1) {
        _end_command(drive);
        return;
    }

    drive->track += drive->step_direction;

    _schedule_next(drive, _get_stepping(drive), _command_seek);
}

void _command_step(struct disk_drive_status *drive) {
    _end_command(drive);

    if (drive->command & 0x10) {
        // u flag is set
        drive->track += drive->step_direction;
    }

    return;
}

uint8_t *_get_drive_data(struct disk_drive_status *drive) {
    if (drive->DRIVE_SELECT_0) return drive->_drive_data[0];
    if (drive->DRIVE_SELECT_1) return drive->_drive_data[1];
    if (drive->DRIVE_SELECT_2) return drive->_drive_data[2];
    if (drive->DRIVE_SELECT_3) return drive->_drive_data[3];
    return NULL;
}

int _get_drive_id(struct disk_drive_status *drive) {
    if (drive->DRIVE_SELECT_0) return 0;
    if (drive->DRIVE_SELECT_1) return 1;
    if (drive->DRIVE_SELECT_2) return 2;
    if (drive->DRIVE_SELECT_3) return 3;
    return 0;
}

void _command_read_sector(struct disk_drive_status *drive) {
    uint8_t *_drive_data = _get_drive_data(drive);

    if (drive->sector_data_pos >= drive->sector_length) {
        if ((drive->command & 0x10) == 0) {
            // single sector
            _schedule_next(drive, BYTE_RW_DELAY_NS * 2, _end_command);
            return;
        }
        drive->sector_data_pos = 0;
        drive->sector++;

        // log_message(LOG_INFO, "Drive command read next sector track=%d, sector=%d", drive->track, drive->sector);

        _schedule_next(drive, BYTE_RW_DELAY_NS * 82, _command_read_sector);
        return;
    }

    if (!_drive_data ||
            drive->sector > drive->sectors ||
            !drive->sector ||
            drive->track >= drive->tracks) {
        _end_command(drive);
        drive->status_2_3.RNF = 1;
        return;
    }

    drive->status_2_3.LOST_DATA = 0;
    if (drive->status_2_3.DATA_REQUEST) {
        drive->status_2_3.LOST_DATA = 1;
        log_message(LOG_ERROR, "Data lost");
    }

    int data_pos = (((int)drive->track) * drive->sectors + (int)drive->sector - 1) * drive->sector_length + drive->sector_data_pos;
    if (drive->sector_data_pos < drive->sector_length && data_pos < drive->_drive_data_length[_get_drive_id(drive)])
        drive->data = _drive_data[data_pos];
    // log_message(LOG_INFO, "     read sector track=%d, sector=%d pos=%d data_pos=%d data=%02X", drive->track, drive->sector, drive->sector_data_pos, data_pos, drive->data);
    drive->status_2_3.DATA_REQUEST = 1;
    drive->sector_data_pos++;

    _schedule_next(drive, BYTE_RW_DELAY_NS, _command_read_sector);
}

void _command_write_sector(struct disk_drive_status *drive) {
    uint8_t *_drive_data = _get_drive_data(drive);

    if (!_drive_data ||
            drive->sector > drive->sectors ||
            !drive->sector ||
            drive->track >= drive->tracks) {
        _end_command(drive);
        drive->status_2_3.RNF = 1;
        return;
    }

    if (drive->sector_data_pos == -2) {
        // we are just after the CRC mark
        // ask for data
        drive->status_2_3.DATA_REQUEST = 1;
        drive->sector_data_pos++;
        _schedule_next(drive, BYTE_RW_DELAY_NS * 8, _command_write_sector);
        return;
    }

    if (drive->sector_data_pos == -1) {
        // after the gap following the CRC
        if (drive->status_2_3.DATA_REQUEST) {
            log_message(LOG_ERROR, "Data lost 1st byte");
            _end_command(drive);
            drive->status_2_3.LOST_DATA = 1;
            return;
        }
        drive->sector_data_pos++;
        _schedule_next(drive, BYTE_RW_DELAY_NS * (11 + 12 + 1), _command_write_sector);
        return;
    }

    if (drive->status_2_3.DATA_REQUEST) {
        log_message(LOG_ERROR, "Data lost");
        drive->status_2_3.LOST_DATA = 1;
        _drive_data[(((int)drive->track) * drive->sectors + (int)drive->sector - 1) * drive->sector_length + drive->sector_data_pos] = 0;
    } else {
        drive->status_2_3.LOST_DATA = 0;
        _drive_data[(((int)drive->track) * drive->sectors + (int)drive->sector - 1) * drive->sector_length + drive->sector_data_pos] = drive->data;
    }
    drive->sector_data_pos++;

    if (drive->sector_data_pos >= drive->sector_length) {
        if ((drive->command & 0x10) == 0) {
            // single sector
            _end_command(drive);
            return;
        }
        drive->sector_data_pos = -2;
        drive->sector++;
        _schedule_next(drive, BYTE_RW_DELAY_NS * (3 + 18 + 2), _command_write_sector);
        return;
    }

    drive->status_2_3.DATA_REQUEST = 1;
    _schedule_next(drive, BYTE_RW_DELAY_NS, _command_write_sector);
}


/*
It simulates track write command
all non-data fields are ignored, it just waits for FB writes to mark the start of the data, writes 256 bytes, then waits
for 4E to mark switching to next sector
It will not work correctly if expected bytes aren't sent
*/
void _command_write_track(struct disk_drive_status *drive) {
    uint8_t *_drive_data = _get_drive_data(drive);

    if (!_drive_data ||
            drive->sector > drive->sectors ||
            !drive->sector ||
            drive->track >= drive->tracks) {
        _end_command(drive);
        return;
    }

    uint8_t data = drive->data;
    if (drive->status_2_3.DATA_REQUEST) {
        log_message(LOG_ERROR, "Data lost");
        drive->status_2_3.LOST_DATA = 1;
        data = 0;
    }

    if (drive->sector_data_pos >= 0 && drive->sector_data_pos < drive->sector_length) {
        _drive_data[(((int)drive->track) * drive->sectors + (int)drive->sector - 1) * drive->sector_length + drive->sector_data_pos] = data;
    }

    if (drive->sector_data_pos > drive->sector_length) {
        // log_message(LOG_INFO, "Data after = %02x", data);
    }
    if (drive->sector_data_pos > drive->sector_length && data == 0x4e) {
        // a sector completed, move to next
        drive->sector_data_pos = -1;
        drive->sector++;
        // log_message(LOG_INFO, "Switching next sector");
    } else if (drive->sector_data_pos >= 0) {
        drive->sector_data_pos++;
    } else if (data == 0xfb) {  // data address mark
        drive->sector_data_pos = 0;
        // log_message(LOG_INFO, "_command_write_track, start writing sector %d", drive->sector);
    }

    drive->status_2_3.DATA_REQUEST = 1;
    _schedule_next(drive, BYTE_RW_DELAY_NS, _command_write_track);
}

void _command_read_address(struct disk_drive_status *drive) {
    unsigned old_data_request = drive->status_2_3.DATA_REQUEST;
    uint8_t *_drive_data = _get_drive_data(drive);

    if (drive->sector_data_pos >= 6) {
        _end_command(drive);
        drive->sector = drive->track;
        return;
    }

    if (!_drive_data ||
            drive->sector > drive->sectors ||
            !drive->sector ||
            drive->track >= drive->tracks) {
        _end_command(drive);
        drive->status_2_3.RNF = 1;
        return;
    }

    switch(drive->sector_data_pos) {
        case 0: drive->data = drive->track; break;
        case 2: drive->data = drive->sector; break;
        case 3: drive->data = 0x01; // sector length
        case 1:  // side
        case 4:  // crc1
        case 5:  // crc2
            drive->data = 0; break;
    }
    drive->data = _drive_data[(((int)drive->track) * drive->sectors + (int)drive->sector - 1) * drive->sector_length + drive->sector_data_pos];
    drive->sector_data_pos++;
    drive->status_2_3.DATA_REQUEST = 1;
    if (old_data_request) {
        drive->status_2_3.LOST_DATA = 1;
        log_message(LOG_ERROR, "Data lost");
    }
    _schedule_next(drive, BYTE_RW_DELAY_NS, _command_read_address);
}

void disk_drive_reset(struct disk_drive_status *drive) {
    drive->_next_command = NULL;
    drive->next_command_after_nano = 0;
    drive->irq = 0;
    drive->HALT = 0;
    drive->status_1.BUSY = 0;
    drive->drive_select_ff = 0;
    drive->command = 3;
    drive->sector = 1;

    drive->_seek_track_target = 0;

    _clear_status_1(drive);
    drive->_seek_track_target = 0;
    _command_seek(drive);
}

void _start_command(struct disk_drive_status *drive) {
    if ((drive->command & 0xf0) == 0) {
        // restore
        log_message(LOG_INFO, "Drive command restore %02X", drive->command);
        _clear_status_1(drive);
        drive->_seek_track_target = 0;
        _command_seek(drive);
    } else if ((drive->command & 0xf0) == 0x10) {
        // seek
        log_message(LOG_INFO, "Drive command seek");
        _clear_status_1(drive);
        drive->_seek_track_target = drive->data;
        _command_seek(drive);
    } else if ((drive->command & 0xe0) == 0x20) {
        // step
        log_message(LOG_INFO, "Drive command step");
        _clear_status_1(drive);
        _schedule_next(drive, _get_stepping(drive), _command_step);
    } else if ((drive->command & 0xe0) == 0x40) {
        // step-in
        log_message(LOG_INFO, "Drive command step-in");
        _clear_status_1(drive);
        drive->step_direction = 1;
        _schedule_next(drive, _get_stepping(drive), _command_step);
    } else if ((drive->command & 0xe0) == 0x60) {
        // step-out
        log_message(LOG_INFO, "Drive command step-out");
        _clear_status_1(drive);
        drive->step_direction = -1;
        _schedule_next(drive, _get_stepping(drive), _command_step);
    } else if ((drive->command & 0xe0) == 0x80) {
        // read sector
        log_message(LOG_INFO, "Drive command read sector track=%d, sector=%d", drive->track, drive->sector);
        drive->sector_data_pos = 0;
        _clear_status_2(drive);
        _schedule_next(drive, drive->command & 4 ? 15000000 : BYTE_RW_DELAY_NS * 55, _command_read_sector);
    } else if ((drive->command & 0xe0) == 0xA0) {
        // write sector
        log_message(LOG_INFO, "Drive command write sector track=%d, sector=%d", drive->track, drive->sector);
        drive->sector_data_pos = -2;
        _clear_status_2(drive);
        if (drive->is_write_protect[_get_drive_id(drive)]) {
            log_message(LOG_INFO, "Disk is write protected");
            _end_command(drive);
            drive->status_2_3.PROTECTED = 1;
        } else {
            _schedule_next(drive, drive->command & 4 ? 15000000 : BYTE_RW_DELAY_NS * (18 + 2), _command_write_sector);
        }
    } else if ((drive->command & 0xf0) == 0xC0) {
        // read address
        log_message(LOG_INFO, "Drive command read address");
        drive->sector_data_pos = 0;
        _clear_status_2(drive);
        drive->sector = 1;
        _schedule_next(drive, drive->command & 4 ? 15000000 : BYTE_RW_DELAY_NS * 55, _command_read_address);
    } else if ((drive->command & 0xf0) == 0xE0) {
        // read track
        log_message(LOG_INFO, "Drive command read track");
    } else if ((drive->command & 0xf0) == 0xF0) {
        // write track
        log_message(LOG_INFO, "Drive command write track");
        _clear_status_2(drive);
        drive->sector = 1;
        drive->sector_data_pos = 0 - (101 + 59);
        drive->status_2_3.DATA_REQUEST = 1;
        _schedule_next(drive, drive->command & 4 ? 15000000 : BYTE_RW_DELAY_NS * (18 + 2), _command_write_track);
    } else if ((drive->command & 0xf0) == 0xD0) {
        // force interrupt
        log_message(LOG_INFO, "Drive command force interrupt");

        drive->_next_command = NULL;
        drive->next_command_after_nano = 0;
        if (!drive->status_1.BUSY) {
            _clear_status_1(drive);
        }
        drive->status_1.BUSY = 0;

        if ((drive->command & 0xf) != 0) {
            drive->irq = 1;
        }
    } else {
        log_message(LOG_ERROR, "Unknow drive command %02x", drive->command);
    }
}

uint8_t disk_drive_read_register(void *data, uint16_t address) {
    struct disk_drive_status *drive = data;
    // log_message(LOG_INFO, "disk_drive_read_register: %02x", address);

    if ((address & 0x8) == 0) {
        return 0xff;
    }

    address = address & 3;
    switch (address)
    {
        case 0:
        {
            uint8_t ret = drive->status;
            drive->irq = 0;
            return ret;
        }
        case 1:
            return drive->track;
        case 2:
            return drive->sector;
        default:
            drive->status_2_3.DATA_REQUEST = 0;
            drive->status_2_3.LOST_DATA = 0;
            return drive->data;
    }
}


void disk_drive_write_register(void *data, uint16_t address, uint8_t value) {
    struct disk_drive_status *drive = data;
    // log_message(LOG_INFO, "disk_drive_write_register: %02x %02x", address, value);

    if ((address & 0x8) == 0) {
        drive->drive_select_ff = value;
        if (drive->irq) drive->HALT = 0;
        return;
    }

    address = address & 3;
    switch (address)
    {
        case 0:
            drive->status_2_3.DATA_REQUEST = 0;
            drive->status_2_3.LOST_DATA = 0;
            drive->irq = 0;
            drive->command = value;
            _start_command(drive);
            break;
        case 1:
            drive->track = value;
            break;
        case 2:
            drive->sector = value;
            break;
        case 3:
            drive->data = value;
            drive->status_2_3.DATA_REQUEST = 0;
            drive->status_2_3.LOST_DATA = 0;
            break;
    }
    if (drive->irq) drive->HALT = 0;
}


int disk_drive_create_empty_image(const char * filename) {
    const size_t FILE_SIZE = 161280;
#ifdef _WIN32
    HANDLE hFile = CreateFile(filename, GENERIC_READ | GENERIC_WRITE,
        0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        log_message(LOG_ERROR, "Failed to create/open the file. Error: %lu", GetLastError());
        return -1;
    }

    LARGE_INTEGER fileSize;
    fileSize.QuadPart = FILE_SIZE;

    if (!SetFilePointerEx(hFile, fileSize, NULL, FILE_BEGIN)) {
        log_message(LOG_ERROR, "Failed to set file pointer. Error: %lu", GetLastError());
        CloseHandle(hFile);
        return -1;
    }

    if (!SetEndOfFile(hFile)) {
        log_message(LOG_ERROR, "Failed to set file size. Error: %lu", GetLastError());
        CloseHandle(hFile);
        return -1;
    }

    log_message(LOG_INFO, "File '%s' created with size %d bytes.", filename, FILE_SIZE);

    CloseHandle(hFile);
#else
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    if (fd == -1) {
        perror("Failed to create/open the file");
        return -1;
    }

    if (ftruncate(fd, FILE_SIZE) != 0) {
        perror("Failed to set file size");
        close(fd);
        return -1;
    }

    log_message(LOG_INFO, "File '%s' created with size %d bytes.", filename, FILE_SIZE);

    close(fd);
#endif
    return 0;
}


int disk_drive_load_disk(struct disk_drive_status *drive, int drive_no, const char *path) {
    if (drive->_drive_data[drive_no]) {
#ifdef _WIN32
        UnmapViewOfFile(drive->_drive_data[drive_no]);
        CloseHandle(drive->_drive_map_handle[drive_no]);
        drive->_drive_map_handle[drive_no] = NULL;
        CloseHandle(drive->_drive_file_handle[drive_no]);
        drive->_drive_file_handle[drive_no] = NULL;
#else
        munmap(drive->_drive_data[drive_no], drive->_drive_data_length[drive_no]);
#endif
        drive->_drive_data[drive_no] = NULL;
    }
    if (!path) {
        log_message(LOG_INFO, "Unloading disk:%d", drive_no);
        return 0;
    }

    log_message(LOG_INFO, "Loading disk:%d %s", drive_no, path);

    drive->is_write_protect[drive_no] = is_file_writable(path) ? 0 : 1;
    if(drive->is_write_protect[drive_no]) {
        log_message(LOG_INFO, "Disk is readonly %s", path);
    }

#ifdef _WIN32
    drive->_drive_file_handle[drive_no] = CreateFile(
        path, 
        drive->is_write_protect[drive_no] ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
        0,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (drive->_drive_file_handle[drive_no] == INVALID_HANDLE_VALUE) {
        log_message(LOG_ERROR, "Error opening disk:%s", path);
        return 1;
    }

    LARGE_INTEGER ldisk_file_length;
    if (!GetFileSizeEx(drive->_drive_file_handle[drive_no], &ldisk_file_length)) {
        log_message(LOG_ERROR, "Error read disk size:%s", path);
        CloseHandle(drive->_drive_file_handle[drive_no]);
        drive->_drive_file_handle[drive_no] = NULL;
        return 1;
    }
    size_t disk_file_length = ldisk_file_length.QuadPart;

    drive->_drive_map_handle[drive_no] = CreateFileMappingA(
        drive->_drive_file_handle[drive_no],
        NULL,
        drive->is_write_protect[drive_no] ? PAGE_READONLY : PAGE_READWRITE,
        0, 0, NULL);
    if (drive->_drive_map_handle[drive_no] == INVALID_HANDLE_VALUE) {
        log_message(LOG_ERROR, "Error mapping disk:%s", path);
        CloseHandle(drive->_drive_file_handle[drive_no]);
        drive->_drive_file_handle[drive_no] = NULL;
        return 1;
    }

    drive->_drive_data[drive_no] = MapViewOfFile(
        drive->_drive_map_handle[drive_no],
        drive->is_write_protect[drive_no] ? FILE_MAP_READ: FILE_MAP_ALL_ACCESS,
        0, 0, 0);
    if (drive->_drive_data[drive_no] == NULL) {
        log_message(LOG_ERROR, "Error reading disk:%s", path);
        CloseHandle(drive->_drive_map_handle[drive_no]);
        drive->_drive_map_handle[drive_no] = NULL;
        CloseHandle(drive->_drive_file_handle[drive_no]);
        drive->_drive_file_handle[drive_no] = NULL;
        return 1;
    }
#else
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        error_general_file(path);
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        error_general_file(path);
        close(fd);
        return 1;
    }

    size_t disk_file_length = st.st_size;

    if ((drive->_drive_data[drive_no] = mmap(NULL, disk_file_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
    {
        error_general_file(path);
        close(fd);
        drive->_drive_data[drive_no] = 0;
        return 1;
    }
    close(fd);
#endif


    drive->_drive_data_length[drive_no] = disk_file_length;
    drive->tracks = 35;
    drive->sectors = 18;
    drive->sector_length = 256;
    drive->sector_data_pos = 0;

    return 0;
}
