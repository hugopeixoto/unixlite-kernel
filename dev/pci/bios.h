#ifndef _PCI_BIOS_H
#define _PCI_BIOS_H

#define BIOS_PCI_FUNC_ID 		0xb1XX
#define BIOS_PCI_BIOS_PRESENT 		0xb101
#define BIOS_FIND_PCI_DEV		0xb102
#define BIOS_FIND_PCI_CLASS_CODE	0xb103
#define BIOS_GENERATE_SPECIAL_CYCLE	0xb106
#define BIOS_RDCFGB			0xb108
#define BIOS_RDCFGW			0xb109
#define BIOS_RDCFGD			0xb10a
#define BIOS_WRCFGB			0xb10b
#define BIOS_WRCFGW			0xb10c
#define BIOS_WRCFGD			0xb10d

#define BIOS_SUCC			0x00
#define BIOS_FUNC_NOT_SUPPORTED		0x81
#define BIOS_BAD_VENDOR_ID		0x83
#define BIOS_DEV_NOT_FOUND		0x86
#define BIOS_BAD_REGISTER_NUMBER	0x87

/* rd:read, wr:write, cfg:config, devfn:device function, serv:service */
extern int pcifindclass(ulong classcode, ushort index, uchar* bus, uchar* devfn);
extern int pcifinddev(ushort vend, ushort devid, ushort index, uchar* bus, uchar* devfn);
extern int pcirdcfgb(uchar bus, uchar devfn, uchar where, uchar* value);
extern int pcirdcfgw(uchar bus, uchar devfn, uchar where, ushort* value);
extern int pcirdcfgd(uchar bus, uchar devfn, uchar where, ulong* value);
extern int pciwrcfgb(uchar bus, uchar devfn, uchar where, uchar value);
extern int pciwrcfgw(uchar bus, uchar devfn, uchar where, ushort value);
extern int pciwrcfgd(uchar bus, uchar devfn, uchar where, ulong value);

#include "pci.h"
#endif
