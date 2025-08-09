#ifndef __LOONGARCH_REGS_H__
#define __LOONGARCH_REGS_H__

#define LOONGARCH_CSR_CRMD		    0x0	    /* Current mode info */
#define LOONGARCH_CSR_PRMD          0x1     /* Prev-exception mode info */
#define LOONGARCH_CSR_CPUID		    0x20	/* CPU core id */

#define LOONGARCH_CSR_SAVE0		    0x30    /* Kscratch registers */
#define LOONGARCH_CSR_SAVE1         0x31    /* Kscratch registers */

#define LOONGARCH_CSR_TLBEHI		0x11	/* TLB EntryHi */
#define LOONGARCH_CSR_PGDL          0x19
#define LOONGARCH_CSR_PGD           0x1b
#define LOONGARCH_CSR_TLBRENTRY		0x88	/* TLB refill exception entry */
#define LOONGARCH_CSR_TLBRBADV		0x89	/* TLB refill badvaddr */
#define LOONGARCH_CSR_TLBRSAVE		0x8b	/* KScratch for TLB refill exception */

#endif