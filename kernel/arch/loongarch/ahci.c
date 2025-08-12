#include "types.h"
#include "memlayout.h"
#include "pci.h"
#include "printf.h"
#include "ahci.h"
#include "defs.h"
#include "ahci_block_device.h"
#include "debug.h"
#include "hba.h"
#include "debug.h"
#include "ahci.h"
#include "scsi.h"

static struct hba_command_table *ahci_initialize_command_table(struct hba_command_header * cmdheader,unsigned int count,unsigned long buf,unsigned int interrupt_on_complete);
static struct hba_command_header *ahci_initialize_command_header(unsigned long port_base,int slot,unsigned int count,int write);
static struct fis_reg_host_to_device *
ahci_initialize_fis_host_to_device( struct hba_command_table *cmdtbl,
									unsigned int startl, 
									unsigned int starth,
									int ata_command,
									unsigned int count);
static int ahci_find_cmdslot(unsigned long port_base);
static int ahci_init_device(struct hba_port* port);
static int ahci_identify(unsigned long port_base);

unsigned long SATA_ABAR_BASE;//sata控制器的bar地址
char ahci_port_base_vaddr[1048576];
struct block_device_request_queue ahci_req_queue;
/*启动命令引擎*/
static void start_cmd(unsigned long port_base)
{
	printf("start_cmd start\n");
	unsigned int* port_regs = (unsigned int*)(SATA_ABAR_BASE | port_base);

	/* 初始化端口，并置于就绪状态 */
	port_regs[HBA_RPxCI] = 0;
	port_regs[HBA_RPxSERR] = -1;
	port_regs[HBA_RPxIE] |= HBA_PxINTR_DMA;
	port_regs[HBA_RPxIE] |= HBA_PxINTR_D2HR;
	// Wait until CR (bit15) is cleared
	while (*(unsigned int *)(SATA_ABAR_BASE| (port_base + PORT_CMD)) & HBA_PxCMD_CR);

	// Set FRE (bit4) and ST (bit0)
	*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CMD)) |= HBA_PxCMD_FRE;// 开启端口的接收
	*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CMD)) |= HBA_PxCMD_ST;//开启向端口输入命令

	// struct hba_port port = {
	// 	.regs = port_regs,
	// 	.ssts = port_regs[HBA_RPxSSTS],
	// 	.cmdlst = port_regs[HBA_RPxCLB],
	// 	.fis = port_regs[HBA_RPxFB],
	// };
	// if(!ahci_init_device(&port))
	// {
	// 	printf("ahci_init_device error\n");
	// }
	ahci_identify(port_base);
	// printf("sizeof unsigned long %d\n", sizeof(unsigned long));
	printf("start_cmd down\n");
}

static int ahci_init_device(struct hba_port* port) {
	unsigned long port_base = (unsigned long)port->regs;
	// wait until the port is free
	while(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY))
		;
	
	int slot = ahci_find_cmdslot(port_base);
	if(slot == -1) {
		Warn("NO empty slot!");
		return 0;
	}

	void* buf = kalloc();
	port->device = kalloc();

	struct hba_command_header* cmdheader = ahci_initialize_command_header(port_base, slot, 1, 0);
	struct hba_command_table* cmdtbl = ahci_initialize_command_table(cmdheader, 1, (unsigned long)buf, 1);
	struct fis_reg_host_to_device* cmdfis;
	int command;
	if(port->regs[HBA_RPxSIG] == HBA_DEV_SIG_ATA) {
		command = ATA_IDENTIFY_DEVICE;
	} else {
		command = ATA_IDENTIFY_PAKCET_DEVICE;
		port->device->flags |= HBA_DEV_FATAPI;
	}
	cmdfis = ahci_initialize_fis_host_to_device(cmdtbl, 0, 0, command, 0);
	int spin = 0;
	int tfd = *(unsigned int*)(SATA_ABAR_BASE | (port_base + PORT_TFD));
	while ((tfd & (BIT_STAT_BSY | BIT_STAT_DRQ)) && spin < 1000000) {
		spin++;
	}
	if (spin == 1000000)
	{
		printf("Port is hung");
		return E_PORT_HUNG;
	}
	*(unsigned int *)(SATA_ABAR_BASE|(port_base + PORT_CI)) = 1 << slot; // Issue command

	printf("before command sent, PxIS = 0x%x\n", *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)));
	//等待命令发送
	while (1)
	{
		printf("");
		if (!((*(unsigned int*)( SATA_ABAR_BASE | (port_base + PORT_CI)) | (*(unsigned int*)(SATA_ABAR_BASE | (port_base + PORT_SACT)))) & (1 << slot))) 
		{
			break;
		}
		unsigned int PxIS = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS));
		printf("PxIS = 0x%x\n", PxIS);
		if (PxIS & HBA_PxIS_TFES) // 如果HBA_PxIS_TFES被置位则说明访问异常
		{
			printf("Read disk error\n");
			return 0;
		}
	}

	printf("after command apply, PxIS = 0x%x\n", *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)));
	// Check again
	if (*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)) & HBA_PxIS_TFES)
	{	
		printf("Read disk error\n");
		printf("PxIS = 0x%x\n", *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)));
		unsigned int PxTFD = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_TFD));
		unsigned int PxSERR = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_SERR));
		printf("PxTFD = 0x%x\n", PxTFD);
		printf("PxSERR = 0x%x\n", PxSERR);
		return 0;
	}
	
	if (!(port->device->flags & HBA_DEV_FATAPI)) {
        goto done;
    }

	Warn("HBA_DEV_FATAPI");
done:
	return 1;
}
/*停止命令引擎*/ 
static void stop_cmd(unsigned int port_base)
{
	printf("stop_cmd start\n");
	// Clear ST (bit0)
	*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CMD)) &= ~HBA_PxCMD_ST;
	// Clear FRE (bit4)
	*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CMD)) &= ~HBA_PxCMD_FRE;

	// Wait until FR (bit14), CR (bit15) are cleared
	while (1)
	{
		if (*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CMD)) & HBA_PxCMD_FR)
			continue;
		if (*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CMD)) & HBA_PxCMD_CR)
			continue;
		break;
	}
	printf("stop_cmd down\n");
}

/*寻找可用的命令槽位*/
static int ahci_find_cmdslot(unsigned long port_base)
{
	// If not set in SACT and CI, the slot is free
	unsigned int pxsact = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_SACT));
	unsigned int pxci = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_CI));
	uint32_t slots = pxsact | pxci;
	int num_of_cmd_clots =((*(unsigned int*)(SATA_ABAR_BASE | HBA_CAP)) & 0x0f00) >> 8;  // bit 12-8
	for (int i = 0; i < num_of_cmd_clots; i++)
	{
		if ((slots & 1) == 0)
			return i;
		slots >>= 1;
	}
	printf("Cannot find free command list entry\n");
	return -1;
}

/*构建command_table
*cmdheader：结构体hba_command_header
*count：扇区数
*buf：读写缓冲区
*interrupt_on_complete：中断完成时占位标志，1为读0为写
*/
static struct hba_command_table *ahci_initialize_command_table(struct hba_command_header * cmdheader,unsigned int count,unsigned long buf,unsigned int interrupt_on_complete)
{
	struct hba_command_table *cmdtbl = (struct hba_command_table *)(cmdheader->command_table_base);
	memset(cmdtbl, 0, sizeof(struct hba_command_table) + (cmdheader->prdt_len - 1) * sizeof(struct hba_prdt_entry));

	// 一个表项对应16个磁盘扇区
	int i;
	for (i = 0; i < cmdheader->prdt_len - 1; ++i)
	{
		cmdtbl->prdt_entries[i].data_base = buf;
		cmdtbl->prdt_entries[i].byte_count = 8 * 1024 - 1; // 8K bytes (this value should always be set to 1 less than the actual value)
		cmdtbl->prdt_entries[i].interrupt_on_complete = 1;
		buf += 4 * 1024; // 4K uint16_ts
		count -= 16;     // 16 sectors
	}

	// Last entry
	cmdtbl->prdt_entries[i].data_base = buf;
	cmdtbl->prdt_entries[i].byte_count = (count << 9) ; // 512 bytes per sector
	cmdtbl->prdt_entries[i].interrupt_on_complete = interrupt_on_complete;
	return cmdtbl;
}

/*构建command_header
*port_base：端口基址
*slot：命令槽位
*count：扇区数
*write：读/写命令
*/
static struct hba_command_header *ahci_initialize_command_header(unsigned long port_base,int slot,unsigned int count,int write)
{
	struct hba_command_header *cmdheader = (struct hba_command_header *)(*(unsigned long*)(SATA_ABAR_BASE |(port_base + PORT_CLB)));
	cmdheader += slot;
	cmdheader->fis_length = sizeof(struct fis_reg_host_to_device) / sizeof(uint32_t); // 帧结构大小
	cmdheader->write = write ? 1 : 0;                                        //0为读，1为写
	cmdheader->prdt_len = (uint16_t)((count - 1) >> 4) + 1;     // PRDT entries count
	return cmdheader;
}

/*打包h2d类型的fis
*cmdtbl：hba_command_table结构
*startl starth：磁盘的高32位和低32位
*ata_command：磁盘命令，读写
*count: sector count
*/
static struct fis_reg_host_to_device *
ahci_initialize_fis_host_to_device( struct hba_command_table *cmdtbl,
									unsigned int startl, 
									unsigned int starth,
									int ata_command,
									unsigned int count)
{
	struct fis_reg_host_to_device *cmdfis = (struct fis_reg_host_to_device *)(&cmdtbl->command_fis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1; // Command
	cmdfis->command = ata_command;

	cmdfis->lba0 = (uint8_t)startl;
	cmdfis->lba1 = (uint8_t)(startl >> 8);
	cmdfis->lba2 = (uint8_t)(startl >> 16);
	cmdfis->device = 1 << 6; // LBA mode

	cmdfis->lba3 = (uint8_t)(startl >> 24);
	cmdfis->lba4 = (uint8_t)starth;
	cmdfis->lba5 = (uint8_t)(starth >> 8);

	cmdfis->count_l = count & 0xFF;
	cmdfis->count_h = (count >> 8) & 0xFF;
	return cmdfis;
}

int ahci_read(unsigned long port_base, unsigned int startl, unsigned int starth, unsigned int count, unsigned long buf)
{
	// *(unsigned int *)(SATA_ABAR_BASE|(port_base + PORT_IS)) = (uint32_t)-1; // Clear pending interrupt bits
	printf("ahci_read: port_base %p, startl %p, starth %p, count %d, buf %p\n", port_base, startl, starth, count, buf);
	int spin = 0;            // Spin lock timeout counter
	int slot = ahci_find_cmdslot(port_base);//寻找是否有空出的命令槽位
	if (slot == -1)
		return E_NOEMPTYSLOT;
	struct hba_command_header* cmdheader = ahci_initialize_command_header(port_base, slot, count,0);
	struct hba_command_table* cmdtbl = ahci_initialize_command_table(cmdheader, count, buf, 1);
	//ATA device
	// struct fis_reg_host_to_device* cmdfis = ahci_initialize_fis_host_to_device(cmdtbl, startl, starth, ATA_CMD_READ_DMA_EXT, count);
	//ATAPI device
	void* cbd = cmdtbl->acmd;
	struct fis_reg_host_to_device* cmdfis = ahci_initialize_fis_host_to_device(cmdtbl, startl, starth, ATA_PACKET, 0);
	scsi_create_packet16(cbd, SCSI_READ_BLOCKS_16, startl, starth, count);

	int tfd = *(unsigned int*)(SATA_ABAR_BASE | (port_base + PORT_TFD));
	while ((tfd & (BIT_STAT_BSY | BIT_STAT_DRQ)) && spin < 1000000) {
		spin++;
	}
	if (spin == 1000000)
	{
		printf("Port is hung");
		return E_PORT_HUNG;
	}
	*(unsigned int *)(SATA_ABAR_BASE|(port_base + PORT_CI)) = 1 << slot; // Issue command

	printf("before command sent, PxIS = 0x%x\n", *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)));
	//等待命令发送
	while (1)
	{
		printf("");
		if (!((*(unsigned int*)( SATA_ABAR_BASE | (port_base + PORT_CI)) | (*(unsigned int*)(SATA_ABAR_BASE | (port_base + PORT_SACT)))) & (1 << slot))) 
		{
			break;
		}
		unsigned int PxIS = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS));
		printf("PxIS = 0x%x\n", PxIS);
		if (PxIS & HBA_PxIS_TFES) // 如果HBA_PxIS_TFES被置位则说明访问异常
		{
			printf("Read disk error\n");
			return E_TASK_FILE_ERROR;
		}
	}

	printf("after command apply, PxIS = 0x%x\n", *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)));
	// Check again
	if (*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)) & HBA_PxIS_TFES)
	{	
		printf("Read disk error\n");
		printf("PxIS = 0x%x\n", *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)));
		unsigned int PxTFD = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_TFD));
		unsigned int PxSERR = *(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_SERR));
		printf("PxTFD = 0x%x\n", PxTFD);
		printf("PxSERR = 0x%x\n", PxSERR);
		return E_TASK_FILE_ERROR;
	}

	return AHCI_SUCCESS;
}

int ahci_write(unsigned long port_base, unsigned int startl, unsigned int starth, unsigned int count, unsigned long buf)
{
	*(unsigned int *)(SATA_ABAR_BASE | (port_base + PORT_IS)) = 0xffff; // Clear pending interrupt bits
	int slot = ahci_find_cmdslot(port_base);
	if (slot == -1)
		return E_NOEMPTYSLOT;
	struct hba_command_header* cmdheader = ahci_initialize_command_header(port_base, slot, count,1);
	cmdheader->clear_busy_upon_r_ok = 1;
	cmdheader->pmport = 1;
	struct hba_command_table* cmdtbl = (struct hba_command_table*)(cmdheader->command_table_base);
	memset(cmdtbl, 0, sizeof(struct hba_command_table) + (cmdheader->prdt_len - 1) * sizeof(struct hba_prdt_entry));

	// 8K bytes (16 sectors) per PRDT
	int i;
	for (i = 0; i < cmdheader->prdt_len - 1; ++i)
	{
		cmdtbl->prdt_entries[i].data_base = buf;
		cmdtbl->prdt_entries[i].byte_count = 8 * 1024 - 1; // 8K bytes (this value should always be set to 1 less than the actual value)
		cmdtbl->prdt_entries[i].interrupt_on_complete = 1;
		buf += 4 * 1024; // 4K uint16_ts
		count -= 16;     // 16 sectors
	}
	cmdtbl->prdt_entries[i].data_base = buf;
	cmdtbl->prdt_entries[i].byte_count = (count << 9); // 512 bytes per sector
	cmdtbl->prdt_entries[i].interrupt_on_complete = 0;
	struct fis_reg_host_to_device *cmdfis = ahci_initialize_fis_host_to_device(cmdtbl,startl,starth,ATA_CMD_WRITE_DMA_EXT,count);

	*(unsigned int *)(SATA_ABAR_BASE|(port_base + PORT_CI)) = 1 ; // Issue command

	while (1)
	{
	printf("");
	if ((*(unsigned int *)(SATA_ABAR_BASE|(port_base + PORT_CI)) & (1 << slot)) == 0)
			break;
		if (*(unsigned int *)(SATA_ABAR_BASE|(port_base + PORT_IS)) & HBA_PxIS_TFES)
		{ // Task file error
			printf("Write disk error");
			return E_TASK_FILE_ERROR;
		}
	}
	if (*(unsigned int *)(SATA_ABAR_BASE|(port_base+PORT_IS)) & HBA_PxIS_TFES)
	{
		printf("Write disk error");
		return E_TASK_FILE_ERROR;
	}
	return AHCI_SUCCESS;
}

static int ahci_identify(unsigned long port_base)
{
    *(unsigned int *)(SATA_ABAR_BASE|(port_base+PORT_IS)) = 0xffff; // Clear pending interrupt bits
    int slot = ahci_find_cmdslot(port_base);
    if (slot == -1)
        return E_NOEMPTYSLOT;
    int spin = 0;
    struct hba_command_header* cmdheader = ahci_initialize_command_header(port_base, slot, 1,1);//identify信息占据了一个扇区
    struct ata_identify buf;
    struct hba_command_table* cmdtbl = ahci_initialize_command_table(cmdheader, 1, (unsigned long)&buf, 0);
    struct fis_reg_host_to_device *cmdfis = ahci_initialize_fis_host_to_device(cmdtbl, 0, 0, ATA_IDENTIFY_PAKCET_DEVICE, 1);//从磁盘的第0扇区中读取出mbr
    int tfd = *(unsigned int*)(SATA_ABAR_BASE | (port_base + PORT_TFD));
    while ((tfd & (BIT_STAT_BSY | BIT_STAT_DRQ)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000)
    {
        printf("Port is hung");
        return E_PORT_HUNG;
    }
    printf("slot=%d\n", slot);
    *(unsigned int *)(SATA_ABAR_BASE|(port_base+PORT_CI)) = 1 << slot; // Issue command

    //等待命令发送
    while (1)
    {
		if ((*(unsigned int *)(SATA_ABAR_BASE|(port_base+PORT_CI)) & (1 << slot)) == 0)//等待命令槽位清零
			break;
		if (*(unsigned int *)(SATA_ABAR_BASE|(port_base+PORT_IS)) & HBA_PxIS_TFES) // 如果HBA_PxIS_TFES被置位则说明访问异常
		{
			printf("Read disk error\n");
			return E_TASK_FILE_ERROR;
		}
	}
	// Check again
	if (*(unsigned int *)(SATA_ABAR_BASE|(port_base+PORT_IS)) & HBA_PxIS_TFES)
	{
		printf("Read disk error\n");
		return E_TASK_FILE_ERROR;
	}
	printf("buf.ata_device 0x%x\n", buf.ata_device);
	if(buf.ata_device & 0x1) {
		printf("16 bytes command packet\n");
	} else if(!(buf.ata_device & 0x3)){
		printf("12 bytes command packet\n");
	} else {
		printf("reserve\n");
	}

	return AHCI_SUCCESS;
}

//io调度函数
// static long ahci_query_disk()
// {
//     while (1)
//     {
//         if (ahci_req_queue.request_count!=0)
//         {
//             /* code */
//         }else{
//             //若无要刷入磁盘的请求则调用线程休眠的函数
//         }
//     }
// }

/*将请求提交到io调度队列*/
// void ahci_submit(struct block_device_request_packet *pack)
// {
//     list_append(&(ahci_req_queue.queue_list), &(pack->list));
//     ++ahci_req_queue.request_count;

//     // if (ahci_req_queue.in_service == NULL) // 当前没有正在请求的io包，立即执行磁盘请求
//         // ahci_query_disk();
// }

static int check_type(unsigned int port)
{
	uint32_t ssts = *(unsigned int*)(SATA_ABAR_BASE | (port + PORT_SSTS));
	uint32_t sig = *(unsigned int*)(SATA_ABAR_BASE | (port + PORT_SIG));
	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;

	if (det != HBA_PORT_DET_PRESENT) {
		// Check drive status
		// printf(" det == 0x%x\n", det);
		return AHCI_DEV_NULL;
	}
	if (ipm != HBA_PORT_IPM_ACTIVE) {
		// printf(" ipm == 0x%x\n", ipm);
		return AHCI_DEV_NULL;
	}

	switch (sig)
	{
	case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
	case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
	case SATA_SIG_PM:
		return AHCI_DEV_PM;
	default:
		return AHCI_DEV_SATA;
	}
}

static void ahci_probe_port(void)
{
	uint32_t pi = *(unsigned int*)(SATA_ABAR_BASE | HBA_PI);
	uint32_t cap = *(unsigned int*)(SATA_ABAR_BASE | HBA_CAP);
	uint32_t pnums = (cap & 0x1f) + 1;
	printf("pi:0x%x, cap 0x%x, pnums 0x%x\n", pi, cap, pnums);
	for (int i = 0; i < pnums; ++i, (pi >>= 1))
	{
		if (pi & 1)
		{
			unsigned int dt = check_type(PORT_BASE + PORT_OFFEST * i);
			printf("ahci_probe_port dt:%d\n",dt);
			if (dt == AHCI_DEV_SATA)
			{
				printf("SATA drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_SATAPI)
			{
				printf("SATAPI drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_SEMB)
			{
				printf("SEMB drive found at port %d\n", i);
			}
			else if (dt == AHCI_DEV_PM)
			{
				printf("PM drive found at port %d\n", i);
			}
			else
			{
				// printf("No drive found at port %d\n", i);
			}
		}
	}
	printf("ahci_probe_port down\n");
}

static void port_rebase(int portno)
{
	unsigned long port = PORT_BASE + portno * PORT_OFFEST;//计算端口的偏移地址
	stop_cmd(port);  // 停止命令引擎

	// 命令列表偏移量：1K * portno
	// 命令列表条目大小 = 32
	// 命令列表条目最大数量 = 32
	// 命令列表最大大小 = 32*32 = 每个端口 1K
	*(unsigned long *)(SATA_ABAR_BASE | (port + PORT_CLB)) = (unsigned long) (ahci_port_base_vaddr + (portno << 10));
	memset((void *)(ahci_port_base_vaddr + (portno << 10)), 0, 1024);

	// FIS 偏移量：32K + 256 * 端口号
	// FIS 条目大小 = 每个端口 256 字节
	*(unsigned long *)(SATA_ABAR_BASE | (port + PORT_FB)) = (unsigned long)(ahci_port_base_vaddr + (32 << 10) + (portno << 8));
	memset((void *)(ahci_port_base_vaddr + (32 << 10) + (portno << 8)), 0, 256);

	// 命令表偏移：40K + 8K*portno
	// 命令表大小 = 256*32 = 每个端口 8K
	struct hba_command_header *cmdheader = (struct hba_command_header *)(*(unsigned long *)(SATA_ABAR_BASE | (port + PORT_CLB)));
	for (int i = 0; i < 32; ++i)//32个命令槽位
	{
		cmdheader[i].prdt_len = 8;  // 每个命令表 8 个 prdt 条目
		// 256 bytes per command table, 64+16+48+16*8
		// 每个命令表256字节，64+16+48+16*8
		cmdheader[i].command_table_base = (uint64_t)(ahci_port_base_vaddr + (40 << 10) + (portno << 13) + (i << 8));
		memset((void *)cmdheader[i].command_table_base, 0, 256);
	}
	start_cmd(port); // Start command engine
}

/*磁盘驱动初始化*/
void disk_init(void) {
	printf("disk_init start\n");
	// char* block_data = 0;
	/*获取磁盘控制器的bar地址*/
	pci_device_t *pci_dev= pci_get_device_by_bus(0, 8, 0);
	pci_device_dump(pci_dev);
	/*sata控制器不存在则报错*/
	if (pci_dev==NULL)
	{
		printf("[ahci]: no AHCI controllers present!\n");
	}
	SATA_ABAR_BASE = CSR_DMW0_BASE | pci_dev->bar[0].base_addr;
	for(int i = 0; i < PCI_MAX_BAR; i++) {
		printf("pci_dev->bar[i].base_addr %p\n", pci_dev->bar[i].base_addr);
	}
	/*注册中断处理程序*/
	printf("SATA_ABAR_BASE %p\n", SATA_ABAR_BASE);
	// unsigned int* hba_base = (unsigned int*)SATA_ABAR_BASE;
	*(unsigned int *)(SATA_ABAR_BASE | HBA_GHC) |= HBA_GHC_IE;//全局中断使能
	*(unsigned int *)(SATA_ABAR_BASE | HBA_GHC) |= HBA_GHC_AHCI_ENABLE;//启用ahci
	ahci_probe_port();  // 扫描ahci的所有端口
	// port_rebase(1);//开启1号端口
	port_rebase(2);
	printf("disk_init down\n");
}