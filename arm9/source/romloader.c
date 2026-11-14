#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "c_defs.h"
#include "minIni.h"
#include "menu.h"
#include "NesMachine.h"
//#include "extlink_filestruct.h"

extern int subscreen_stat;
extern volatile int emu_vblank_paused;

char tmpname[] = "dszip.tmp";

char diskbios[8192];
const int diskbios_size = sizeof(diskbios);

int romsize;	// actual rom size
int freemem_start;	// points to unused mem (end of loaded rom)
int freemem_end;
u32 oldinput;

char romfilename[256];	// name of current rom.  null when there's no file (rom was embedded)
char *romfileext;	// points to file extension

//extern u8 autostate;			// from ui.c
int active_interface = 0;

#define FDS_HEADER_SIZE 16
#define FDS_SIDE_SIZE 65500
#define FDS_DIRTY_BUFFER_SIZE 0x40000

enum {
	ROM_LOAD_ERROR_NONE,
	ROM_LOAD_ERROR_FDS_BIOS,
	ROM_LOAD_ERROR_FDS_IMAGE
};

static int rom_load_error;
static int diskbios_loaded;
static int fds_image_loaded;
static int fds_image_has_header;
static size_t fds_image_data_size;
static char fds_image_path[1024];
extern u8 NES_DISK[];

void rommenu(int roms);
void drawmenu(int sel, int roms);
int getinput(void);

int init_rommenu(void);
void listrom(int line, int rom, int highlight); // print rom title
int loadrom();	// return -1 on success
int selectrom(int rom);	// return -1 on success, or rom count (from directory change)
void stringsort(const char **s);

static int is_fds_filename(const char *name)
{
	const char *extension = strrchr(name, '.');
	const char *inner_extension = NULL;
	size_t prefix_length;

	if (extension == NULL)
		return 0;
	if (strcmp(extension, ".fds") == 0 || strcmp(extension, ".FDS") == 0)
		return 1;
	if (strcmp(extension, ".gz") != 0 && strcmp(extension, ".GZ") != 0 &&
		strcmp(extension, ".zip") != 0 && strcmp(extension, ".ZIP") != 0)
		return 0;

	prefix_length = (size_t)(extension - name);
	while (prefix_length > 0) {
		if (name[prefix_length - 1] == '.') {
			inner_extension = name + prefix_length - 1;
			break;
		}
		prefix_length--;
	}
	return inner_extension != NULL && extension - inner_extension == 4 &&
		(strncmp(inner_extension, ".fds", 4) == 0 ||
		strncmp(inner_extension, ".FDS", 4) == 0);
}

static int is_direct_fds_filename(const char *name)
{
	const char *extension = strrchr(name, '.');
	return extension != NULL &&
		(strcmp(extension, ".fds") == 0 || strcmp(extension, ".FDS") == 0);
}

static int valid_fds_side_count(size_t count)
{
	return count >= 1 && count <= 4;
}

static int valid_raw_fds_size(size_t size)
{
	return size == FDS_SIDE_SIZE || size == FDS_SIDE_SIZE * 2 ||
		size == FDS_SIDE_SIZE * 3 || size == FDS_SIDE_SIZE * 4;
}

static int validate_fds_stream(FILE *file, int *has_header, size_t *data_size)
{
	u8 header[FDS_HEADER_SIZE];
	u8 signature[3];
	long image_size;
	size_t sides;

	if (fseek(file, 0, SEEK_END) != 0)
		return 0;
	image_size = ftell(file);
	if (image_size < 0 || image_size > ROM_MAX_SIZE)
		return 0;
	if (valid_raw_fds_size((size_t)image_size)) {
		if (fseek(file, 0, SEEK_SET) != 0 ||
			fread(signature, 1, sizeof(signature), file) != sizeof(signature) ||
			memcmp(signature, "FDS", sizeof(signature)) == 0 ||
			fseek(file, 0, SEEK_SET) != 0)
			return 0;
		*has_header = 0;
		*data_size = image_size;
		return 1;
	}
	if (image_size < FDS_HEADER_SIZE ||
		fseek(file, 0, SEEK_SET) != 0 ||
		fread(header, 1, sizeof(header), file) != sizeof(header) ||
		memcmp(header, "FDS\x1a", 4) != 0)
		return 0;

	sides = header[4];
	if (!valid_fds_side_count(sides) ||
		(size_t)image_size != FDS_HEADER_SIZE + FDS_SIDE_SIZE * sides)
		return 0;
	*has_header = 1;
	*data_size = FDS_SIDE_SIZE * sides;
	return fseek(file, 0, SEEK_SET) == 0;
}

static int validate_fds_image(const u8 *image, size_t image_size,
	int *has_header, size_t *data_size)
{
	size_t sides;

	if (valid_raw_fds_size(image_size)) {
		if (memcmp(image, "FDS", 3) == 0)
			return 0;
		*has_header = 0;
		*data_size = image_size;
		return 1;
	}
	if (image_size < FDS_HEADER_SIZE ||
		memcmp(image, "FDS\x1a", 4) != 0)
		return 0;

	sides = image[4];
	if (!valid_fds_side_count(sides) ||
		image_size != FDS_HEADER_SIZE + FDS_SIDE_SIZE * sides)
		return 0;
	*has_header = 1;
	*data_size = FDS_SIDE_SIZE * sides;
	return 1;
}

static int stream_has_fds_magic(FILE *file)
{
	u8 signature[3];
	int has_magic;

	if (fseek(file, 0, SEEK_SET) != 0)
		return 1;
	if (fread(signature, 1, sizeof(signature), file) != sizeof(signature)) {
		has_magic = ferror(file) != 0;
		if (fseek(file, 0, SEEK_SET) != 0)
			return 1;
		return has_magic;
	}
	has_magic = memcmp(signature, "FDS", sizeof(signature)) == 0;
	if (fseek(file, 0, SEEK_SET) != 0)
		return 1;
	return has_magic;
}

static int make_fds_image_path(const char *filename, char *path, size_t path_size)
{
	char directory[768];
	int length;

	if (!is_direct_fds_filename(filename) ||
		getcwd(directory, sizeof(directory)) == NULL)
		return 0;
	length = snprintf(path, path_size, "%s/%s", directory, filename);
	return length >= 0 && (size_t)length < path_size;
}

static int fds_disk_has_writes(void)
{
	return fds_image_data_size <= FDS_DIRTY_BUFFER_SIZE &&
		memchr(NES_DISK, 0xff, fds_image_data_size) != NULL;
}

static void clear_fds_session(void)
{
	fds_image_loaded = 0;
	fds_image_has_header = 0;
	fds_image_data_size = 0;
	fds_image_path[0] = '\0';
}

int save_fds(void)
{
	const u8 *header;
	const u8 *data;
	size_t output_size;
	size_t bytes_written;
	size_t side_count;
	FILE *file;
	int target_has_header;
	size_t target_data_size;
	int write_failed;

	if (!active_interface || !diskbios_loaded || !(__emuflags & DISKBIOS) ||
		!fds_image_loaded || globals.mapperNr != 20 ||
		fds_image_path[0] == '\0' || globals.romBase == NULL ||
		fds_image_data_size == 0 ||
		fds_image_data_size > FDS_DIRTY_BUFFER_SIZE)
		return 0;

	header = globals.romBase - FDS_HEADER_SIZE;
	if (memcmp(header, "FDS\x1a", 4) != 0)
		return 0;
	side_count = header[4];
	if (!valid_fds_side_count(side_count) ||
		FDS_SIDE_SIZE * side_count != fds_image_data_size ||
		(globals.prgSize16k >> 2) != side_count ||
		!fds_disk_has_writes())
		return 0;

	file = fopen(fds_image_path, "rb");
	if (file == NULL)
		return -1;
	write_failed = !validate_fds_stream(file, &target_has_header, &target_data_size) ||
		target_has_header != fds_image_has_header ||
		target_data_size != fds_image_data_size;
	if (fclose(file) != 0)
		write_failed = 1;
	if (write_failed)
		return -1;

	if (fds_image_has_header) {
		data = header;
		output_size = FDS_HEADER_SIZE + fds_image_data_size;
	} else {
		data = globals.romBase;
		output_size = fds_image_data_size;
	}

	file = fopen(fds_image_path, "wb");
	if (file == NULL)
		return -1;
	bytes_written = fwrite(data, 1, output_size, file);
	write_failed = bytes_written != output_size || fflush(file) != 0;
	if (fclose(file) != 0)
		write_failed = 1;
	if (write_failed)
		return -1;

	memset(NES_DISK, 0, FDS_DIRTY_BUFFER_SIZE);
	return 1;
}

char *adjust_fds(const char *name, char *rom, int romSize)
{
	char *newPtr = rom;
	if (is_fds_filename(name)) {
		int discCount = 0;
		if (romSize == 65500) {
			discCount = 1;
		}
		else if (romSize == 65500 * 2) {
			discCount = 2;
		}
		else if (romSize == 65500 * 3) {
			discCount = 3;
		}
		else if (romSize == 65500 * 4) {
			discCount = 4;
		}
		if (discCount != 0) {
			newPtr -= 16;
			memset(newPtr, 0, FDS_HEADER_SIZE);
			memcpy(newPtr, "FDS\x1a", 4);
			newPtr[4] = discCount;
		}
	}
	return newPtr;
}

int is_nsf_file(const char *name, char *rom)
{
	// actually the first four chars should be "NESM"
	if (strstr(name, ".NSF") || strstr(name, ".nsf")) {
		memcpy(&nsfHeader, rom, sizeof(nsfHeader));
		__emuflags |= NSFFILE;
		__nsfSongNo = 0;
		__nsfPlay = 0;
		__nsfInit = 0;
		IPC_MAPPER = 256;
		return 1;
	}
	__emuflags &= ~NSFFILE;
	return 0;
}

/*****************************
* name:			do_rommenu
* function:		show a menu to select a nes rom file.
* argument:		none
* description:		called when starting.
******************************/
int global_roms = 0;

void do_rommenu() {
	int roms = global_roms;
	int i;

	ips_stat = false;		// disable ips first.
	emu_vblank_paused = 1;
	DMA_CR(0) = 0;
	DMA_CR(1) = 0;

	BG_PALETTE[0] = 0;
	videoSetMode(MODE_0_2D);
	REG_MASTER_BRIGHT = (2 << 14) | 16;

	(void)Sound_pause();

	if (!global_roms) {
		roms = init_rommenu();
		global_roms = roms;
	}

	if (!roms) {
		if (!active_interface) {			// another driver is present, but init failed
			consoletext(64*3,"Device failed.",0);
		} else {						// no DLDI error, no files
			consoletext(64*3,"No roms found.",0);
		}
		if (rom_load_error != ROM_LOAD_ERROR_NONE) {
			consoletext(64*4, rom_load_error == ROM_LOAD_ERROR_FDS_BIOS ?
				"disksys.rom not found" : "Invalid FDS image", 0x1000);
			rom_load_error = ROM_LOAD_ERROR_NONE;
		}
		if (romfilename[0] != '\0') {
			for (i = 0; i < 60; i++)
				swiWaitForVBlank();
			apply_display_settings();
			(void)Sound_resume();
			menu_stat = 0;
			menu_draw = 0;
			hideconsole();
			clearconsole();
			emu_vblank_paused = 0;
			return;
		}
		while(1) swiWaitForVBlank();
	}
	showconsole();
	clearconsole();
	showversion();
	rommenu(global_roms);
	menu_stat = 0;
	menu_draw = 0;
	hideconsole();
	clearconsole();
	emu_vblank_paused = 0;
}



/*****************************
* name:			rommenu
* function:		show a rom menu.
* argument:		roms: the count of valid roms.
* description:		called by do_rommenu.
******************************/
void rommenu(int roms) {
	static int selectedrom = 0;
	int key;
	int sel = selectedrom;
	int loaded = 0;

	if (romfilename[0] != '\0')
		save_sram();

	oldinput = IPC_KEYS;

	if (roms == 1 && *((char **)rom_files)[0] == 2)
		key = KEY_START;
	else
		key = 0x80000000;
	do {
		swiWaitForVBlank();
		switch(key) {
			case KEY_START:
			case KEY_B:
			case KEY_A:
			case KEY_Y:
				loaded = selectrom(sel);
				if (loaded > 0) {	// didn't load (it was a directory) loaded == 0 means that ips file is loaded.
					sel = 0;
					roms = loaded;
					global_roms = roms;
				}
				break;
			case KEY_RIGHT:
				sel += 10;
				if (sel > roms-1) sel = roms-1;
				break;
			case KEY_LEFT:
				sel -= 10;
				if (sel < 0) sel = 0;
				break;
			case KEY_UP:
				sel = sel+roms-1;
				break;
			case KEY_DOWN:
				sel++;
				break;
		}
		sel %= roms;
		if (key && loaded >= 0) {
			drawmenu(sel,roms);
			if (rom_load_error != ROM_LOAD_ERROR_NONE) {
				consoletext(0, rom_load_error == ROM_LOAD_ERROR_FDS_BIOS ?
					"disksys.rom not found" : "Invalid FDS image", 0x1000);
				rom_load_error = ROM_LOAD_ERROR_NONE;
			}
		}
		key = getinput();
	} while (loaded >= 0);

	selectedrom = sel;
}

#define ROWS 21
#define OFFSET 3
/*****************************
* name:			drawmenu
* function:		show a rom menu.
* argument:		sel: the select number
			roms: count of roms
* description:		called by rommenu.
******************************/
void drawmenu(int sel, int roms) {
	int i,j,topline,rom;
	
	clearconsole();
	if (roms > ROWS) {
		topline = 8*(roms-ROWS)*sel/(roms-1);
		rom = topline/8;
		j = (rom<roms-ROWS)?ROWS+1:ROWS;
	} else {
		topline = 0;
		rom = 0;
		j = roms;
	}
	
	for (i=0;i<j;i++) {
		listrom(i+OFFSET,rom,sel==rom);
		rom++;
	}

//	if (roms>ROWS)
		REG_BG0VOFS_SUB = topline%8;
//	else
//		SUB_BG0_Y0 = -(ROWS*4)+roms*4;
}

/*****************************
* name:			getinput
* function:		get a key input for rom selection.
* argument:		none
* description:		called by rommenu.
******************************/
int getinput() {
	static int lastdpad,repeatcount=0;
	int dpad;
	int keyhit;
	scanKeys();
	IPC_KEYS = keysCurrent();
	keyhit = (oldinput^IPC_KEYS) & IPC_KEYS;
	oldinput = IPC_KEYS;
	dpad = IPC_KEYS & (KEY_UP+KEY_DOWN+KEY_LEFT+KEY_RIGHT);
	if (lastdpad == dpad) {
		repeatcount++;
		if (repeatcount<25 || repeatcount&3) // delay/repeat
			dpad = 0;
	} else {
		repeatcount = 0;
		lastdpad = dpad;
	}
	return dpad|(keyhit & (KEY_Y+KEY_X+KEY_A+KEY_B+KEY_START));
}

/*****************************
* name:			listrom
* function:		show a rom file name on the sub-screen.
* argument:		line: row number on the screnn.
			rom: rom number of the roms list.
			highlight: if the rom is selected(highlighted).
* description:		called by rommenu.
******************************/
void listrom(int line, int rom, int highlight) {
	const char *s;
	char **files = (char **)rom_files;
	s = files[rom];
	if (*s == 1) {	// dir
		menutext(line,s+1,highlight?2:0);
	} else		// file
		menutext(line,s+1,highlight);
}

/*****************************
* name:         loadrom
* function:		load a rom to memory, for NES emulation.
* description:	called by selectrom and bootext
*****************************/
int loadrom() {
	FILE *f; // rom file
	int i; // romsize
	char *roms; // rom data
	char *extension = strrchr(romfilename, '.');
	int fds_file = is_fds_filename(romfilename);
	int fds_has_header = 0;
	size_t fds_data_size = 0;
	size_t fds_file_size = 0;
	int compressed = strstr(romfilename, ".GZ") || strstr(romfilename, ".gz") ||
		strstr(romfilename, ".ZIP") || strstr(romfilename, ".zip");

	rom_load_error = ROM_LOAD_ERROR_NONE;
	if (extension == NULL)
		return 0;

	if (fds_file) {
		if (!diskbios_loaded || (__emuflags & DISKBIOS) == 0) {
			rom_load_error = ROM_LOAD_ERROR_FDS_BIOS;
			return 0;
		}
	}

	if (compressed) {
		irqDisable(IRQ_VBLANK);
		if (load_gz(romfilename)) {
			unlink(tmpname);
			irqEnable(IRQ_VBLANK);
			return 0;
		}
		f = fopen(tmpname,"rb");
	}
	else {
		irqDisable(IRQ_VBLANK);
		f = fopen(romfilename,"rb");
	}
	if (f == NULL) {
		if (compressed)
			unlink(tmpname);
		irqEnable(IRQ_VBLANK);
		return 0;
	}

	if (!fds_file && stream_has_fds_magic(f))
		fds_file = 1;
	if (fds_file && (!diskbios_loaded || (__emuflags & DISKBIOS) == 0)) {
		fclose(f);
		if (compressed)
			unlink(tmpname);
		irqEnable(IRQ_VBLANK);
		rom_load_error = ROM_LOAD_ERROR_FDS_BIOS;
		return 0;
	}
	if (fds_file && !validate_fds_stream(f, &fds_has_header, &fds_data_size)) {
		fclose(f);
		if (compressed)
			unlink(tmpname);
		irqEnable(IRQ_VBLANK);
		rom_load_error = ROM_LOAD_ERROR_FDS_IMAGE;
		return 0;
	}

	romfileext = extension + 1;
	roms = (char *)rom_start;
	clear_fds_session();
	if (fds_file) {
		size_t bytes_read;

		fds_file_size = fds_data_size + (fds_has_header ? FDS_HEADER_SIZE : 0);
		bytes_read = fread(roms, 1, fds_file_size, f);
		i = (int)bytes_read;
		if (bytes_read != fds_file_size) {
			fclose(f);
			if (compressed)
				unlink(tmpname);
			irqEnable(IRQ_VBLANK);
			rom_load_error = ROM_LOAD_ERROR_FDS_IMAGE;
			return 0;
		}
	} else {
		i = fread(roms,1,ROM_MAX_SIZE,f);	//read the whole thing (filesize=some huge number, don't care)
	}
	fclose(f);

	if (compressed) {
		unlink(tmpname);
	}
	irqEnable(IRQ_VBLANK);
	do_ips(i);
	if (fds_file && !validate_fds_image((u8 *)roms, (size_t)i,
		&fds_has_header, &fds_data_size)) {
		rom_load_error = ROM_LOAD_ERROR_FDS_IMAGE;
		return 0;
	}
	romsize = i;
    if (i < 0x100000)
        i = 0x100000;            // leave some room for FDS files. Also the NSF file need some extra memory
    freemem_start = ((u32)roms+i+3)&~3;
    freemem_end = (u32)roms+ROM_MAX_SIZE;
	{
		extern void cheat_reset(void);
		cheat_reset();
	}
	recorder_reset();

	if (!is_nsf_file(romfilename, roms)) {
		roms = adjust_fds(romfilename, roms, romsize);
		romcorrect(roms);
	}
	if (fds_file)
		memset(NES_DISK, 0, FDS_DIRTY_BUFFER_SIZE);
	initcart(roms);
	IPC_MAPPER = globals.mapperNr;
	if (fds_file && globals.mapperNr == 20) {
		fds_image_loaded = 1;
		fds_image_has_header = fds_has_header;
		fds_image_data_size = fds_data_size;
		if (!make_fds_image_path(romfilename, fds_image_path, sizeof(fds_image_path)))
			fds_image_path[0] = '\0';
	} else {
		clear_fds_session();
		if (fds_file) {
			rom_load_error = ROM_LOAD_ERROR_FDS_IMAGE;
			return 0;
		}
	}

	load_sram();
	//if (autostate) loadstate(..);

	return -1;	//(-1 on success)
}

/*****************************
* name:			selectrom
* function:		enter a subdir or select a rom.
* argument:		rom: rom number of the roms list.
* description:		called by rommenu.
******************************/
int selectrom(int rom) {
	char **files = (char **)rom_files;
	char old_filename[sizeof(romfilename)];
	const char *old_extension;
	int old_extension_offset = -1;
	int count;
	int result;
	size_t filename_len;

	if (*files[rom] == 1) {	// directory
		if (chdir(files[rom]+1) != 0)
			return 0;
		count = init_rommenu();
		if (count > 0)
			return count;
		if (chdir("..") != 0)
			return 0;
		return init_rommenu();
	} else {	// file
		if (strstr(files[rom]+1, ".ips") || strstr(files[rom]+1, ".IPS")) {	// a ips file is loaded.
			load_ips(files[rom]+1);
			return 0;
		}
		else {
			filename_len = strlen(files[rom]+1);
			if (filename_len >= sizeof(romfilename))
				return 0;
			memcpy(old_filename, romfilename, sizeof(old_filename));
			old_extension = strrchr(romfilename, '.');
			if (old_extension != NULL)
				old_extension_offset = (int)(old_extension - romfilename) + 1;
			memcpy(romfilename,files[rom]+1,filename_len+1);

			result = loadrom();
			if (result != -1) {
				memcpy(romfilename, old_filename, sizeof(romfilename));
				romfileext = old_extension_offset >= 0 ?
					romfilename + old_extension_offset : NULL;
			}
			return result;
		}
	}
}

/*****************************
* name:			init_rommenu
* function:		count the nes roms&dirs in current dir...
* argument:		none
* description:		called by do_rommenu.
******************************/
// return number of file entries (roms+dirs)
int init_rommenu() {
	const char **files;
	char *nextfile;
	char *names_end;
	int idx = 0;
	int is_parent;
	int is_directory;
	size_t name_len;
	DIR *dir;
	struct dirent *cnt;
	struct stat statbuf;

	if (!active_interface) {		// DLDI trouble
		return 0;
	}
	files = (const char **)rom_files;
	nextfile = (char *)&files[MAXFILES];
	names_end = nextfile + MAXFILES * 64;
	irqDisable(IRQ_VBLANK);
	dir = opendir(".");
	if (dir == NULL) {
		irqEnable(IRQ_VBLANK);
		return 0;
	}
	memset(files, 0, MAXFILES * sizeof(*files));

	while (idx < MAXFILES-1 && (cnt = readdir(dir)) != NULL) {
		is_parent = strcmp("..", cnt->d_name) == 0;
		if (is_parent) {
			is_directory = 1;
		} else {
			if (stat(cnt->d_name, &statbuf) != 0)
				continue;
			is_directory = S_ISDIR(statbuf.st_mode);
		}
		if (((strstr(cnt->d_name, ".NES") || strstr(cnt->d_name, ".nes"))
			|| (strstr(cnt->d_name, ".FDS") || strstr(cnt->d_name, ".fds"))
			|| (strstr(cnt->d_name, ".IPS") || strstr(cnt->d_name, ".ips"))
			|| (strstr(cnt->d_name, ".GZ") || strstr(cnt->d_name, ".gz"))
			|| (strstr(cnt->d_name, ".ZIP") || strstr(cnt->d_name, ".zip"))
			|| (strstr(cnt->d_name, ".nsf") || strstr(cnt->d_name, ".nsf"))
			|| is_directory) && strcmp(".", cnt->d_name) != 0) {
			name_len = strlen(cnt->d_name);
			if (name_len + 2 > (size_t)(names_end - nextfile))
				break;
			if (is_directory)
				*nextfile = 1;
			else 
				*nextfile = 2;
			memcpy(nextfile + 1, cnt->d_name, name_len + 1);
			files[idx] = nextfile;
			nextfile += name_len + 2;
			idx++;
		}
	}
	closedir(dir);
	irqEnable(IRQ_VBLANK);
	files[idx] = 0;
	stringsort(files);
	return idx;
}


/*****************************
* name:			less
* function:		return (s1 < s2)
* argument:		s1, s2
* description:		cmpare to strings.for sorting.
******************************/
// case insensitive string compare
inline int less(const char *s1, const char *s2) {
	int c1,c2;
	do {
		c1 = *s1++;
		c2 = *s2++;
		if (c1 >= 'a') c1 = c1-'a'+'A';
		if (c2 >= 'a') c2 = c2-'a'+'A';
	} while (c1 == c2);
	return c1 < c2;
}

/*****************************
* name:			stringsort
* function:		sort the strings.
* argument:		p1: pointer of string list.
* description:		none
******************************/
// lazy man's string sorter
void stringsort(const char **p1) {
	const char **p2;
	const char *s1,*s2;
	do {	// next s1
		s1 = *p1;
		if (s1) {
			p2 = p1;
			do {	// next s2
				p2++;
				s2 = *p2;
				if (s2 && less(s2,s1)) {
					*p1 = s2;
					*p2 = s1;
					s1 = s2;
				}
			} while(s2);
		}
		p1++;
	} while(s1);
}

extern int argc;
extern char **argv;
char inibuf[768];
char disksyspath[768];
char ininame[768];
char defaultDisksyspath[] = "/disksys.rom";

char *findpath(int argc, char **argv, const char *name){
	int i;
	size_t name_len = strlen(name);
	const char *default_dir = (argc > 0 && argv[0] != NULL && argv[0][0] != '\0') ? argv[0] : "/";
	size_t def_len = strlen(default_dir);
	int def_needs_slash;

	for (i = 0; i < argc; i++) {
		size_t path_len;
		int needs_slash;

		if (argv[i] == NULL)
			continue;
		path_len = strlen(argv[i]);
		if (path_len == 0 || path_len >= sizeof(ininame))
			continue;
		needs_slash = argv[i][path_len - 1] != '/';
		if (path_len + needs_slash + name_len >= sizeof(ininame))
			continue;

		memcpy(ininame, argv[i], path_len);
		if (needs_slash)
			ininame[path_len++] = '/';
		memcpy(ininame + path_len, name, name_len + 1);
		if (!access(ininame,0)) return ininame;
	}
	// create at app directory (argv[0]) on next save
	def_needs_slash = (def_len > 0 && default_dir[def_len - 1] != '/') ? 1 : 0;
	if (def_len > 0 && def_len + def_needs_slash + name_len < sizeof(ininame)) {
		memcpy(ininame, default_dir, def_len);
		if (def_needs_slash)
			ininame[def_len++] = '/';
		memcpy(ininame + def_len, name, name_len + 1);
	} else if (name_len + 2 <= sizeof(ininame)) {
		ininame[0] = '/';
		memcpy(ininame + 1, name, name_len + 1);
	} else {
		ininame[0] = '\0';
	}
	return NULL;
}

int keystr2int(const char *buf)
{
	int i;
	int ret = 0;
	if (buf[0] == 0) return 0;
	for (i = 0; i < 12; i++) {
		if (strstr(buf, keystrs[i]) != NULL) {
			ret |= (1 << i);
		}
	}
	return ret;
}

static long get_ini_long(const char *key, long default_value, long min_value, long max_value)
{
	char text[64];
	char *start;
	char *end;
	long value;
	int length = ini_gets("nesDSrev2", key, "", text, sizeof(text), ininame);

	if (length <= 0 || (size_t)length >= sizeof(text) - 1)
		return default_value;

	start = text;
	while (isspace((unsigned char)*start))
		start++;
	if (*start == '\0')
		return default_value;

	errno = 0;
	value = strtol(start, &end, 10);
	if (end == start || errno == ERANGE)
		return default_value;
	while (isspace((unsigned char)*end))
		end++;
	if (*end != '\0' || value < min_value || value > max_value)
		return default_value;
	return value;
}

static int get_ini_shortcut(int index)
{
	char text[512];
	char *start;
	char *end;
	long value;
	int length = ini_gets("nesDSrev2", ishortcuts[index], "", text, sizeof(text), ininame);

	if (length <= 0 || (size_t)length >= sizeof(text) - 1)
		return shortcuts_tbl[index];

	start = text;
	while (isspace((unsigned char)*start))
		start++;
	errno = 0;
	value = strtol(start, &end, 10);
	if (end != start) {
		while (isspace((unsigned char)*end))
			end++;
		if (*end == '\0') {
			if (errno == 0 && value >= 0 && value <= 0xfff)
				return value;
			return shortcuts_tbl[index];
		}
	}

	value = keystr2int(text);
	return value != 0 ? (int)value & 0xfff : shortcuts_tbl[index];
}

static void load_ini_gesture(int index)
{
	char text[32];
	int length;
	int i;

	length = ini_gets("nesDSrev2", igestures[index], "", text, sizeof(text), ininame);
	if (length <= 0 || length > 16 || (size_t)length >= sizeof(text) - 1)
		return;
	for (i = 0; i < length; i++) {
		if (text[i] != 'U' && text[i] != 'D' && text[i] != 'L' && text[i] != 'R')
			return;
	}
	memcpy(gestures_tbl[index], text, length + 1);
}

static void load_ini_path(const char *key, const char *default_value, char *buffer, size_t buffer_size)
{
	int length = ini_gets("nesDSrev2", key, "", buffer, buffer_size, ininame);

	if (length <= 0 || (size_t)length >= buffer_size - 1 || buffer[0] == '\0')
		length = ini_gets("", key, "", buffer, buffer_size, ininame);
	if (length <= 0 || (size_t)length >= buffer_size - 1 || buffer[0] == '\0')
		snprintf(buffer, buffer_size, "%s", default_value);
}

static void set_default_config(void)
{
	const char *app_path = NULL;
	const char *colon = NULL;
	const char *slash = NULL;
	size_t directory_length;

	rom_load_error = ROM_LOAD_ERROR_NONE;
	diskbios_loaded = 0;
	clear_fds_session();
	__emuflags = NOFLICKER;
	joyflags &= ~(B_A_SWAP | L_R_DISABLE | AUTOFIRE);

	ad_scale = DEFAULT_SCREEN_SCALE;
	ad_ypos = DEFAULT_SCREEN_OFFSET;
	soft_frameskip = 3;
	autofire_fps = 2;
	gammavalue = 0;
	palette_value = 0;
	slots_num = 0;
	screen_swap = 0;
	use_saves_dir = false;
	__af_state = 0;
	__af_start = ((autofire_fps >> 1) << 8) + (autofire_fps >> 1) + (autofire_fps & 1);
	reset_shortcuts();

	memset(inibuf, 0, sizeof(inibuf));
	if (argc > 0 && argv != NULL && argv[0] != NULL) {
		app_path = argv[0];
		colon = strchr(app_path, ':');
		if (colon != NULL && colon[1] == '/')
			app_path = colon + 1;
		slash = strrchr(app_path, '/');
	}
	if (slash != NULL) {
		directory_length = (size_t)(slash - app_path + 1);
		if (directory_length < sizeof(inibuf))
			memcpy(inibuf, app_path, directory_length);
	}
	if (inibuf[0] == '\0')
		strcpy(inibuf, "/");

	strcpy(disksyspath, defaultDisksyspath);
	ininame[0] = '\0';
	memset(diskbios, 0, sizeof(diskbios));
}

static int load_diskbios(const char *path)
{
	FILE *bios = fopen(path, "rb");
	int loaded = 0;
	long bios_size;

	if (bios == NULL) {
		diskbios_loaded = 0;
		return 0;
	}
	if (fseek(bios, 0, SEEK_END) == 0) {
		bios_size = ftell(bios);
		if (bios_size == (long)sizeof(diskbios) &&
			fseek(bios, 0, SEEK_SET) == 0 &&
			fread(diskbios, 1, sizeof(diskbios), bios) == sizeof(diskbios))
			loaded = 1;
	}
	fclose(bios);
	if (!loaded)
		memset(diskbios, 0, sizeof(diskbios));
	diskbios_loaded = loaded;
	return loaded;
}

static void build_dir_filepath(char *dst, size_t dst_size, const char *dir, const char *filename)
{
	size_t dir_len;
	size_t file_len;
	int needs_slash;

	if (dst == NULL || dst_size == 0)
		return;
	dst[0] = '\0';
	if (dir == NULL || dir[0] == '\0' || filename == NULL)
		return;

	dir_len = strlen(dir);
	file_len = strlen(filename);
	needs_slash = (dir[dir_len - 1] != '/') ? 1 : 0;
	if (dir_len + needs_slash + file_len >= dst_size)
		return;

	memcpy(dst, dir, dir_len);
	if (needs_slash)
		dst[dir_len++] = '/';
	memcpy(dst + dir_len, filename, file_len + 1);
}

int bootext() {
	char default_start_dir[sizeof(inibuf)];
	char candidate_bios[sizeof(disksyspath)];
	int loaded_bios = 0;
	int i;

	set_default_config();
	memcpy(default_start_dir, inibuf, sizeof(default_start_dir));
	if (findpath(8,(char *[]){default_start_dir,"/_nds/","/_dstwoplug/","/ismartplug/","/moonshl2/extlink/","/_iMenu/_ini/","/_plugin_/","/"},"nesDS.ini")){
		// interrupt 1: read config
		long value;
		int render;

		if (get_ini_long("BASwap", 0, 0, 1)) joyflags |= B_A_SWAP;
		if (get_ini_long("LRDisable", 0, 0, 1)) joyflags |= L_R_DISABLE;
		value = get_ini_long("Blend", NOFLICKER, 0, 3);
		if (value != 2)
			__emuflags = (__emuflags & ~3) | value;
		if (get_ini_long("PALTiming", 0, 0, 1)) __emuflags |= PALTIMING;
		if (get_ini_long("FollowMem", 0, 0, 1)) __emuflags |= FOLLOWMEM;
		if (get_ini_long("ScreenSwap", 0, 0, 1)) __emuflags |= SCREENSWAP;
		if (get_ini_long("AllPixelOn", 0, 0, 1)) __emuflags |= ALLPIXELON;
		render = get_ini_long("Render", 0, 0, 2);
		__emuflags &= ~(SPLINE | SOFTRENDER);
		if (render == 1) __emuflags |= SPLINE;
		else if (render == 2) __emuflags |= SOFTRENDER;
		if (get_ini_long("AutoSRAM", 0, 0, 1)) __emuflags |= AUTOSRAM;
		ad_scale = get_ini_long("Screen_Scale", DEFAULT_SCREEN_SCALE,
			MIN_SCREEN_SCALE, MAX_SCREEN_SCALE);
		ad_ypos = get_ini_long("Screen_Offset", DEFAULT_SCREEN_OFFSET,
			-MAX_SCREEN_OFFSET, MAX_SCREEN_OFFSET);
		gammavalue = get_ini_long("Screen_Gamma", 0, 0, 4);
		palette_value = get_ini_long("Screen_Palette", 0, 0, 13);
		autofire_fps = get_ini_long("AutoFire", 2, 2, 30);
		use_saves_dir = get_ini_long("UseSavesDir", 0, 0, 1) != 0;

		for (i = 0 ; i < MAX_SC; i++) {
			shortcuts_tbl[i] = get_ini_shortcut(i);
			load_ini_gesture(i);
		}
		load_ini_path("StartIn", default_start_dir, inibuf, sizeof(inibuf));
		load_ini_path("DisksysBios", defaultDisksyspath, disksyspath, sizeof(disksyspath));
	}
	palset();
	__af_start = ((autofire_fps >> 1) << 8) + (autofire_fps >> 1) + (autofire_fps & 1);
	apply_display_settings();

	i = strlen(inibuf);
	if (i == 0 || (size_t)i >= sizeof(inibuf) - 1) {
		memcpy(inibuf, default_start_dir, sizeof(inibuf));
		i = strlen(inibuf);
	}
	while (i > 1 && inibuf[i - 1] == '/') {
		inibuf[--i] = '\0';
	}
	if (i == 0) {
		strcpy(inibuf, "/");
		i = 1;
	}
	if (chdir(inibuf) != 0) {
		const char *colon = strchr(default_start_dir, ':');
		int moved = 0;

		if (colon != NULL && inibuf[0] == '/') {
			size_t drive_len = (size_t)(colon - default_start_dir + 1);
			char drive_path[sizeof(inibuf)];

			if (drive_len + (size_t)i < sizeof(drive_path)) {
				memcpy(drive_path, default_start_dir, drive_len);
				memcpy(drive_path + drive_len, inibuf, (size_t)i + 1);
				if (chdir(drive_path) == 0) {
					strcpy(inibuf, drive_path);
					i = (int)strlen(inibuf);
					moved = 1;
				}
			}
		}
		if (!moved) {
			memcpy(inibuf, default_start_dir, sizeof(inibuf));
			i = strlen(inibuf);
			while (i > 1 && inibuf[i - 1] == '/') {
				inibuf[--i] = '\0';
			}
			if (i == 0) {
				strcpy(inibuf, "/");
				i = 1;
			}
			if (chdir(inibuf) != 0) {
				strcpy(inibuf, "/");
				i = 1;
				if (chdir(inibuf) != 0)
					return 0;
			}
		}
	}
	if (i > 1 && (size_t)i < sizeof(inibuf) - 1) {
		inibuf[i++] = '/';
		inibuf[i] = '\0';
	}

	// 0. If DisksysBios was explicitly customized in nesDS.ini, try it first
	if (disksyspath[0] != '\0' && strcmp(disksyspath, defaultDisksyspath) != 0) {
		loaded_bios = load_diskbios(disksyspath);
	}
	// 1. Search in nesDS.nds directory (app directory)
	if (!loaded_bios) {
		build_dir_filepath(candidate_bios, sizeof(candidate_bios), default_start_dir, "disksys.rom");
		if (candidate_bios[0] != '\0' && load_diskbios(candidate_bios)) {
			strcpy(disksyspath, candidate_bios);
			loaded_bios = 1;
		}
	}
	// 2. Search in ROM directory (StartIn from nesDS.ini)
	if (!loaded_bios && strcmp(inibuf, default_start_dir) != 0) {
		build_dir_filepath(candidate_bios, sizeof(candidate_bios), inibuf, "disksys.rom");
		if (candidate_bios[0] != '\0' && load_diskbios(candidate_bios)) {
			strcpy(disksyspath, candidate_bios);
			loaded_bios = 1;
		}
	}
	// 3. Search in root directory (/disksys.rom) last
	if (!loaded_bios) {
		if (load_diskbios(defaultDisksyspath)) {
			strcpy(disksyspath, defaultDisksyspath);
			loaded_bios = 1;
		}
	}
	if (loaded_bios)
		__emuflags |= DISKBIOS;

	// interrupt 2: read frontend
	if (!readFrontend(romfilename, sizeof(romfilename))) return 0;

	// interrupt 3: load rom
	if (loadrom() == -1) return 1;
	romfilename[0] = '\0';
	romfileext = NULL;
	return 0;
}

int load_gz(const char *fname)
{
#if 1
	return do_decompression(fname, tmpname);
#endif
	return 0;
}
