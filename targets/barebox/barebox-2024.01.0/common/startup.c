// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * (C) Copyright 2002-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 */

#ifdef CONFIG_DEBUG_INITCALLS
#define DEBUG
#endif

/**
 * @file
 * @brief Main entry into the C part of barebox
 */
#include <common.h>
#include <shell.h>
#include <init.h>
#include <command.h>
#include <malloc.h>
#include <debug_ll.h>
#include <fs.h>
#include <errno.h>
#include <slice.h>
#include <linux/stat.h>
#include <envfs.h>
#include <magicvar.h>
#include <linux/reboot-mode.h>
#include <asm/sections.h>
#include <uncompress.h>
#include <globalvar.h>
#include <console_countdown.h>
#include <environment.h>
#include <linux/ctype.h>
#include <watchdog.h>
#include <glob.h>
#include <net.h>
#include <bselftest.h>
#include <libfile.h>
#include <base64.h>
#include <s_record.h>
#include "nyx_api.h"
#include "paging_uefi_x64.h"
extern initcall_t __barebox_initcalls_start[], __barebox_early_initcalls_end[],
		  __barebox_initcalls_end[];

extern exitcall_t __barebox_exitcalls_start[], __barebox_exitcalls_end[];


#if defined CONFIG_FS_RAMFS && defined CONFIG_FS_DEVFS
static int mount_root(void)
{
	mount("none", "ramfs", "/", NULL);
	mkdir("/dev", 0);
	mkdir("/tmp", 0);
	mount("none", "devfs", "/dev", NULL);

	if (IS_ENABLED(CONFIG_FS_EFIVARFS)) {
		mkdir("/efivars", 0);
		mount("none", "efivarfs", "/efivars", NULL);
	}

	if (IS_ENABLED(CONFIG_FS_PSTORE)) {
		mkdir("/pstore", 0);
		mount("none", "pstore", "/pstore", NULL);
	}

	return 0;
}
fs_initcall(mount_root);
#endif

static int load_environment(void)
{
	const char *default_environment_path;
	int __maybe_unused ret = 0;

	default_environment_path = default_environment_path_get();

	if (IS_ENABLED(CONFIG_DEFAULT_ENVIRONMENT))
		defaultenv_load("/env", 0);

	if (IS_ENABLED(CONFIG_ENV_HANDLING)) {
		envfs_load(default_environment_path, "/env", 0);
	} else {
		if (IS_ENABLED(CONFIG_DEFAULT_ENVIRONMENT))
			pr_notice("No support for persistent environment. Using default environment\n");
	}

	nvvar_load();

	return 0;
}
environment_initcall(load_environment);

static int global_autoboot_abort_key;
static const char * const global_autoboot_abort_keys[] = {
	"any",
	"ctrl-c",
};
static int global_autoboot_timeout = 3;

static const char * const global_autoboot_states[] = {
	[AUTOBOOT_COUNTDOWN] = "countdown",
	[AUTOBOOT_ABORT] = "abort",
	[AUTOBOOT_MENU] = "menu",
	[AUTOBOOT_BOOT] = "boot",
};
static int global_autoboot_state = AUTOBOOT_COUNTDOWN;

static bool test_abort(void)
{
	bool do_abort = false;
	int c, ret;
	char key;

	while (tstc()) {
		c = getchar();
		if (tolower(c) == 'q' || c == 3)
			do_abort = true;
	}

	if (!do_abort)
		return false;

	printf("Abort init sequence? (y/n)\n"
	       "Will continue with init sequence in:");

	ret = console_countdown(5, CONSOLE_COUNTDOWN_EXTERN, "yYnN", &key);
	if (!ret)
		return false;

	if (tolower(key) == 'y')
		return true;

	return false;
}

#define INITFILE "/env/bin/init"
#define MENUFILE "/env/menu/mainmenu"

/**
 * set_autoboot_state - set the autoboot state
 * @autoboot: the state to set
 *
 * This functions sets the autoboot state to the given state. This can be called
 * by boards which have its own idea when and how the autoboot shall be aborted.
 */
void set_autoboot_state(enum autoboot_state autoboot)
{
	global_autoboot_state = autoboot;
}

/**
 * do_autoboot_countdown - print autoboot countdown to console
 *
 * This prints the autoboot countdown to the console and waits for input. This
 * evaluates the global.autoboot_abort_key to determine which keys are allowed
 * to interrupt booting and also global.autoboot_timeout to determine the timeout
 * for the counter. This function can be called multiple times, it is executed
 * only the first time.
 *
 * Returns an enum autoboot_state value containing the user input.
 */
enum autoboot_state do_autoboot_countdown(void)
{
	static enum autoboot_state autoboot_state = AUTOBOOT_UNKNOWN;
	unsigned flags = CONSOLE_COUNTDOWN_EXTERN;
	int ret;
	struct stat s;
	bool menu_exists;
	char *abortkeys = NULL;
	unsigned char outkey;

	if (autoboot_state != AUTOBOOT_UNKNOWN)
		return autoboot_state;

	if (!console_get_first_active()) {
		printf("\nNon-interactive console, booting system\n");
		return autoboot_state = AUTOBOOT_BOOT;
	}

	if (global_autoboot_state != AUTOBOOT_COUNTDOWN)
		return global_autoboot_state;

	menu_exists = stat(MENUFILE, &s) == 0;

	if (menu_exists) {
		printf("\nHit m for menu or %s to stop autoboot: ",
		       global_autoboot_abort_keys[global_autoboot_abort_key]);
		abortkeys = "m";
	} else {
		printf("\nHit %s to stop autoboot: ",
		       global_autoboot_abort_keys[global_autoboot_abort_key]);
	}

	switch (global_autoboot_abort_key) {
	case 0:
		flags |= CONSOLE_COUNTDOWN_ANYKEY;
		break;
	case 1:
		flags |= CONSOLE_COUNTDOWN_CTRLC;
		break;
	default:
		break;
	}

	command_slice_release();
	ret = console_countdown(global_autoboot_timeout, flags, abortkeys,
				&outkey);
	command_slice_acquire();

	if (ret == 0)
		autoboot_state = AUTOBOOT_BOOT;
	else if (menu_exists && outkey == 'm')
		autoboot_state = AUTOBOOT_MENU;
	else
		autoboot_state = AUTOBOOT_ABORT;

	return autoboot_state;
}

static int register_autoboot_vars(void)
{
	globalvar_add_simple_enum("autoboot_abort_key",
				  &global_autoboot_abort_key,
                                  global_autoboot_abort_keys,
				  ARRAY_SIZE(global_autoboot_abort_keys));
	globalvar_add_simple_int("autoboot_timeout",
				 &global_autoboot_timeout, "%u");
	globalvar_add_simple_enum("autoboot",
				  &global_autoboot_state,
				  global_autoboot_states,
				  ARRAY_SIZE(global_autoboot_states));

	return 0;
}
postcore_initcall(register_autoboot_vars);

static int run_init(void)
{
	const char *bmode;
	bool env_bin_init_exists;
	enum autoboot_state autoboot;
	struct stat s;
	glob_t g;
	int i, ret;

	setenv("PATH", "/env/bin");
	export("PATH");

	/* Run legacy /env/bin/init if it exists */
	env_bin_init_exists = stat(INITFILE, &s) == 0;
	if (env_bin_init_exists) {
		pr_info("running %s...\n", INITFILE);
		run_command("source " INITFILE);
		return 0;
	}

	/* Unblank console cursor */
	printf("\e[?25h");

	if (test_abort()) {
		pr_info("Init sequence aborted\n");
		return -EINTR;
	}

	/* Run scripts in /env/init/ */
	ret = glob("/env/init/*", 0, NULL, &g);
	if (!ret) {
		for (i = 0; i < g.gl_pathc; i++) {
			const char *path = g.gl_pathv[i];
			char *scr;

			ret = stat(path, &s);
			if (ret)
				continue;

			if (!S_ISREG(s.st_mode))
				continue;

			pr_debug("Executing '%s'...\n", path);
			scr = basprintf("source %s", path);
			run_command(scr);
			free(scr);
		}

		globfree(&g);
	}

	/* source matching script in /env/bmode/ */
	bmode = reboot_mode_get();
	if (bmode) {
		char *scr, *path;

		scr = xasprintf("source /env/bmode/%s", bmode);
		path = &scr[strlen("source ")];
		if (stat(path, &s) == 0) {
			pr_info("Invoking '%s'...\n", path);
			run_command(scr);
		}
		free(scr);
	}

	autoboot = do_autoboot_countdown();

	console_ctrlc_allow();

	if (autoboot == AUTOBOOT_BOOT)
		run_command("boot");

	if (IS_ENABLED(CONFIG_NET))
		eth_open_all();

	if (autoboot == AUTOBOOT_MENU)
		run_command(MENUFILE);

	if (autoboot == AUTOBOOT_ABORT && autoboot == global_autoboot_state)
		watchdog_inhibit_all();

	run_shell();
	run_command(MENUFILE);

	return 0;
}

typedef void (*ctor_fn_t)(void);

/* Call all constructor functions linked into the kernel. */
static void do_ctors(void)
{
#ifdef CONFIG_CONSTRUCTORS
	ctor_fn_t *fn = (ctor_fn_t *) __ctors_start;

	for (; fn < (ctor_fn_t *) __ctors_end; fn++)
		(*fn)();
#endif
}

int (*barebox_main)(void);


int  efi_bio_driver_register(void);
void fuzz_nfs(void);
void fuzz_sntp(void);
void fuzz_ping(void);
void fuzz_image(void *buf, int size);
int fuzz_start = 0;
kAFL_payload *payload_buffer;
void __noreturn start_barebox(void)
{
	initcall_t *initcall;
	int result;
    
	if (!IS_ENABLED(CONFIG_SHELL_NONE) && IS_ENABLED(CONFIG_COMMAND_SUPPORT))
		barebox_main = run_init;

	do_ctors();

	for (initcall = __barebox_initcalls_start;
			initcall < __barebox_initcalls_end; initcall++) {
		pr_debug("initcall-> %pS\n", *initcall);
		result = (*initcall)();
		if (result)
			pr_err("initcall %pS failed: %s\n", *initcall,
					strerror(-result));
	}
	
	pr_debug("initcalls done\n");

	if (IS_ENABLED(CONFIG_SELFTEST_AUTORUN))
		selftests_run();
	
	

	struct eth_device *edev = eth_get_byname("eth0");
	if(!edev)
	{
		printf("eth0 not found\n");
		while(1)
		{
			;
		}
	}

	ifup_edev(edev, IFUP_FLAG_SKIP_CONF);
	edev->ifup = true;

    init_fuzz_intc();

	make_directory("/fuzzfs");

    volatile host_config_t host_config;
    volatile agent_config_t agent_config = {0};
	
    kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
    kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);

    kAFL_hypercall(HYPERCALL_KAFL_USER_SUBMIT_MODE, KAFL_MODE_64);

	
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

    payload_buffer = memalign(0x1000,host_config.payload_buffer_size);
    kAFL_hypercall (HYPERCALL_KAFL_SET_AGENT_CONFIG, (uint64_t)&agent_config);

    uint64_t buffer_kernel[3] = {0};


    buffer_kernel[0] = 0x7000;
    buffer_kernel[1] = 0x20000000;
    buffer_kernel[2] = 0;
    kAFL_hypercall(HYPERCALL_KAFL_RANGE_SUBMIT, (uint64_t)buffer_kernel);
    kAFL_hypercall (HYPERCALL_KAFL_GET_PAYLOAD, (uint64_t)payload_buffer);
    
    kAFL_hypercall (HYPERCALL_KAFL_NEXT_PAYLOAD, 0);
    kAFL_hypercall (HYPERCALL_KAFL_ACQUIRE, 0);

    
    fuzz_start = 1;
	

	efi_bio_driver_register();
	

	
	char buf[5];
    int ret = mount("/dev/disk0.0", NULL, "/fuzzfs", NULL);
	if (!ret)
	{
		
		
		int fd = open("/fuzzfs/a.txt.ln",O_RDWR);
		if(!fd)
		{
			fd = open("/fuzzfs/a.txt",O_RDWR);
		}
		if (fd)
		{
			kAFL_hypercall(HYPERCALL_KAFL_PRINTF, "kAFL fuzzer initialized.");
			pread(fd, buf, 5, 3);
			pwrite(fd, buf, 5, 1);
			close(fd);
		}
		unlink("/fuzzfs/a.txt.ln");
		mkdir ("/fuzzfs/b", 0755);
		fd = creat("/fuzzfs/b/b.txt", 0);
		if (fd)
		{
			pwrite(fd, buf, 5, 0);
			pread(fd, buf, 5, 0);  //read from write only mode
			close(fd);
		}
		DIR *dir;
		struct dirent *d;
		dir = opendir("/fuzzfs");
		while ((d = readdir(dir))) 
		{
			;
		}
		closedir(dir);
		rmdir ("/fuzzfs/b");
		unlink("/fuzzfs/a.txt");
	}

	// // base64decode
	// char *buf = memalign(0x1000,0x1000);
	// decode_base64(buf, 0x1000, payload_buffer->data);

	// fuzz_image(payload_buffer->data, payload_buffer->size);

	// json not compiled for barebox
	// unsigned int num_tokens;
	// jsmntok_t *ret = jsmn_parse_alloc(payload_buffer->data, payload_buffer->size,&num_tokens);
	// if(ret)
	// {
	// 	const char *path[] = {"game", "settings","graphics","resolution", NULL};
	// 	jsmn_strdup(path, payload_buffer->data, ret);
	// }
	
	// ratp 

	// int count;
	// ulong addr;
	// srec_decode (payload_buffer->data, &count, &addr, payload_buffer->data);


	// fdt_add_subnode(payload_buffer->data, 3, "fuzz");
	// fdt_del_node(payload_buffer->data, 3);
	// fdt_setprop_u64(payload_buffer->data, 5, "fuzz1",5);
	// fdt_set_name(payload_buffer->data, 3, "set");
	// fdt_add_mem_rsv(payload_buffer->data, 0x12345678, 0x1000);


    
	
	// eth_log(edev);
	// IPaddr_t ip;
	// IPaddr_t netmask;
	// IPaddr_t gateway;
	// IPaddr_t nameserver;
	// string_to_ip("192.168.1.10", &ip);
	// string_to_ip("255.255.255.0", &netmask);
	// string_to_ip("192.168.1.3", &gateway);
	// string_to_ip("134.96.225.88", &nameserver);


	// net_set_ip(edev, ip);
	// net_set_netmask(edev, netmask);
	// net_set_gateway(gateway);
	// net_set_nameserver(nameserver);

	// IPaddr_t target;
	// resolv("internal.wjqwjq.com", &target);

	// fuzz_nfs();
	// fuzz_sntp();
	// fuzz_ping();

	kAFL_hypercall (HYPERCALL_KAFL_RELEASE, 0);
	while(1)
	{
		;
	}
	

	if (barebox_main)
		barebox_main();

    

	if (IS_ENABLED(CONFIG_SHELL_NONE)) {
		pr_err("Nothing left to do\n");
		hang();
	} else {
		while (1)
			run_shell();
	}
}

void __noreturn hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}

/* Everything needed to cleanly shutdown barebox.
 * Should be called before starting an OS to get
 * the devices into a clean state
 */
void shutdown_barebox(void)
{
	exitcall_t *exitcall;

	for (exitcall = __barebox_exitcalls_start;
			exitcall < __barebox_exitcalls_end; exitcall++) {
		pr_debug("exitcall-> %pS\n", *exitcall);
		(*exitcall)();
	}

	console_flush();
}

BAREBOX_MAGICVAR(global.autoboot,
                 "Autoboot state. Possible values: countdown (default), abort, menu, boot");
BAREBOX_MAGICVAR(global.autoboot_abort_key,
                 "Which key allows to interrupt autoboot. Possible values: any, ctrl-c");
BAREBOX_MAGICVAR(global.autoboot_timeout,
                 "Timeout before autoboot starts in seconds");
