/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2009 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#define LOG_ELS		0x00000001	/* ELS events */
#define LOG_DISCOVERY	0x00000002	/* Link discovery events */
#define LOG_MBOX	0x00000004	/* Mailbox events */
#define LOG_INIT	0x00000008	/* Initialization events */
#define LOG_LINK_EVENT	0x00000010	/* Link events */
#define LOG_IP		0x00000020	/* IP traffic history */
#define LOG_FCP		0x00000040	/* FCP traffic history */
#define LOG_NODE	0x00000080	/* Node table events */
#define LOG_TEMP	0x00000100	/* Temperature sensor events */
#define LOG_BG		0x00000200	/* BlockGuard events */
#define LOG_MISC	0x00000400	/* Miscellaneous events */
#define LOG_SLI		0x00000800	/* SLI events */
#define LOG_FCP_ERROR	0x00001000	/* log errors, not underruns */
#define LOG_LIBDFC	0x00002000	/* Libdfc events */
#define LOG_VPORT	0x00004000	/* NPIV events */
#define LOG_SECURITY	0x00008000	/* Security events */
#define LOG_EVENT	0x00010000	/* CT,TEMP,DUMP, logging */
#define LOG_FIP		0x00020000	/* FIP events */
#define LOG_FCP_UNDER	0x00040000	/* FCP underruns errors */
#define LOG_SCSI_CMD	0x00080000	/* ALL SCSI commands */
#define LOG_ALL_MSG	0xffffffff	/* LOG all messages */

#define lpfc_printf_vlog(vport, level, mask, fmt, arg...) \
do { \
	{ if (((mask) & (vport)->cfg_log_verbose) || (level[1] <= '3')) \
		dev_printk(level, &((vport)->phba->pcidev)->dev, "%d:(%d):" \
			   fmt, (vport)->phba->brd_no, vport->vpi, ##arg); } \
} while (0)

#define lpfc_printf_log(phba, level, mask, fmt, arg...) \
do { \
	{ uint32_t log_verbose = (phba)->pport ? \
				 (phba)->pport->cfg_log_verbose : \
				 (phba)->cfg_log_verbose; \
	  if (((mask) & log_verbose) || (level[1] <= '3')) \
		dev_printk(level, &((phba)->pcidev)->dev, "%d:" \
			   fmt, phba->brd_no, ##arg); \
	} \
} while (0)