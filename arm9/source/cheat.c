#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"
#include "c_defs.h"

extern void enter_save_context(void);
extern void leave_save_context(void);
extern int active_interface;
extern char romfilename[];
extern char *romfileext;

extern int last_x, last_y;

extern int Sound_pause(void);
extern int Sound_resume(void);

#define CHEAT_LABEL_LEN 15

struct cheatlist_s
{
	unsigned int add;
	unsigned char val;
	unsigned char type;
};

struct cheatlist_s cheatlistdata[100];
char cheatlabels[100][CHEAT_LABEL_LEN + 1];

static void normalize_cheat_label(const char *src, char dst[CHEAT_LABEL_LEN + 1])
{
	int i = 0;
	if (src != NULL) {
		while (src[i] != '\0' && i < CHEAT_LABEL_LEN) {
			char c = src[i];
			if (c >= 'A' && c <= 'Z') {
				c = (char)(c - 'A' + 'a');
			} else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
				c = ' ';
			}
			dst[i] = c;
			i++;
		}
	}
	while (i < CHEAT_LABEL_LEN) {
		dst[i] = ' ';
		i++;
	}
	dst[CHEAT_LABEL_LEN] = '\0';
}

static int is_cheat_label_empty(const char label[CHEAT_LABEL_LEN + 1])
{
	int i;
	for (i = 0; i < CHEAT_LABEL_LEN; i++) {
		if (label[i] != ' ' && label[i] != '\0')
			return 0;
	}
	return 1;
}

int cheatcount = 0;//all codes below are for the cheat func.
char * searchtypechar[] = {"number      ", "equal     = ", "not equal !=", "increase  + ", "decrease  - ", "map       ? "};
touchstring cheattouch1[] = {
{64 * 4, "searchtype"},
{64 * 5 + 26, "\x82"}, {64 * 5 + 30, "\x82"}, 
{64 * 7 + 26, "\x83"}, {64 * 7 + 30, "\x83"},
{64 * 8, "start"},
{64 * 8 + 40, "new search"},
{-1, 0}
};

touchstring cheataddtouch[] = {
{64 * 11 - 8, "+"},
{64 * 12 - 4, "+"},
{64 * 13 - 8, "+"},
{64 * 14 - 4, "+"},
{64 * 15 - 8, "+"},
{64 * 16 - 4, "+"},
{64 * 17 - 8, "+"},
{64 * 18 - 4, "+"},
{64 * 19 - 8, "+"},
{64 * 20 - 4, "+"},
{64 * 21 - 8, "+"},
{64 * 22 - 4, "+"},
{-1, 0}
};

void cheat_reset(void)
{
	int i;
	cheatcount = 0;
	memset(cheatlistdata, 0, sizeof(cheatlistdata));
	for (i = 0; i < 100; i++) {
		normalize_cheat_label(NULL, cheatlabels[i]);
	}
}

//load a cheat file (supports both human-readable text format and legacy 1024-byte binary).
void load_cheat(void)
{
	FILE *f;
	char buf[1025];
	int len;
	int is_legacy_binary = 0;
	int i;

	if(!active_interface) return;

	(void)Sound_pause();
	enter_save_context();
	romfileext[0]='c';
	romfileext[1]='h';
	romfileext[2]='t';
	romfileext[3]=0;
	f=fopen(romfilename,"rb");
	if(!f) {
		leave_save_context();
		(void)Sound_resume();
		return;
	}
	len=fread(buf,1,sizeof(buf) - 1,f);
	fclose(f);
	leave_save_context();
	(void)Sound_resume();

	if (len <= 0)
		return;

	// Detect legacy 1024-byte binary cheat format (count in first 4 bytes, 0..14)
	if (len >= 1023) {
		int bin_count = 0;
		memcpy(&bin_count, buf, 4);
		if (bin_count >= 0 && bin_count <= 14 && (buf[2] == 0 && buf[3] == 0)) {
			is_legacy_binary = 1;
			cheatcount = bin_count;
			memset(cheatlistdata, 0, sizeof(cheatlistdata));
			for (i = 0; i < 100; i++) {
				normalize_cheat_label(NULL, cheatlabels[i]);
			}
			if (cheatcount > 0)
				memcpy(cheatlistdata, buf + 4, (size_t)cheatcount * sizeof(cheatlistdata[0]));
		}
	}

	if (!is_legacy_binary) {
		char *line;
		char *next_line;

		buf[len] = '\0';
		cheatcount = 0;
		memset(cheatlistdata, 0, sizeof(cheatlistdata));
		for (i = 0; i < 100; i++) {
			normalize_cheat_label(NULL, cheatlabels[i]);
		}

		line = buf;
		while (line != NULL && *line != '\0' && cheatcount < 14) {
			unsigned int addr = 0;
			unsigned int val = 0;
			char *p;
			char *sep1;
			char *sep2;
			char *label_ptr = NULL;
			unsigned char c_type = 0;
			unsigned char c_dis = 0;

			next_line = strpbrk(line, "\r\n");
			if (next_line != NULL) {
				*next_line = '\0';
				next_line++;
				while (*next_line == '\r' || *next_line == '\n')
					next_line++;
			}

			p = line;
			while (*p == ' ' || *p == '\t')
				p++;

			if (*p != '\0' && *p != '#' && *p != ';') {
				sep1 = strchr(p, ':');
				if (sep1 != NULL) {
					*sep1 = '\0';
					addr = (unsigned int)strtoul(p, NULL, 16) & 0x7FF;
					p = sep1 + 1;

					sep2 = strchr(p, ':');
					if (sep2 != NULL)
						*sep2 = '\0';
					val = (unsigned int)strtoul(p, NULL, 16) & 0xFF;

					if (sep2 != NULL) {
						char *opt = sep2 + 1;
						char *sep3 = strchr(opt, ':');
						if (sep3 != NULL) {
							char *state_str = sep3 + 1;
							char *sep4;
							*sep3 = '\0';
							sep4 = strchr(state_str, ':');
							if (sep4 != NULL) {
								*sep4 = '\0';
								label_ptr = sep4 + 1;
							}
							if (strstr(state_str, "off") || strstr(state_str, "OFF") || strstr(state_str, "0"))
								c_dis = 0x80;
						}
						if (strstr(opt, "once") || strstr(opt, "ONCE") || opt[0] == '1' || opt[0] == '2')
							c_type = (opt[0] == '2') ? 2 : 1;
						if (strstr(opt, "off") || strstr(opt, "OFF"))
							c_dis = 0x80;
					}

					cheatlistdata[cheatcount].add = addr;
					cheatlistdata[cheatcount].val = (unsigned char)val;
					cheatlistdata[cheatcount].type = c_type | c_dis;
					normalize_cheat_label(label_ptr, cheatlabels[cheatcount]);
					cheatcount++;
				}
			}
			line = next_line;
		}
	}
}

//save to a cheat file in human-readable text format.
void save_cheat(void)
{
	FILE *f;
	int i;

	if(!active_interface) return;

	(void)Sound_pause();
	enter_save_context();
	romfileext[0]='c';
	romfileext[1]='h';
	romfileext[2]='t';
	romfileext[3]=0;
	f=fopen(romfilename,"w");
	if(!f) {
		leave_save_context();
		(void)Sound_resume();
		return;
	}
	fprintf(f, "# nesDS Cheat File (ADDR:VAL:TYPE:STATE:LABEL)\n");
	for (i = 0; i < cheatcount && i < 14; i++) {
		unsigned char raw_type = cheatlistdata[i].type;
		const char *type_str = ((raw_type & ~0x80) == 0) ? "fix" : "once";
		const char *state_str = (raw_type & 0x80) ? "off" : "on";
		if (is_cheat_label_empty(cheatlabels[i])) {
			fprintf(f, "%03X:%02X:%s:%s:\n",
				cheatlistdata[i].add & 0x7FF,
				cheatlistdata[i].val & 0xFF,
				type_str,
				state_str);
		} else {
			fprintf(f, "%03X:%02X:%s:%s:%s\n",
				cheatlistdata[i].add & 0x7FF,
				cheatlistdata[i].val & 0xFF,
				type_str,
				state_str,
				cheatlabels[i]);
		}
	}
	fflush(f);
	fclose(f);
	leave_save_context();
	(void)Sound_resume();
}

// add a cheat list to the Chtlist menu....
void addcheatlist(int add)
{
	if(cheatcount >= 14)
		return;
	cheatlistdata[cheatcount].add = add;
	cheatlistdata[cheatcount].val = *(char *)(NES_RAM + add);
	cheatlistdata[cheatcount].type = 2;
	normalize_cheat_label(NULL, cheatlabels[cheatcount]);
	cheatcount++;
}


unsigned char lastcheatdata[0x800];
unsigned char cheatdata[0x800];
char lastcheatstatus[0x800];
//do cheat search or something else
int addcheat()
{
	static int searchnum = 0;
	static int searchtype = 0;
	static int count = 0;
	static char newsearch = 1;
	char *p;
	int i;
	p = (char *)NES_RAM;//NDS_RAM
	consoletext(64 * 4, "searchtype: ", 0);
	consoletext(64 * 4 + 24, searchtypechar[searchtype], 0);
	consoletext(64 * 6, "searchnum: 0x", 0);
	hex(64 * 6 + 26, searchnum >> 4, 0);
	hex(64 * 6 + 30, searchnum, 0);
	i = do_touchstrings(cheattouch1, 0);
	if(touchstate==4) {
		switch(i)  {
			case 0:
				searchtype++;
				if(searchtype > 5)
					searchtype = 0;
				break;
			case 1:
				searchnum += 0x10;
				break;
			case 2:
				searchnum += 0x01;
				break;
			case 3:
				searchnum -= 0x10;
				break;
			case 4:
				searchnum -= 0x01;
				break;
			case 6:
				newsearch = 1;
			case 5:
				{
					count = 0;
					for(i = 10; i < 22; i++)  {
						consoletext(64 * i, "                                ", 0);
					}
					for(i = 0; i < 0x800; i++)  {
						if(lastcheatstatus[i] || searchtype == 5)  {
							lastcheatstatus[i] = 0;
							switch(searchtype)  {
								case 0:
									if(*p == searchnum)
										lastcheatstatus[i] = 1;
									break;
								case 1:
									if(*p == lastcheatdata[i])
										lastcheatstatus[i] = 1;
									break;
								case 2:
									if(*p != lastcheatdata[i])
										lastcheatstatus[i] = 1;
									break;
								case 3:
									if(*p > lastcheatdata[i])
										lastcheatstatus[i] = 1;
									break;
								case 4:
									if(*p < lastcheatdata[i])
										lastcheatstatus[i] = 1;
									break;
								case 5:
									lastcheatstatus[i] = 1;
									break;
							}
						}
						if(newsearch == 1)
							lastcheatstatus[i] = 1;
						if(lastcheatstatus[i]) {
							count++;
							lastcheatdata[i] = cheatdata[i];
							cheatdata[i] = *p;
						}
						p++;
					}
					newsearch = 0;
				}
				break;
			default:
				break;
		}
		searchnum = searchnum&0xff;
	}
	consoletext(64 * 9, "found:0x", 0);
	hex(64 * 9 + 16, count, 3);
	if(count > 0)  {
		count = 0;
		for(i = 0; i < 0x800; i++)  {
			if(lastcheatstatus[i])  {
				if (count < 11)  {
					consoletext(64 * 10 + 64 * count, "add:0x    old:0x   new:0x  ", 0);
					hex(64 * 10 + 64 * count + 12, i, 2);
					hex(64 * 10 + 64 * count + 32, lastcheatdata[i], 1);
					hex(64 * 10 + 64 * count + 50, cheatdata[i], 1);
				}
				count++;
			}
		}
	}
	if(count > 0)  {
		int showcount;
		showcount = count > 10? 11:count;
		cheataddtouch[showcount].offset = -1;
		i = do_touchstrings(cheataddtouch, 0);
		cheataddtouch[showcount].offset = 64 * (11 + showcount) - ((showcount&1)?4:8);
		if(touchstate==4) {
			if(i >= 0)  {
				int j;
				count = 0;
				for(j = 0; j < 0x800; j++)  {
					if(lastcheatstatus[j] == 1) count++;
					if(count == i + 1)  {
						break;
					}
				}
				addcheatlist(j);
			}
		}
	}
	return 0;
}

touchstring cheatlisttouch[] = {
{0, "E"},
{-1,0}
};


#define CHEAT_DISABLED 0x80

static char step_label_char(char c, int dir)
{
	int idx = 0;
	if (c >= '0' && c <= '9')
		idx = 1 + (c - '0');
	else if (c >= 'a' && c <= 'z')
		idx = 11 + (c - 'a');

	idx += dir;
	if (idx < 0)
		idx = 36;
	else if (idx > 36)
		idx = 0;

	if (idx == 0)
		return ' ';
	if (idx <= 10)
		return (char)('0' + (idx - 1));
	return (char)('a' + (idx - 11));
}

int cheatlist()
{
	char *type[] = {"fix", "once", "once"};
	int i;
	static char state;
	static char edit_mode;
	static unsigned char editnum;
	static unsigned int preadd;
	static unsigned char preval, pretype, predis;
	static char prelabel[CHEAT_LABEL_LEN + 1];

	if (menu_stat == 5) {
		for (i = 4; i <= 24; i++) {
			consoletext(64 * i, "                                ", 0);
		}
		menu_stat = 6;
	}

	for (i = 4; i < cheatcount + 4; i++) {
		int idx = i - 4;
		unsigned char raw_type = cheatlistdata[idx].type;
		unsigned char is_off = (raw_type & CHEAT_DISABLED) ? 1 : 0;
		unsigned char base_type = raw_type & ~CHEAT_DISABLED;
		int color = is_off ? 0x2000 : 0;

		if (base_type > 2)
			base_type = 0;

		if (!is_cheat_label_empty(cheatlabels[idx])) {
			consoletext(64 * i, "                   type:        ", color);
			consoletext(64 * i, cheatlabels[idx], color);
			consoletext(64 * i + 48, type[base_type], color);
		} else {
			consoletext(64 * i, "add:0x    val:0x   type:        ", color);
			hex(64 * i + 12, cheatlistdata[idx].add, 2);
			hex(64 * i + 12 + 20, cheatlistdata[idx].val, 1);
			consoletext(64 * i + 30 + 8 + 10, type[base_type], color);
		}
		consoletext(64 * i + 56 + (i & 1 ? 4 : 0), cheatlisttouch[0].str, 0);

		{
			u16 *map = (u16 *)SUB_BG;
			int col;
			int row_base = 32 * i;
			for (col = 0; col < 28; col++) {
				map[row_base + col] = (map[row_base + col] & 0x0FFF) | color;
			}
		}
	}

	if (touchstate == 4 && cheatcount > 0) {
		int touched_row = last_y / 8;
		if (touched_row >= 4 && touched_row < cheatcount + 4) {
			int target = touched_row - 4;
			if (last_x >= 220) {
				unsigned char raw_type = cheatlistdata[target].type;
				state = 1;
				edit_mode = 0;
				editnum = (unsigned char)target;
				preadd = cheatlistdata[target].add;
				preval = cheatlistdata[target].val;
				predis = (raw_type & CHEAT_DISABLED) ? CHEAT_DISABLED : 0;
				pretype = raw_type & ~CHEAT_DISABLED;
				if (pretype > 2)
					pretype = 0;
				normalize_cheat_label(cheatlabels[target], prelabel);
				menu_stat = 5;
			} else {
				cheatlistdata[target].type ^= CHEAT_DISABLED;
				menu_stat = 5;
			}
		}
	}

	if (state == 1) {
		if (edit_mode == 0) {
			touchstring mytouch[] = {
				{64 * 19 + 4, "\x82"},
				{64 * 19 + 8, "\x82"},
				{64 * 19 + 12, "\x82"},
				{64 * 19 + 20, "\x82"},
				{64 * 19 + 24, "\x82"},
				{64 * 21 + 4, "\x83"},
				{64 * 21 + 8, "\x83"},
				{64 * 21 + 12, "\x83"},
				{64 * 21 + 20, "\x83"},
				{64 * 21 + 24, "\x83"},
				{64 * 20 + 32, "type"},
				{64 * 20 + 52, "ok"},
				{64 * 20 + 58, "del"},
				{64 * 21 + 42, "V/L"},
				{-1, 0}
			};
			consoletext(64 * 20, "0x      0x          :           ", 0);
			hex(64 * 20 + 4, preadd >> 8, 0);
			hex(64 * 20 + 8, preadd >> 4, 0);
			hex(64 * 20 + 12, preadd, 0);
			hex(64 * 20 + 20, preval >> 4, 0);
			hex(64 * 20 + 24, preval, 0);
			consoletext(64 * 20 + 42, type[pretype], 0);
			i = do_touchstrings(mytouch, 0);
			if (touchstate == 4) {
				if (i >= 0 && i <= 13)
					menu_stat = 5;
				switch (i) {
					case 0:
						preadd += 0x100;
						break;
					case 1:
						preadd += 0x010;
						break;
					case 2:
						preadd += 0x001;
						break;
					case 3:
						preval += 0x10;
						break;
					case 4:
						preval += 0x01;
						break;
					case 5:
						preadd -= 0x100;
						break;
					case 6:
						preadd -= 0x010;
						break;
					case 7:
						preadd -= 0x001;
						break;
					case 8:
						preval -= 0x10;
						break;
					case 9:
						preval -= 0x01;
						break;
					case 10:
						pretype = pretype ? 0 : 1;
						break;
					case 11:
						cheatlistdata[editnum].add = preadd;
						cheatlistdata[editnum].val = preval;
						cheatlistdata[editnum].type = (pretype ? 1 : 0) | predis;
						normalize_cheat_label(prelabel, cheatlabels[editnum]);
						state = 0;
						break;
					case 12:
						{
							int k;
							if (cheatcount > 0 && editnum < cheatcount) {
								for (k = editnum; k < cheatcount - 1; k++) {
									cheatlistdata[k].add = cheatlistdata[k + 1].add;
									cheatlistdata[k].val = cheatlistdata[k + 1].val;
									cheatlistdata[k].type = cheatlistdata[k + 1].type;
									memcpy(cheatlabels[k], cheatlabels[k + 1], CHEAT_LABEL_LEN + 1);
								}
								cheatlistdata[cheatcount - 1].add = 0;
								cheatlistdata[cheatcount - 1].val = 0;
								cheatlistdata[cheatcount - 1].type = 0;
								normalize_cheat_label(NULL, cheatlabels[cheatcount - 1]);
								cheatcount--;
							}
							state = 0;
						}
						break;
					case 13:
						edit_mode = 1;
						break;
				}
				preval &= 0xFF;
				preadd &= 0x7FF;
			}
		} else {
			static int cursor_pos = 0;
			touchstring labeltouch[] = {
				{64 * 19 + 36, "\x82"},
				{64 * 21 + 36, "\x83"},
				{64 * 20 + 52, "ok"},
				{64 * 20 + 58, "del"},
				{64 * 21 + 42, "V/L"},
				{-1, 0}
			};
			if (cursor_pos < 0 || cursor_pos >= CHEAT_LABEL_LEN)
				cursor_pos = 0;

			consoletext(64 * 20, "                                ", 0);
			consoletext(64 * 20, prelabel, 0);
			// Draw 15-char underline guide on row 21 (cols 0..14)
			consoletext(64 * 21, "---------------", 0x2000);
			// Highlight currently selected character and its underline marker in yellow (0x1000)
			{
				u16 *map = (u16 *)SUB_BG;
				int row_text = 32 * 20;
				int row_line = 32 * 21;
				map[row_text + cursor_pos] = (map[row_text + cursor_pos] & 0x0FFF) | 0x1000;
				map[row_line + cursor_pos] = (map[row_line + cursor_pos] & 0x0FFF) | 0x1000;
			}

			i = do_touchstrings(labeltouch, 0);
			if (touchstate == 4) {
				if (i >= 0 && i <= 4) {
					menu_stat = 5;
					switch (i) {
						case 0: // Up arrow
							prelabel[cursor_pos] = step_label_char(prelabel[cursor_pos], 1);
							break;
						case 1: // Down arrow
							prelabel[cursor_pos] = step_label_char(prelabel[cursor_pos], -1);
							break;
						case 2: // ok
							cheatlistdata[editnum].add = preadd;
							cheatlistdata[editnum].val = preval;
							cheatlistdata[editnum].type = (pretype ? 1 : 0) | predis;
							normalize_cheat_label(prelabel, cheatlabels[editnum]);
							cursor_pos = 0;
							state = 0;
							break;
						case 3: // del
							{
								int k;
								if (cheatcount > 0 && editnum < cheatcount) {
									for (k = editnum; k < cheatcount - 1; k++) {
										cheatlistdata[k].add = cheatlistdata[k + 1].add;
										cheatlistdata[k].val = cheatlistdata[k + 1].val;
										cheatlistdata[k].type = cheatlistdata[k + 1].type;
										memcpy(cheatlabels[k], cheatlabels[k + 1], CHEAT_LABEL_LEN + 1);
									}
									cheatlistdata[cheatcount - 1].add = 0;
									cheatlistdata[cheatcount - 1].val = 0;
									cheatlistdata[cheatcount - 1].type = 0;
									normalize_cheat_label(NULL, cheatlabels[cheatcount - 1]);
									cheatcount--;
								}
								cursor_pos = 0;
								state = 0;
							}
							break;
						case 4: // V/L
							edit_mode = 0;
							break;
					}
				} else {
					// Touch directly on any of the 15 label characters (row 19..21, cols 0..14) to move cursor
					int trow = last_y / 8;
					int tcol = last_x / 8;
					if (trow >= 19 && trow <= 21 && tcol >= 0 && tcol < CHEAT_LABEL_LEN) {
						cursor_pos = tcol;
						menu_stat = 5;
					}
				}
			}
		}
	} else {
		touchstring mytouch[] = {
			{64 * 20 + 40, "add new one"},
			{-1, 0},
		};
		i = do_touchstrings(mytouch, 0);
		if (touchstate == 4) {
			switch (i) {
				case 0:
					if (cheatcount < 14) {
						cheatlistdata[cheatcount].add = 0;
						cheatlistdata[cheatcount].val = 0;
						cheatlistdata[cheatcount].type = 2 | CHEAT_DISABLED;
						normalize_cheat_label(NULL, cheatlabels[cheatcount]);
						cheatcount++;
					}
					menu_stat = 5;
					break;
			}
		}
	}
	return 0;
}

int do_cheat()
{
	int i;
	if(cheatcount)  {
		for(i = 0; i < cheatcount; i++)  {
			if (cheatlistdata[i].type & CHEAT_DISABLED)
				continue;
			if(cheatlistdata[i].type == 0)  {
				*(char *)(NES_RAM + cheatlistdata[i].add) = cheatlistdata[i].val;
			}
			else if(cheatlistdata[i].type == 1)  {
				*(char *)(NES_RAM + cheatlistdata[i].add) = cheatlistdata[i].val;
				cheatlistdata[i].type = 2;
			}
		}
	}
	return 0;
}