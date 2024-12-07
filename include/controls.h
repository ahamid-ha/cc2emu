#include "machine.h"

void error_msg(const char *msg);
void error_general_file(const char *path);
void controls_init(struct machine_status *machine);
void controls_display();
void controls_input_begin(void);
void controls_input_end(void);
