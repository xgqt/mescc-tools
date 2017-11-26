/* -*- c-file-style: "linux";indent-tabs-mode:t -*- */
/* Copyright (C) 2016 Jeremiah Orians
 * Copyright (C) 2017 Jan Nieuwenhuizen <janneke@gnu.org>
 * This file is part of stage0.
 *
 * stage0 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * stage0 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with stage0.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#define max_string 4096
#define MACRO 1
#define STR 2
#define TRUE 1
#define FALSE 0

#if __MESC__
#include <fcntl.h>
#define strnlen(string, int) strlen(string)
#endif

FILE* source_file;
FILE* destination_file;
int Reached_EOF;
int BigEndian;
int Architecture;

struct Token
{
	struct Token* next;
	int type;
	char* Text;
	char* Expression;
};

struct Token* newToken()
{
	struct Token* p;

	p = calloc (1, sizeof (struct Token));
	if (NULL == p)
	{
		fprintf (stderr, "calloc failed.\n");
		exit (EXIT_FAILURE);
	}

	return p;
}

struct Token* reverse_list(struct Token* head)
{
	struct Token* root = NULL;
	while(NULL != head)
	{
		struct Token* next = head->next;
		head->next = root;
		root = head;
		head = next;
	}
	return root;
}

void purge_lineComment()
{
	int c = fgetc(source_file);
	while((10 != c) && (13 != c))
	{
		c = fgetc(source_file);
	}
}

char* store_atom(char c)
{
	char* store = calloc(max_string + 1, sizeof(char));
	if(NULL == store)
	{
		fprintf(stderr, "Exhusted available memory\n");
		exit(EXIT_FAILURE);
	}
	int ch;
	int i = 0;
	ch = c;
	do
	{
		store[i] = (char)ch;
		ch = fgetc(source_file);
		i = i + 1;
	} while ((9 != ch) && (10 != ch) && (32 != ch) && (i <= max_string));

	return store;
}

char* store_string(char c)
{
	char* store = calloc(max_string + 1, sizeof(char));
	if(NULL == store)
	{
		fprintf(stderr, "Exhusted available memory\n");
		exit(EXIT_FAILURE);
	}
	int ch;
	int i = 0;
	ch = c;
	do
	{
		store[i] = (char)ch;
		i = i + 1;
		ch = fgetc(source_file);
		if(-1 == ch)
		{
			fprintf(stderr, "Unmatched \"!\n");
			exit(EXIT_FAILURE);
		}
		if(max_string == i)
		{
			fprintf(stderr, "String: %s exceeds max string size\n", store);
			exit(EXIT_FAILURE);
		}
	} while(ch != c);

	return store;
}

struct Token* Tokenize_Line(struct Token* head)
{
	int c;

restart:
	c = fgetc(source_file);

	if((35 == c) || (59 == c))
	{
		purge_lineComment();
		goto restart;
	}

	if((9 == c) || (10 == c) || (32 == c))
	{
		goto restart;
	}

	struct Token* p = newToken();
	if(-1 == c)
	{
		Reached_EOF = TRUE;
		free(p);
		return head;
	}
	else if((34 == c) || (39 == c))
	{
		p->Text = store_string(c);
		p->type = STR;
	}
	else
	{
		p->Text = store_atom(c);
	}

	p->next = head;
	return p;
}

void setExpression(struct Token* p, char *match, char *Exp)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		/* Leave macros alone */
		if((i->type & MACRO))
		{
			continue;
		}
		else if(0 == strncmp(i->Text, match, max_string))
		{ /* Only if there is an exact match replace */
			i->Expression = Exp;
		}
	}
}

void identify_macros(struct Token* p)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		if(0 == strncmp(i->Text, "DEFINE", max_string))
		{
			i->type = MACRO;
			i->Text = i->next->Text;
			if(i->next->next->type & STR)
			{
				i->Expression = i->next->next->Text + 1;
			}
			else
			{
				i->Expression = i->next->next->Text;
			}
			i->next = i->next->next->next;
		}
	}
}

void line_macro(struct Token* p)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		if(i->type & MACRO)
		{
			setExpression(i->next, i->Text, i->Expression);
		}
	}
}

void hexify_string(struct Token* p)
{
	char table[16] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
	int i = ((strnlen(p->Text + 1 , max_string)/4) + 1) * 8;

	char* d = calloc(max_string, sizeof(char));
	p->Expression = d;

	while(0 < i)
	{
		i = i - 1;
		d[i] = 0x30;
	}

	while( i < max_string)
	{
		if(0 == p->Text[i+1])
		{
			i = max_string;
		}
		else
		{
			d[2*i]  = table[p->Text[i+1] / 16];
			d[2*i + 1] = table[p->Text[i+1] % 16];
			i = i + 1;
		}
	}
}

void process_string(struct Token* p)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		if(i->type & STR)
		{
			if('\'' == i->Text[0])
			{
				i->Expression = i->Text + 1;
			}
			else if('"' == i->Text[0])
			{
				hexify_string(i);
			}
		}
	}
}


void preserve_other(struct Token* p)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		if((NULL == i->Expression) && !(i->type & MACRO))
		{
			i->Expression = i->Text;
		}
	}
}

void range_check(int displacement, int number_of_bytes)
{
	switch(number_of_bytes)
	{
		case 4: break;
		case 3:
		{
			if((8388607 < displacement) || (displacement < -8388608))
			{
				fprintf(stderr, "A displacement of %d does not fit in 3 bytes", displacement);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 2:
		{
			if((32767 < displacement) || (displacement < -32768))
			{
				fprintf(stderr, "A displacement of %d does not fit in 2 bytes", displacement);
				exit(EXIT_FAILURE);
			}
			break;
		}
		case 1:
		{
			if((127 < displacement) || (displacement < -128))
			{
				fprintf(stderr, "A displacement of %d does not fit in 1 byte", displacement);
				exit(EXIT_FAILURE);
			}
			break;
		}
		default: exit(EXIT_FAILURE);
	}
}

int hex(int c)
{
	if (c >= '0' && c <= '9') return (c - 48);
	else if (c >= 'a' && c <= 'z') return (c - 87);
	else if (c >= 'A' && c <= 'Z') return (c - 55);
	else return -1;
}

int decimal(int c)
{
	if (c >= '0' && c <= '9') return (c - 48);
	else return -1;
}

int numerate_string(char *a)
{
	int count = 0;
	int index;

	/* If NULL string */
	if(0 == a[0])
	{
		return 0;
	}
	/* Deal with hex */
	else if (a[0] == '0' && a[1] == 'x')
	{
		index = 2;
		while(0 != a[index])
		{
			if(-1 == hex(a[index])) return 0;
			count = (16 * count) + hex(a[index]);
			index = index + 1;
		}
		return count;
	}
	/* Deal with decimal */
	else
	{
		int negative;
		if('-' == a[0])
		{
			negative = TRUE;
			index = 1;
		}
		else
		{
			negative = FALSE;
			index = 0;
		}

		while(0 != a[index])
		{
			if(-1 == decimal(a[index])) return 0;
			count = (10 * count) + decimal(a[index]);
			index = index + 1;
		}
		if(negative)
		{
			count = count * -1;
		}
		return count;
	}
}

char* LittleEndian(uint value, char* c, int Number_of_bytes)
{
	/* {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'} */
	char table[16] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};

	switch(Number_of_bytes)
	{
		case 4:
		{
			c[6] = table[value >> 28];
			c[7] = table[(value >> 24)% 16];
		}
		case 3:
		{
			c[4] = table[(value >> 20)% 16];
			c[5] = table[(value >> 16)% 16];
		}
		case 2:
		{
			c[2] = table[(value >> 12)% 16];
			c[3] = table[(value >> 8)% 16];
		}
		case 1:
		{
			c[0] = table[(value >> 4)% 16];
			c[1] = table[value % 16];
			break;
		}
		default:
		{
			fprintf(stderr, "Recieved invalid number of bytes in LittleEndian %d\n", Number_of_bytes);
			exit(EXIT_FAILURE);
		}
	}
	return c;
}

char* express_number(int value, char c)
{
	char* ch;
	if('!' == c)
	{
		range_check(value, 1);
		ch = calloc(3, sizeof(char));
		if(BigEndian)
		{
			sprintf(ch, "%02x", value);
		}
		else
		{
			ch = LittleEndian(value, ch, 1);
		}
	}
	else if('@' == c)
	{
		range_check(value, 2);
		ch = calloc(5, sizeof(char));
		if(BigEndian)
		{
			sprintf(ch, "%04x", value);
		}
		else
		{
			ch = LittleEndian(value, ch, 2);
		}
	}
	else if('%' == c)
	{
		range_check(value, 4);
		ch = calloc(9, sizeof(char));
		if(BigEndian)
		{
			sprintf(ch, "%08x", value);
		}
		else
		{
			ch = LittleEndian(value, ch, 4);
		}
	}
	else
	{
		fprintf(stderr, "Given symbol %c to express immediate value %d\n", c, value);
		exit(EXIT_FAILURE);
	}
	return ch;
}

void eval_immediates(struct Token* p)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		if((NULL == i->Expression) && !(i->type & MACRO))
		{
			int value;
			switch(Architecture)
			{
				case 2:
				case 1:
				{
					value = numerate_string(i->Text + 1);
					if(('0' == i->Text[1]) || (0 != value))
					{
						i->Expression = express_number(value, i->Text[0]);
					}
					break;
				}
				case 0:
				{
					value = numerate_string(i->Text);
					if(('0' == i->Text[0]) || (0 != value))
					{
						range_check(value, 2);
						i->Expression = calloc(5, sizeof(char));
						sprintf(i->Expression, "%04X", value);
					}
					break;
			}
		}
	}
	}
}

void print_hex(struct Token* p)
{
	for(struct Token* i = p; NULL != i; i = i->next)
	{
		if(i->type ^ MACRO)
		{
			fprintf(destination_file, "\n%s", i->Expression);
		}
	}

	fprintf(destination_file, "\n");
}

#if !__MESC__
static
#endif
struct option long_options[] = {
	{"Architecture", required_argument, 0, 'A'},
	{"BigEndian", no_argument, &BigEndian, TRUE},
	{"LittleEndian", no_argument, &BigEndian, FALSE},
	{"file", required_argument, 0, 'f'},
	{"output", required_argument, 0, 'o'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{0, 0, 0, 0}
};

/* Standard C main program */
int main(int argc, char **argv)
{
/* Default endianness is that of the native host */
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
	defined(__BIG_ENDIAN__) || \
	defined(__ARMEB__) || \
	defined(__THUMBEB__) || \
	defined(__AARCH64EB__) || \
	defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
// It's a big-endian target architecture
		BigEndian = true;
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
	defined(__LITTLE_ENDIAN__) || \
	defined(__ARMEL__) || \
	defined(__THUMBEL__) || \
	defined(__AARCH64EL__) || \
	defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
// It's a little-endian target architecture
	BigEndian = FALSE;
#else
#error "I don't know what architecture this is!"
	exit(EXIT_FAILURE);
#endif

	struct Token* head = NULL;
	Architecture = 0;
	destination_file = stdout;
	int c;

	int option_index = 0;
	while ((c = getopt_long(argc, argv, "f:h:o:V", long_options, &option_index)) != -1)
	{
		switch(c)
		{
			case 0: break;
			case 'A':
			{
				Architecture = atoi(optarg);
				break;
			}
			case 'h':
			{
				fprintf(stderr, "Usage: %s -f FILENAME1 {-f FILENAME2} (--BigEndian|--LittleEndian) [--BaseAddress 12345] [--Architecture 12345]\n", argv[0]);
				fprintf(stderr, "Architecture 0: Knight; 1: x86; 2: AMD64");
				exit(EXIT_SUCCESS);
			}
			case 'f':
			{
#if __MESC__
				source_file = open(optarg, O_RDONLY);
#else
				source_file = fopen(optarg, "r");
#endif
				Reached_EOF = FALSE;
				while(!Reached_EOF)
				{
					head = Tokenize_Line(head);
				}
				break;
			}
			case 'o':
			{
				destination_file = fopen(optarg, "w");
				break;
			}
			case 'V':
			{
				fprintf(stdout, "M1 0.3\n");
				exit(EXIT_SUCCESS);
			}
			default:
			{
				fprintf(stderr, "Unknown option\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	if(NULL == head)
	{
		fprintf(stderr, "Either no input files were given or they were empty\n");
		exit(EXIT_FAILURE);
	}

	head = reverse_list(head);
	identify_macros(head);
	line_macro(head);
	process_string(head);
	eval_immediates(head);
	preserve_other(head);
	print_hex(head);

	return EXIT_SUCCESS;
}
