/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __PXE_UTILS_H
#define __PXE_UTILS_H

#include <abuf.h>
#include <alist.h>
#include <bootflow.h>
#include <linux/list.h>

/*
 * A note on the pxe file parser.
 *
 * We're parsing files that use syslinux grammar, which has a few quirks.
 * String literals must be recognized based on context - there is no
 * quoting or escaping support. There's also nothing to explicitly indicate
 * when a label section completes. We deal with that by ending a label
 * section whenever we see a line that doesn't include.
 *
 * As with the syslinux family, this same file format could be reused in the
 * future for non pxe purposes. The only action it takes during parsing that
 * would throw this off is handling of include files. It assumes we're using
 * pxe, and does a tftp download of a file listed as an include file in the
 * middle of the parsing operation. That could be handled by refactoring it to
 * take a 'include file getter' function.
 *
 * See doc/develop/bootstd/pxe_api.rst for API documentation.
 */

/**
 * struct pxe_label - describes a single label in a pxe file
 *
 * Create these with label_create()
 *
 * @num: String version of the ID number of the label, e.g. "1"
 * @name: name of the 'label' line
 * @menu: name of the menu as given on the 'menu label' line
 * @kernel_label: the kernel label, including FIT config if present
 * @kernel: the path to the kernel file to use for this label
 * @config: FIT configuration to use (after '#'), or NULL if none
 * @append: kernel command line to use when booting this label
 * @initrd: path to single initrd (used if !CONFIG_PXE_INITRD_LIST)
 * @initrds: list of initrd paths (alist of char *) (used if
 * CONFIG_PXE_INITRD_LIST)
 * @fdt: path to FDT to use
 * @fdtdir: path to FDT directory to use
 * @files: list of files to load (alist of struct pxe_file)
 * @say: message to print when this label is selected for booting
 * @ipappend: flags for appending IP address (0x1) and MAC address (0x3)
 * @attempted: 0 if we haven't tried to boot this label, 1 if we have
 * @localboot: 1 if this label specified 'localboot', 0 otherwise
 * @localboot_val: value of the localboot parameter
 * @kaslrseed: 1 to generate kaslrseed from hw_rng
 * @list: lets these form a list, which a pxe_menu struct will hold.
 */
struct pxe_label {
	char num[12];
	char *name;
	char *menu;
	char *kernel_label;
	char *kernel;
	char *config;
	char *append;
	char *initrd;
	struct alist initrds;
	char *fdt;
	char *fdtdir;
	struct alist files;
	char *say;
	int ipappend;
	int attempted;
	int localboot;
	int localboot_val;
	int kaslrseed;
	struct list_head list;
};

/**
 * struct pxe_include - an include file that needs to be loaded
 *
 * @path: Path to the include file
 * @cfg: Menu to parse the include into
 * @nest_level: Nesting level to use when parsing this include
 */
struct pxe_include {
	char *path;
	struct pxe_menu *cfg;
	int nest_level;
};

/**
 * enum pxe_file_type_t - type of file to load for PXE boot
 *
 * @PFT_KERNEL: Kernel image
 * @PFT_INITRD: Initial ramdisk
 * @PFT_FDT: Flattened device tree
 * @PFT_FDTOVERLAY: Device tree overlay
 */
enum pxe_file_type_t {
	PFT_KERNEL,
	PFT_INITRD,
	PFT_FDT,
	PFT_FDTOVERLAY,
};

/**
 * struct pxe_file - a file that needs to be loaded
 *
 * @path: Path to the file
 * @type: Type of file (kernel, initrd, etc.)
 * @addr: Address where file was loaded (filled by caller)
 * @size: Size of loaded file (filled by caller)
 */
struct pxe_file {
	char *path;
	enum pxe_file_type_t type;
	ulong addr;
	ulong size;
};

/*
 * Describes a pxe menu as given via pxe files.
 *
 * title - the name of the menu as given by a 'menu title' line.
 * default_label - the name of the default label, if any.
 * fallback_label - the name of the fallback label, if any.
 * bmp - the bmp file name which is displayed in background
 * timeout - time in tenths of a second to wait for a user key-press before
 *           booting the default label.
 * prompt - if 0, don't prompt for a choice unless the timeout period is
 *          interrupted.  If 1, always prompt for a choice regardless of
 *          timeout.
 * labels - a list of labels defined for the menu.
 * includes - list of struct pxe_include for files that need loading/parsing
 */
struct pxe_menu {
	char *title;
	char *default_label;
	char *fallback_label;
	char *bmp;
	int timeout;
	int prompt;
	struct list_head labels;
	struct alist includes;
};

struct pxe_context;

/**
 * Read a file
 *
 * @ctx: PXE context
 * @file_path: Full path to filename to read
 * @addrp: On entry, address to load file or 0 to reserve an address with lmb;
 * on exit, address to which the file was loaded
 * @align: Reservation alignment, if using lmb
 * @type: File type
 * @fileszeip: Returns file size
 */
typedef int (*pxe_getfile_func)(struct pxe_context *ctx, const char *file_path,
				ulong *addrp, ulong align,
				enum bootflow_img_t type, ulong *filesizep);

/**
 * struct pxe_context - context information for PXE parsing
 *
 * @getfile: Function called by PXE to read a file
 * @userdata: Data the caller requires for @getfile
 * @allow_abs_path: true to allow absolute paths
 * @bootdir: Directory that files are loaded from ("" if no directory). This is
 *	allocated
 * @pxe_file_size: Size of the PXE file
 * @use_ipv6: TRUE : use IPv6 addressing, FALSE : use IPv4 addressing
 * @use_fallback: TRUE : use "fallback" option as default, FALSE : use
 *	"default" option as default
 * @no_boot: Stop show of actually booting and just return
 * @quiet: Suppress "Retrieving file" messages when loading files
 * @bflow: Bootflow being booted, or NULL if none (must be valid if @no_boot)
 * @cfg: PXE menu (NULL if not yet probed)
 *
 * The following are only used when probing for a label
 * @label: Label to process
 * @kern_addr_str: String containing kernel address (cannot be NULL)
 * @kern_addr: Kernel address (cannot be 0)
 * @kern_size: Kernel size in bytes
 * @initrd_addr: initaddr address (0 if none)
 * @initrd_size: initrd size (only used if @initrd_addr)
 * @initrd_str: initrd string to process (only used if @initrd_addr)
 * @fdt_addr: FDT address from loaded file (0 if none)
 * @conf_fdt_str: FDT-address string
 * @conf_fdt: FDT address
 * @fdt: Working FDT pointer, for kaslrseed and overlay operations
 * @restart: true to use BOOTM_STATE_RESTART instead of BOOTM_STATE_START (only
 *	supported with FIT / bootm)
 * @fake_go: Do a 'fake' boot, up to the last possible point, then return
 */
struct pxe_context {
	/**
	 * getfile() - read a file
	 *
	 * @ctx: PXE context
	 * @file_path: Path to the file
	 * @file_addr: String containing the hex address to put the file in
	 *	memory
	 * @filesizep: Returns the file size in bytes
	 * Return 0 if OK, -ve on error
	 */
	pxe_getfile_func getfile;

	void *userdata;
	bool allow_abs_path;
	char *bootdir;
	ulong pxe_file_size;
	bool use_ipv6;
	bool use_fallback;
	bool no_boot;
	bool quiet;
	struct bootflow *bflow;
	struct pxe_menu *cfg;

	/* information on the selected label to boot */
	struct pxe_label *label;
	char *kern_addr_str;
	ulong kern_addr;
	ulong kern_size;
	ulong initrd_addr;
	ulong initrd_size;
	char *initrd_str;
	ulong fdt_addr;
	char *conf_fdt_str;
	ulong conf_fdt;
	void *fdt;		/* working FDT pointer, for kaslrseed/overlays */
	bool restart;
	bool fake_go;
};

/**
 * pxe_menu_init() - Allocate and initialise a pxe_menu structure
 *
 * Return: Allocated structure, or NULL on failure
 */
struct pxe_menu *pxe_menu_init(void);

/**
 * pxe_menu_uninit() - Free a pxe_menu structure
 *
 * Free the memory used by a pxe_menu and its labels
 *
 * @cfg: Config to free, previously returned from pxe_menu_init()
 */
void pxe_menu_uninit(struct pxe_menu *cfg);

/**
 * get_pxe_file() - Read a file
 *
 * Retrieve the file at 'file_path' to the locate given by 'file_addr'. If
 * 'bootfile' was specified in the environment, the path to bootfile will be
 * prepended to 'file_path' and the resulting path will be used.
 *
 * @ctx: PXE context
 * @file_path: Path to file
 * @file_addr: Address to place file
 * Returns 1 on success, or < 0 for error
 */
int get_pxe_file(struct pxe_context *ctx, const char *file_path,
		 ulong file_addr);

/**
 * get_pxelinux_path() - Read a file from the same place as pxelinux.cfg
 *
 * Retrieves a file in the 'pxelinux.cfg' folder. Since this uses get_pxe_file()
 * to do the hard work, the location of the 'pxelinux.cfg' folder is generated
 * from the bootfile path, as described in get_pxe_file().
 *
 * @ctx: PXE context
 * @file: Relative path to file
 * @pxefile_addr_r: Address to load file
 * Returns 1 on success or < 0 on error.
 */
int get_pxelinux_path(struct pxe_context *ctx, const char *file,
		      ulong pxefile_addr_r);

/**
 * handle_pxe_menu() - Boot the system as prescribed by a pxe_menu.
 *
 * Use the menu system to either get the user's choice or the default, based
 * on config or user input.  If there is no default or user's choice,
 * attempted to boot labels in the order they were given in pxe files.
 * If the default or user's choice fails to boot, attempt to boot other
 * labels in the order they were given in pxe files.
 *
 * If this function returns, there weren't any labels that successfully
 * booted, or the user interrupted the menu selection via ctrl+c.
 *
 * @ctx: PXE context
 * @cfg: PXE menu
 */
void handle_pxe_menu(struct pxe_context *ctx, struct pxe_menu *cfg);

/**
 * parse_pxefile() - Parse a PXE file
 *
 * Parse the top-level file. Any includes are stored in cfg->includes and
 * should be processed by calling pxe_process_includes().
 *
 * @ctx: PXE context (provided by the caller)
 * @buf: Buffer containing the PXE file
 * Return: NULL on error, otherwise a pointer to a pxe_menu struct. Use
 * pxe_menu_uninit() to free it.
 */
struct pxe_menu *parse_pxefile(struct pxe_context *ctx, struct abuf *buf);

/**
 * pxe_process_includes() - Process include files in a parsed menu
 *
 * Load and parse all include files referenced in cfg->includes. This may
 * add more includes if nested includes are found.
 *
 * @ctx: PXE context with getfile callback
 * @cfg: Parsed PXE menu with includes to process
 * @base: Memory address for loading include files
 * Return: 0 on success, -ve on error
 */
int pxe_process_includes(struct pxe_context *ctx, struct pxe_menu *cfg,
			 ulong base);

/**
 * format_mac_pxe() - Convert a MAC address to PXE format
 *
 * Convert an ethaddr from the environment to the format used by pxelinux
 * filenames based on mac addresses. Convert's ':' to '-', and adds "01-" to
 * the beginning of the ethernet address to indicate a hardware type of
 * Ethernet. Also converts uppercase hex characters into lowercase, to match
 * pxelinux's behavior.
 *
 * @outbuf: Buffer to hold the output (must hold 22 bytes)
 * @outbuf_len: Length of buffer
 * Returns 1 for success, -ENOENT if 'ethaddr' is undefined in the
 * environment, or some other value < 0 on error.
 */
int format_mac_pxe(char *outbuf, size_t outbuf_len);

/**
 * pxe_setup_ctx() - Setup a new PXE context
 *
 * @ctx: Context to set up
 * @getfile: Function to call to read a file
 * @userdata: Data the caller requires for @getfile - stored in ctx->userdata
 * @allow_abs_path: true to allow absolute paths
 * @bootfile: Bootfile whose directory loaded files are relative to, NULL if
 *	none
 * @use_ipv6: TRUE : use IPv6 addressing
 *            FALSE : use IPv4 addressing
 * @use_fallback: TRUE : Use "fallback" option instead of "default" should no
 *                       other choice be selected
 *                FALSE : Use "default" option should no other choice be
 *                        selected
 * @bflow: Bootflow to update, NULL if none
 * Return: 0 if OK, -ENOMEM if out of memory, -E2BIG if bootfile is larger than
 *	MAX_TFTP_PATH_LEN bytes
 */
int pxe_setup_ctx(struct pxe_context *ctx, pxe_getfile_func getfile,
		  void *userdata, bool allow_abs_path, const char *bootfile,
		  bool use_ipv6, bool use_fallback, struct bootflow *bflow);

/**
 * pxe_destroy_ctx() - Destroy a PXE context
 *
 * @ctx: Context to destroy
 */
void pxe_destroy_ctx(struct pxe_context *ctx);

/**
 * pxe_process_str() - Process a PXE file through to boot
 *
 * Note: The file at @pxefile_addr_r must be a nul-terminated string.
 *
 * @ctx: PXE context created with pxe_setup_ctx()
 * @pxefile_addr_r: Address of config to process
 * @prompt: Force a prompt for the user
 */
int pxe_process_str(struct pxe_context *ctx, ulong pxefile_addr_r, bool prompt);

/**
 * pxe_process() - Process a PXE file through to boot
 *
 * @ctx: PXE context created with pxe_setup_ctx()
 * @addr: Address of config to process
 * @size: Size of continue to process
 * @prompt: Force a prompt for the user
 */
int pxe_process(struct pxe_context *ctx, ulong addr, ulong size, bool prompt);

/**
 * pxe_get_file_size() - Read the value of the 'filesize' environment variable
 *
 * @sizep: Place to put the value
 * Return: 0 if OK, -ENOENT if no such variable, -EINVAL if format is invalid
 */
int pxe_get_file_size(ulong *sizep);

/**
 * pxe_get_fdt_fallback() - Get the FDT address using fallback logic
 *
 * When a label doesn't specify an FDT file (via 'fdt' or 'fdtdir'), this
 * function determines the FDT address using fallback environment variables:
 *   1. fdt_addr - if set, use this address
 *   2. fdtcontroladdr - if set and kernel is not FIT format
 *
 * @label: Label being processed
 * @kern_addr: Address where kernel is loaded
 * Return: FDT address string from environment, or NULL if no fallback available
 */
const char *pxe_get_fdt_fallback(struct pxe_label *label, ulong kern_addr);

/**
 * pxe_get() - Get the PXE file from the server
 *
 * This tries various filenames to obtain a PXE file
 *
 * @pxefile_addr_r: Address to put file
 * @bootdirp: Returns the boot filename, or NULL if none. This is the 'bootfile'
 *	option provided by the DHCP server. If none, returns NULL. For example,
 *	"rpi/info", which indicates that all files should be fetched from the
 *	"rpi/" subdirectory
 * @sizep: Size of the PXE file (not bootfile)
 * @use_ipv6: TRUE : use IPv6 addressing
 *            FALSE : use IPv4 addressing
 */
int pxe_get(ulong pxefile_addr_r, char **bootdirp, ulong *sizep, bool use_ipv6);

/**
 * pxe_probe() - Process a PXE file to find the label to boot
 *
 * This fills in the label, etc. fields in @ctx, assuming it funds something to
 * boot. Then pxe_boot() can be called to boot it.
 *
 * @ctx: PXE context created with pxe_setup_ctx()
 * @pxefile_addr_r: Address to load file
 * @prompt: Force a prompt for the user
 * Return: 0 if OK, -ve on error
 */
int pxe_probe(struct pxe_context *ctx, ulong pxefile_addr_r, bool prompt);

/**
 * pxe_boot() - Boot the selected label
 *
 * This boots the label discovered by pxe_probe()
 *
 * Return: Does not return, on success, otherwise returns a -ve error code
 */
int pxe_boot(struct pxe_context *ctx);

/**
 * pxe_select_label() - Select a label from a parsed menu
 *
 * Uses the menu system to get the user's choice or the default.
 * Does NOT load any files or attempt to boot.
 *
 * @cfg: Parsed PXE menu
 * @prompt: Force user prompt regardless of timeout
 * @labelp: Returns selected label (not a copy, points into cfg)
 * Return: 0 on success, -ENOMEM if out of memory, -ENOENT if no default set,
 *	-ECANCELED if user cancelled
 */
int pxe_select_label(struct pxe_menu *cfg, bool prompt,
		     struct pxe_label **labelp);

/**
 * pxe_load_files() - Load kernel/initrd/FDT/overlays for a label
 *
 * Loads the files specified in the label into memory and saves the
 * addresses in @ctx. This does not process the FDT or set up boot
 * parameters - use pxe_load_label() for that.
 *
 * @ctx: PXE context with getfile callback
 * @label: Label whose files to load
 * @fdtfile: Path to FDT file (may be NULL)
 * Return: 0 on success, -ENOENT if no kernel specified, -EIO if file
 *	retrieval failed
 */
int pxe_load_files(struct pxe_context *ctx, struct pxe_label *label,
		   char *fdtfile);

/**
 * pxe_load_label() - Load kernel/initrd/FDT for a label
 *
 * Loads the files specified in the label into memory. Call
 * pxe_setup_label() after this to process the FDT and set up
 * boot parameters.
 *
 * @ctx: PXE context with getfile callback
 * @label: Label whose files to load
 * Return: 0 on success, -ENOENT if no kernel specified, -EIO if file
 *	retrieval failed, -ENOMEM if out of memory
 */
int pxe_load_label(struct pxe_context *ctx, struct pxe_label *label);

/**
 * pxe_setup_label() - Set up boot parameters for a loaded label
 *
 * Processes the FDT (applying overlays if needed) and saves the boot
 * parameters in @ctx. Call this after pxe_load_label().
 *
 * @ctx: PXE context with loaded files
 * @label: Label to set up
 * Return: 0 on success, -ENOSPC if initrd string too long, -ENOMEM if
 *	out of memory
 */
int pxe_setup_label(struct pxe_context *ctx, struct pxe_label *label);

/**
 * parse_pxefile_top() - Entry point for parsing a menu file
 *
 * @ctx: PXE context
 * @p: Start of buffer containing the PXE file
 * @limit: End of buffer (position of nul terminator)
 * @cfg: Menu to parse into
 * @nest_level: Nesting level (1 for top level, higher for includes)
 * Returns 1 on success, < 0 on error.
 */
int parse_pxefile_top(struct pxe_context *ctx, char *p, const char *limit,
		      struct pxe_menu *cfg, int nest_level);

/**
 * label_destroy() - free the memory used by a pxe_label
 *
 * This frees @label itself as well as memory used by its name,
 * kernel, config, append, initrd, fdt, fdtdir and fdtoverlay members, if
 * they're non-NULL.
 *
 * So - be sure to only use dynamically allocated memory for the members of
 * the pxe_label struct, unless you want to clean it up first. These are
 * currently only created by the pxe file parsing code.
 *
 * @label: Label to free
 */
void label_destroy(struct pxe_label *label);

/**
 * pxe_parse_include() - Parse an included file into its target menu
 *
 * After loading an include file referenced in cfg->includes, call this
 * to parse it and merge any labels into the target menu. This may add
 * more entries to cfg->includes if the included file has its own includes.
 *
 * Example usage::
 *
 *    ctx = pxe_parse(addr, size, bootfile);
 *
 *    // Load and process any includes
 *    alist_for_each(inc, &ctx->cfg->includes) {
 *        // read file inc->path to address 'addr', getting 'size'
 *        pxe_parse_include(ctx, inc, addr, size);
 *    }
 *
 * @ctx: PXE context
 * @inc: Include info with path and target menu
 * @addr: Memory address where file is located
 * @size: Size of the file in bytes
 * Return: 1 on success, -ve on error
 */
int pxe_parse_include(struct pxe_context *ctx, const struct pxe_include *inc,
		      ulong addr, ulong size);

/**
 * pxe_parse() - Parse a PXE config file and return an allocated context
 *
 * This allocates a PXE context, sets it up, and parses the config file at the
 * given address. The menu is stored in ctx->cfg and can be used to inspect
 * labels or select one for loading.
 *
 * Use pxe_cleanup() to clean up when done.
 *
 * Note: This does not process include files. Call pxe_process_includes()
 * after this if needed.
 *
 * @addr: Address where config file is loaded
 * @size: Size of config file in bytes
 * @bootfile: Path to config file (for relative path resolution)
 * Return: Allocated context on success, NULL on error
 */
struct pxe_context *pxe_parse(ulong addr, ulong size, const char *bootfile);

/**
 * pxe_load() - Report that a file has been loaded
 *
 * After loading a file from a label's files list, call this to record
 * the load address and size. This information is used later during boot.
 *
 * Example::
 *
 *    alist_for_each(file, &label->files) {
 *        // choose 'addr', load file->path there, getting 'size'
 *        pxe_load(file, addr, size);
 *    }
 *
 * @file: File that was loaded
 * @addr: Address where file was loaded
 * @size: Size of loaded file
 */
void pxe_load(struct pxe_file *file, ulong addr, ulong size);

/**
 * pxe_cleanup() - Free PXE menu and destroy context
 *
 * This combines pxe_menu_uninit() and pxe_destroy_ctx() for convenience
 * when cleaning up after using the callback-free API.
 *
 * @ctx: Context to destroy (ctx->cfg is freed if set)
 */
void pxe_cleanup(struct pxe_context *ctx);

/**
 * pxe_boot_entry() - Parse a PXE config and boot a specific entry by index
 *
 * This parses the config file, walks to the Nth label, loads its files and
 * boots it. Used by multi-entry bootmeths to boot a specific label that was
 * selected during scanning.
 *
 * @ctx: PXE context (must be set up with pxe_setup_ctx())
 * @addr: Address where config file is loaded
 * @entry: Entry index (0 for first label, 1 for second, etc.)
 * Return: Does not return on success, -ve on error
 */
int pxe_boot_entry(struct pxe_context *ctx, ulong addr, int entry);

#endif /* __PXE_UTILS_H */
