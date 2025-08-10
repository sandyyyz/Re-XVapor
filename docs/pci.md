[PCI-PCIe](https://zhuanlan.zhihu.com/p/26172972)
[PCI驱动](https://www.cnblogs.com/LoyenWang/p/14165852.html)
![PCI](image.png)
![PCI-2](image-1.png)

1. Host Bridge，比如PC中常见的North Bridge（北桥）；  
图中处理器、Cache、内存子系统通过Host Bridge连接到PCI上，Host Bridge管理PCI总线域，是联系处理器和PCI设备的桥梁，完成处理器与PCI设备间的数据交换。其中数据交换，包含处理器访问PCI设备的地址空间和PCI设备使用DMA机制访问主存储器，在PCI设备用DMA访问存储器时，会存在Cache一致性问题，这个也是Host Bridge设计时需要考虑的；
此外，Host Bridge还可选的支持仲裁机制，热插拔等；

2. PCI Local Bus；  
PCI总线，由Host Bridge或者PCI-to-PCI Bridge管理，用来连接各类设备，比如声卡、网卡、IDE接口等。可以通过PCI-to-PCI Bridge来扩展PCI总线，并构成多级总线的总线树，比如图中的PCI Local Bus #0和PCI Local Bus #1两条PCI总线就构成一颗总线树，同属一个总线域；

3. PCI-To-PCI Bridge；  
PCI桥，用于扩展PCI总线，使采用PCI总线进行大规模系统互联成为可能，管理下游总线，并转发上下游总线之间的事务；

4. PCI Device；
PCI总线中有三类设备：PCI从设备，PCI主设备，桥设备。
PCI从设备：被动接收来自Host Bridge或者其他PCI设备的读写请求；
PCI主设备：可以通过总线仲裁获得PCI总线的使用权，主动向其他PCI设备或主存储器发起读写请求；  
桥设备：管理下游的PCI总线，并转发上下游总线之间的总线事务，包括PCI桥、PCI-to-ISA桥、PCI-to-Cardbus桥等。

![pci-conf-header](image-5.png)

## 如何写PCI驱动？

1. 枚举PCI设备，访问配置空间。每个设备有256字节大小的配置空间，包含64个32位大小的寄存器。前64字节为头部，包含设备的所有信息和PCI相关设置。设备的配置空间地址寄存器地址可以通过bus,dev,funct,reg组合出。
2. 一条PCI总线上的设备共享总线，总线之间可以通过PCI-PCI桥接器连接。一条总线上最多连接32个设备，每个设备最多8个功能，最多有256条总线。所以枚举PCI设备需要256x32x8次访问设备空间。检测venderid==0xffff表示设备不存在。
3. 是直接通过访问地址访问配置空间吗？还是需要特殊指令？
4. 通过读取command寄存器初始化设备。
5. 一个设备有6个Base Address Register(BAR寄存器)。对BAR内容的解释是设备相关的。kernel可以通过对BAR寄存器定义的地址写入内容来操纵设备(IO空间或者内存空间)
6. 可以通过查询配置空间中的IRQ判断设备使用的是哪条IRQ线。cpu可以读取IRQ处理设备中断
7. MSI(信息中断)用于解决传统IRQ引脚式中断所带来的竞态问题，即设备尚未通过PCI总线将数据写入内存，中断已经产生，(由于PCI总线繁忙，数据还未写入)导致CPU读到错误数据。在MSI方式中,CPU监听PCI的地址总线和数据总线，设备通过向内存中某个位置写入某个特殊格式的值来引发中断。这样就保证了中断触发时，设备一定有总线的使用权，数据一定已经写入了目标内存。
8. 通过检查status寄存器可以判断能力链表是否存在。通过检查capabilities pointer定义的 capability structure 能力链表来判断是否支持MSI， capability structure中capabilityID == 0x5代表支持MSI。启用MSI功能可以通过设置capability structure中Message Control字段实现，可以设置设备允许使用的中断数量，设备最多使用的中断数量，以及使能MSI。







