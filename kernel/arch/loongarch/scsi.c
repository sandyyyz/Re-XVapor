#include "types.h"
#include "scsi.h"
#include "defs.h"

void
scsi_create_packet12(struct scsi_cdb12* cdb,
                     uint8_t opcode,
                     uint32_t lba,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be = SCSI_FLIP(lba);
    cdb->length = alloc_size;
}

void
scsi_create_packet16(struct scsi_cdb16* cdb,
                     uint8_t opcode,
                     uint32_t lbal,
                     uint32_t lbah,
                     uint32_t alloc_size)
{
    memset(cdb, 0, sizeof(*cdb));
    cdb->opcode = opcode;
    cdb->lba_be_hi = SCSI_FLIP(lbah);
    cdb->lba_be_lo = SCSI_FLIP(lbal);
    cdb->length = alloc_size;
}
