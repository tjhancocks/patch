/**
 Copyright (c) 2019 Tom Hancocks

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

/**
 patch tool
 ----------
 This is a simple utility designed for replacing binary data inside a file. For
 instance, the following would insert the string "Hello, World!" into the file
 "build/disk.img" at offset 512:

 	./patch -f build/disk.img -a 512 -t str -d "Hello, World!"

 This however would insert the number 2 as a short/word into the file at offset
 544:

 	.patch -f build/disk.img -a 544 -t dw -d 2

 */

enum data_type { 
	type_db = 1, type_dw = 2, type_dd = 4, type_dq = 8, type_str = 0 
};

static inline enum data_type data_type_for(char *arg)
{
	if (strcmp(arg, "dw") == 0)
		return type_dw;
	else if (strcmp(arg, "dd") == 0)
		return type_dd;
	else if (strcmp(arg, "dq") == 0)
		return type_dq;
	else if (strcmp(arg, "str") == 0)
		return type_str;
	else
		return type_db;
}

static inline uint64_t integer_for(char *arg)
{
	uint64_t value = strtoll(arg, NULL, 10);
	return value;
}

const char *resolve_path(char *path)
{
	// Perform expansion on the the path.
    wordexp_t exp_result;
    wordexp(path, &exp_result, 0);

    unsigned long len = strlen(exp_result.we_wordv[0]);
    char *result = calloc(len + 1, sizeof(*result));
    memcpy(result, exp_result.we_wordv[0], len);

    wordfree(&exp_result);

    return result;
}

const char *copystr(char *str)
{
    unsigned long len = strlen(str);
    char *result = calloc(len + 1, sizeof(*result));
    char *r = result;

    while (*str) {
    	if (*str == '\\' && *(str + 1) == 'r') {
    		*r++ = '\r';
    		str++;
    	}
    	else if (*str == '\\' && *(str + 1) == 'n') {
    		*r++ = '\n';
    		str++;
    	}
    	else {
    		*r++ = *str;
    	}
    	str++;
    }

    return result;
}

int main(int argc, char const *argv[])
{
	const char *file = NULL;
	uint64_t offset = 0;
	enum data_type type = type_db;
	uint64_t length = 1;
	uint8_t pad_value = 0;
	const char *data_in = NULL;

	/* Scan through all command line arguments supplied. */
	int c = 0;
	while ((c = getopt(argc, (char **)argv, "f:a:t:l:p:vd:")) != -1) {
		switch (c) {
			case 'f': /* binary file to work upon */
				file = resolve_path(optarg);
				break;
			case 'a': /* offset to work from */
				offset = integer_for(optarg);
				break;
			case 't': /* type of data to insert */
				type = data_type_for(optarg);
				break;
			case 'l': /* length of data to insert. truncate or pad to it. */
				length = integer_for(optarg);
				break;
			case 'p': /* value to pad with */
				pad_value = (uint8_t)(integer_for(optarg) & 0xFF);
				break;
			case 'd': /* data to write */
				data_in = copystr(optarg);
				break;
			case 'v': /* print the version */
				printf("patch tool v0.1 -- Copyright (c) 2019 Tom Hancocks\n");
				break;
			default:
				/* ignored argument */
				break;
		}
	}

	/* Perform the patch based on the information supplied */
	if (!file) {
		fprintf(stderr, "No binary file supplied.\n");
		return 1;
	}

	FILE *fp = fopen(file, "r+");
	if (!fp) {
		fprintf(stderr, "Failed to open specified binary file.\n");
		return 2;
	}

	/* Seek to the required location */
	fseek(fp, offset, SEEK_SET);

	/* Convert the input data if required. */
	uint64_t r = 0;
	if (type != type_str) {
		/* We need to convert the data to a number, we're looking at a maximum
		   of 8 bytes. */
		uint8_t bytes[8] = { 0 };
		uint64_t v = (uint64_t)integer_for((void *)data_in);
		switch (type) {
			case type_dq:
				bytes[7] = ((v >> 56) & 0xFF);
				bytes[6] = ((v >> 48) & 0xFF);
				bytes[5] = ((v >> 40) & 0xFF);
				bytes[4] = ((v >> 32) & 0xFF);
			case type_dd:
				bytes[3] = ((v >> 24) & 0xFF);
				bytes[2] = ((v >> 16) & 0xFF);
			case type_dw:
				bytes[1] = ((v >> 8) & 0xFF);
			case type_db:
				bytes[0] = (v & 0xFF);
			default:
				break;
		}

		if ((r = fwrite(bytes, 1, type, fp)) != type) {
			fprintf(
				stderr, 
				"Something went wrong when patching file. Wrote %llu bytes.\n",
				r
			);
			return 3;
		}
	}
	else {
		/* Working with a string */
		uint32_t data_in_len = strlen(data_in);
		char *buffer = malloc(length + 1);
		memset(buffer, pad_value, length);
		memcpy(buffer, data_in, (data_in_len > length) ? length : data_in_len);
		
		if ((r = fwrite(buffer, 1, length, fp)) != length) {
			fprintf(
				stderr, 
				"Something went wrong when patching file. Wrote %llu bytes.\n",
				r
			);
			return 3;
		}
		free(buffer);
	}

	/* close the file and finish off */
	fclose(fp);

	return 0;
}