#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern int argc;
extern char **argv;
bool readFrontend(char *target, int target_size)
{
	char dir[768];
	const char *fullpath;
	const char *filename;
	const char *slash;
	size_t dir_length;
	size_t filename_length;

	if (target == NULL || target_size <= 0)
		return false;
	target[0] = '\0';
	if (argc < 2 || argv == NULL || argv[1] == NULL)
		return false;

	fullpath = argv[1];
	slash = strrchr(fullpath, '/');
	if (slash != NULL) {
		dir_length = slash == fullpath ? 1 : (size_t)(slash - fullpath);
		filename = slash + 1;
		if (dir_length >= sizeof(dir))
			return false;
		memcpy(dir, fullpath, dir_length);
		dir[dir_length] = '\0';
	} else {
		dir[0] = '/';
		dir[1] = '\0';
		dir_length = 1;
		filename = fullpath;
	}

	filename_length = strlen(filename);
	if (filename_length == 0 || filename_length >= (size_t)target_size)
		return false;
	memcpy(target, filename, filename_length + 1);
	if (chdir(dir) != 0) {
		target[0] = '\0';
		return false;
	}
	return true;
}

