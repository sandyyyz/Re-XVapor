#include "hba.h"

void ahci_parse_dev_info(struct hba_device* dev_info, uint16_t* data);
void ahci_parsestr(char* str, uint16_t* reg_start, int size_word);