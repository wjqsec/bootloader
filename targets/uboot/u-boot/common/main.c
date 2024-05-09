// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/* #define	DEBUG	*/

#include <common.h>
#include <autoboot.h>
#include <button.h>
#include <bootstage.h>
#include <bootstd.h>
#include <cli.h>
#include <command.h>
#include <console.h>
#include <env.h>
#include <fdtdec.h>
#include <init.h>
#include <net.h>
#include <fs.h>
#include <malloc.h>
#include <version_string.h>
#include <efi_loader.h>
#include <dm.h>
#include <expo.h>
#include <mapmem.h>
#include <part.h>
#include <slre.h>
#include <fdtdec.h>
#include <dm/ofnode.h>
#include <linux/bch.h>
#include "nyx_api.h"
#include "paging.h"

extern char *net_dns_resolve;		/* The host to resolve  */
extern char *net_dns_env_var;		/* the env var to put the ip into */
extern struct udp_ops sntp_ops;
extern struct in_addr	net_ntp_server;
static void run_preboot_environment_command(void)
{
	char *p;

	p = env_get("preboot");
	if (p != NULL) {
		int prev = 0;

		if (IS_ENABLED(CONFIG_AUTOBOOT_KEYED))
			prev = disable_ctrlc(1); /* disable Ctrl-C checking */

		run_command_list(p, -1, 0);

		if (IS_ENABLED(CONFIG_AUTOBOOT_KEYED))
			disable_ctrlc(prev);	/* restore Ctrl-C checking */
	}
}


kAFL_payload *payload_buffer;
int fuzz_start = 0;
/* We come here after U-Boot is initialised and ready to process commands */
void main_loop(void)
{
	const char *s;

	bootstage_mark_name(BOOTSTAGE_ID_MAIN_LOOP, "main_loop");

	if (IS_ENABLED(CONFIG_VERSION_VARIABLE))
		env_set("ver", version_string);  /* set version variable */

	cli_init();

	if (IS_ENABLED(CONFIG_USE_PREBOOT))
		run_preboot_environment_command();

	if (IS_ENABLED(CONFIG_UPDATE_TFTP))
		update_tftp(0UL, NULL, NULL);

	if (IS_ENABLED(CONFIG_EFI_CAPSULE_ON_DISK_EARLY)) {
		/* efi_init_early() already called */
		if (efi_init_obj_list() == EFI_SUCCESS)
			efi_launch_capsules();
	}

	process_button_cmds();

	s = bootdelay_process();
	if (cli_process_fdt(&s))
		cli_secure_boot_cmd(s);
	
	// autoboot_command(s);
	
	//-----------------------------------------------------------
	

	void *page_table_list_ptr = malloc(sizeof(struct page_table_list) + 4096);
	void *page_dir_ptr = malloc(sizeof(struct page_dir)  + 4096);
	
	void *idt_ptr = malloc(sizeof(struct idt));
	init_paging(ROUNDUP_TO_PAGE_ALIGNED(page_dir_ptr),ROUNDUP_TO_PAGE_ALIGNED(page_table_list_ptr));
	
	
    for(unsigned long i = 0 ; i < 0x40000000 ; i+= 0x1000) 
    {
        map_unmap_page(i, 1);
    }
	
    enable_paging();
	init_fuzz_intc(idt_ptr);
	 
	volatile host_config_t host_config;
    volatile agent_config_t agent_config = {0};

    kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
    kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);

    kAFL_hypercall(HYPERCALL_KAFL_USER_SUBMIT_MODE, KAFL_MODE_32);

    kAFL_hypercall(HYPERCALL_KAFL_GET_HOST_CONFIG, (uint32_t)&host_config);
    if (host_config.host_magic != NYX_HOST_MAGIC || host_config.host_version != NYX_HOST_VERSION) 
    {
        habort("magic error\n");
    }
    agent_config.agent_magic = NYX_AGENT_MAGIC;
    agent_config.agent_version = NYX_AGENT_VERSION;

    agent_config.agent_tracing = 0;
    agent_config.agent_ijon_tracing = 0;
    agent_config.agent_non_reload_mode = 0; // persistent fuzz!
    agent_config.coverage_bitmap_size = host_config.bitmap_size;

    payload_buffer = ROUNDUP_TO_PAGE_ALIGNED(malloc(host_config.payload_buffer_size + 4096));
    kAFL_hypercall (HYPERCALL_KAFL_SET_AGENT_CONFIG, (uint32_t)&agent_config);

    uint64_t buffer_kernel[3] = {0};


    buffer_kernel[0] = 0x10;
    buffer_kernel[1] = 0xf0000000;
    buffer_kernel[2] = 0;
    kAFL_hypercall(HYPERCALL_KAFL_RANGE_SUBMIT, (uint32_t)buffer_kernel);
    kAFL_hypercall (HYPERCALL_KAFL_GET_PAYLOAD, payload_buffer);
    
    kAFL_hypercall (HYPERCALL_KAFL_NEXT_PAYLOAD, 0);
    kAFL_hypercall (HYPERCALL_KAFL_ACQUIRE, 0);
	///* file system harness for btrfs erofs ext4 fat squashfs
	
	fuzz_start = 1;

	struct udevice *dev;
	uclass_first_device_err(UCLASS_IDE, &dev);
	
	struct disk_partition info;
	struct blk_desc *dev_desc = NULL;
	int part = blk_get_device_part_str("ide","0:1", &dev_desc, &info, 1);

	fs_set_blk_dev("ide","0:1", 0);
	fs_ls("/"); 

	fs_set_blk_dev("ide","0:1", 0);	
	int exist = fs_exists("/a.txt");
	if(exist)
	{
		// kAFL_hypercall(HYPERCALL_KAFL_PRINTF, "kAFL fuzzer initialized.");
		printf("/a.txt exist\n");
	}

	fs_set_blk_dev("ide","0:1", 0);
	char buf[10];
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	loff_t actread = 0;
	fs_read("/a.txt", (ulong)buf, 0, 5, &actread);
	printf("fd actread %lld %x %x %x\n",actread,buf[0],buf[1],buf[2]);

	fs_set_blk_dev("ide","0:1", 0);
	fs_read("/a.txt.ln", (ulong)buf, 0, 5, &actread);
	printf("fd actread %lld %x %x %x\n",actread,buf[0],buf[1],buf[2]);

	fs_set_blk_dev("ide","0:1", 0);
	fs_unlink("/a.txt.ln");

	fs_set_blk_dev("ide","0:1", 0);
	struct fs_dir_stream *dir = fs_opendir("/");
	if (dir)
	{
		fs_set_blk_dev("ide","0:1", 0);
		struct fs_dirent * dirent = fs_readdir(dir);
		fs_set_blk_dev("ide","0:1", 0);
		fs_closedir(dir);
	}

	// // harness for jffs2 ubifs yaffs cannot link symbol
	// // harness for zfs (seed not work)
	zfs_set_blk_dev(dev_desc, &info);
	int vdev;
	zfs_ls(&vdev, "/", 0);
	// harness for reiserfs
	int dev_ = dev_desc->devnum;
	if (part > 0)
	{
		reiserfs_set_blk_dev(dev_desc, &info);
		if (reiserfs_mount(info.size)) 
		{
			printf ("** Bad Reiserfs partition or disk - %s %d:%d **  %d\n", "ide", dev_, part,info.size);
			reiserfs_ls ("/");
			char buf[5];
			int fd = reiserfs_open ("/a.txt");
			if(fd) 
			{
				reiserfs_read (buf, 5);
			}
			fd = reiserfs_open ("/a.txt.ln");
			if(fd) 
			{
				reiserfs_read (buf, 5);
			}
		}
	}
	
	
	
	// command line harness
	// run_command_list(payload_buffer->data, -1, 0);
	
	
	///* fdt parser harness
	// fdtdec_setup();
	// struct fdt_memory gic_rd_tables;

	// gic_rd_tables.start = 0x100000;
	// gic_rd_tables.end = 0x100000 + 0x100000 - 1;
	// fdtdec_add_reserved_memory(payload_buffer->data, "gic-rd-tables", &gic_rd_tables,
	// 				 NULL, 0, NULL, 0);
	// fdtdec_decode_ram_size(payload_buffer->data, 0, 0,0, 0,0);
	// fdtdec_set_ethernet_mac_address(payload_buffer->data, "\x00\xB0\xD0\x63\xC2\x26", 6);

	// slre harness
	// struct slre slre;
	// char *pattern_email = "[\\w\\.-]+@[\\w\\.-]+";
	// char *pattern_date = "\\b\\d{4}-\\d{2}-\\d{2}\\b";
	// char *pattern_ipv4 = "\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b";
	// char *pattern_time = "([01]?[0-9]|2[0-3]):[0-5][0-9]";
	// char *pattern_uuid = "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}";
	// char *pattern_nest = "\\((?>[^()]+|(?R))*\\)";
	// char *pattern_phone = "(\\+\\d{1,2}\\s)?\\(?\\d{3}\\)?[\\s.-]?\\d{3}[\\s.-]?\\d{4}";

	// if (slre_compile(&slre, pattern_email)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }
	// if (slre_compile(&slre, pattern_date)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }
	// if (slre_compile(&slre, pattern_ipv4)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }
	// if (slre_compile(&slre, pattern_time)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }
	// if (slre_compile(&slre, pattern_uuid)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }
	// if (slre_compile(&slre, pattern_nest)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }
	// if (slre_compile(&slre, pattern_phone)) 
	// {
	// 	slre_match(&slre, payload_buffer->data , payload_buffer->size , 0);
	// }

	// ping harness
	// net_ip = string_to_ip("192.168.1.10");
	// net_netmask = string_to_ip("255.255.255.0");
	// net_gateway = string_to_ip("192.168.1.3");
	// net_dns_server = string_to_ip("134.96.225.88");

	// strcpy(net_boot_file_name,"a.img");
	// net_server_ip =  string_to_ip("134.96.225.88");
	// net_ping_ip = string_to_ip("134.96.225.88");
	// net_ntp_server = string_to_ip("134.96.225.88");
	// net_dns_resolve = "internal.wjqwjq.com";
	// net_dns_env_var = NULL;

	// if (net_loop(PING) < 0)
	// {
	// 	// kAFL_hypercall(HYPERCALL_KAFL_PRINTF, "kAFL fuzzer initialized.");
	// 	printf("host is not alive\n");
	// }
	// else
	// {
	// 	kAFL_hypercall(HYPERCALL_KAFL_PRINTF, "kAFL fuzzer initialized.");
	// 	printf("host is alive\n");
	// }

	// if (net_loop(WGET) < 0)
	// {
	// 	printf("wget is error\n");
	// }
	// else
	// {
	// 	printf("wget is ok\n");
	// }
	
	// if (net_loop(DNS) < 0)
	// {
	// 	printf("dns lookup of failed\n");
	// }
	// else
	// {
	// 	printf("dns lookup of ok\n");
	// }

	// if (net_loop(TFTPGET) < 0)
	// {
	// 	printf("TFTPGET lookup of failed\n");
	// }
	// else
	// {
	// 	printf("TFTPGET lookup of ok\n");
	// }

	// if (net_loop(TFTPPUT) < 0) //permission denied
	// {
	// 	printf("TFTPPUT lookup of failed\n");
	// }
	// else
	// {
	// 	printf("TFTPPUT lookup of ok\n");
	// }


	// if (udp_loop(&sntp_ops) < 0) 
	// {
	// 	printf("sntp_ops lookup of failed\n");
	// }
	// else
	// {
	// 	printf("sntp_ops lookup of ok\n");
	// }

	// strcpy(net_boot_file_name,"/home/w/hd/boot_fuzz/net-server/nfs//a.img");
	// if (net_loop(NFS) < 0) 
	// {
	// 	printf("NFS lookup of failed\n");
	// }
	// else
	// {
	// 	printf("NFS lookup of ok\n");
	// }

	kAFL_hypercall (HYPERCALL_KAFL_RELEASE, 0);
	//-------------------------------------------------

	/* if standard boot if enabled, assume that it will be able to boot */
	if (IS_ENABLED(CONFIG_BOOTSTD_PROG)) {
		int ret;

		ret = bootstd_prog_boot();
		printf("Standard boot failed (err=%dE)\n", ret);
		panic("Failed to boot");
	}

	
	// cli_loop();

	panic("No CLI available");
}
