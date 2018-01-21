#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


typedef uint16_t bitmask;
typedef uint16_t Sample;

#define SET_BIT(mask, bitnum) (*(mask) |= 1 << (bitnum))
#define READ_BIT(mask, bitnum) (*(mask) & (1 << (bitnum)))
#define MAX_FLAGS 12
#define SIZE_MASK (0xF << 12)
#define SET_SIZE(mask, size) \
do { \
	*(mask) = (*(mask) & ~SIZE_MASK) | ((size) << 12); \
} while (0)
#define GET_SIZE(mask) ((*(mask) & SIZE_MASK) >> 12)

#define SET_SAMPLE(sample, offset, length) \
do { \
	*(sample) = (offset) << 8; \
	*(sample) |= (length) &  0x00FF; \
} while (0)

#define SAMPLE_GET_OFFSET(sample)	((sample) >> 8)
#define SAMPLE_GET_LENGTH(sample) ((sample) & 0x00FF)

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ;
#endif


static int
find_sample(const char *input, int ilen, int from, int *pos, int *len)
{
	int j;
	int best_start = 0;
	int best_len = 0;
	int cur_start = 0;
	int cur_len = 0;

	/* lookup for longest substring in previous input */
	for (j = 0; j < from; j++)
	{
		if (from + cur_len + 1 == ilen)
			break;

		if (input[j] == input[from + cur_len])
		{
			if (cur_len == 0)
				cur_start = j;
			
			cur_len++;
		}
		else /* substring ended */
		{
			if (cur_len > best_len)
			{
				best_start = cur_start;
				best_len = cur_len;
			}
			cur_start = 0;
			cur_len = 0;
		}
	}

	if (cur_len > best_len)
	{
		best_start = cur_start;
		best_len = cur_len;
	}

	*pos = best_start;
	*len = best_len;

	return best_len > 1 ? 1 : 0;
}

static int
compress_frame(const char *input, int ilen, char *output, int olen)
{
	int len = 0; /* actual compressed length */
	int i;
	bitmask *mask;
	uint8_t bitnum = 0;
	char *symbols;
	Sample *samples = NULL;
	int nsamples = 0;

	if (ilen == 0)
		return 0;

	for (i = 0; i < ilen; i++)
	{
		int sample_pos,
			sample_len;

		if (bitnum % MAX_FLAGS == 0)
		{
			/* if not the first call */
			if (samples != NULL)
			{
				output = (char *) samples;
			}

			bitnum = 0;
			mask = (bitmask *) output;
			*mask = 0;

			/* symbols array is right after the bitmask */
			symbols = output + sizeof(bitmask);
			/* samples array is after MAX_FLAGS symbols */
			samples = (Sample *) (symbols + sizeof(char) * MAX_FLAGS);
			nsamples = 0;
			len += sizeof(bitmask) + sizeof(char) * MAX_FLAGS;
		}

		/* if substring was found */
		if (find_sample(input, ilen, i, &sample_pos, &sample_len))
		{
			uint16_t offset = i - sample_pos;
			Sample *sample = samples;

			SET_SAMPLE(sample, offset, (uint16_t) sample_len);
			SET_BIT(mask, bitnum);
			i += sample_len;
			/* put next symbol to symbols array */
			symbols[bitnum] = input[i];

			samples++;
			nsamples++;
			bitnum++;

			len += sizeof(Sample); /* extra size for sample */
			
			DEBUG_PRINT("inserted <%u, %u> '%c'\n", offset, sample_len, input[i]);
		}
		else
		{
			symbols[bitnum] = input[i];
			bitnum++;

			DEBUG_PRINT("inserted '%c'\n", input[i]);
		}

		SET_SIZE(mask, bitnum);
	}

	return len;
}

static int
decompress_frame(const char *input, int ilen, char *output, int olen)
{
	int ipos = 0;
	int opos = 0;
	int total = 0;
	bitmask *mask;
	uint8_t bitnum = 0;

	char *symbols;
	Sample *samples = NULL;
	char *cur_input = (char *) input;
	int samples_count = 0;

	mask = (bitmask *) cur_input;
	symbols = (char *) cur_input + sizeof(bitmask);
	samples = (Sample*) (symbols + sizeof(char) * MAX_FLAGS);

	while (cur_input < input + ilen && bitnum < GET_SIZE(mask))
	{
		/* There is offset-length record */
		if (READ_BIT(mask, bitnum) > 0)
		{
			Sample *sample = samples;
			int i;

			for (i = 0; i < SAMPLE_GET_LENGTH(*sample); i++)
			{
				output[opos + i] = output[opos - SAMPLE_GET_OFFSET(*sample) + i];
				putchar(output[opos + i]);
			}

			opos += SAMPLE_GET_LENGTH(*sample);
			samples++;
			total += SAMPLE_GET_LENGTH(*sample);

			samples_count++;
		}
		output[opos] = symbols[bitnum];
		putchar(output[opos]);
		opos++;
		total++;

		bitnum++;

		if (bitnum % MAX_FLAGS == 0)
		{
			bitnum = 0;
			cur_input += sizeof(bitmask) + sizeof(char) * MAX_FLAGS + samples_count * sizeof(Sample);

			mask = (bitmask *) cur_input;
			symbols = (char *) cur_input + sizeof(bitmask);
			samples = (Sample*) (symbols + sizeof(char) * MAX_FLAGS);

			samples_count = 0;
		}

	}

	putchar('\n');
	return total;
}

int
main()
{
	// char input[] = "abcabc";
	// char input[] = "abc";
	// char input[] = "this is the test";
	// char input[] = "this is what this is";
	char input[] = "Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met";
	char compressed[1024];
	char decompressed[1024];
	int total;

	total = compress_frame(input, strlen(input), compressed, 1024);
	printf("decompressed: ");
	total = decompress_frame(compressed, total, decompressed, 1024);

	if (total != strlen(input))
	{
		printf("Data is corrupted: input len != decompressed len\n");
		exit(1);
	}

	if (memcmp(input, decompressed, strlen(input)) != 0)
	{
		printf("Data is corrupted: input != decompressed string\n");
		exit(1);
	}

	return 0;
}
