//
// Created by Eric Zhao on 11/10/2022.
//
#include <stdio.h>
#include <stdlib.h>
#include "../inc/bitmap.h"

#ifdef bitmap_64
#define bitmap_type unsigned long long int
#define bitmap_shift        6
#define bitmap_mask        63
#define bitmap_wordlength  64
#define bitmap_fmt "%016llx"
#else	// assumed to be 32 bits
#define bitmap_type unsigned int
#define bitmap_shift        5
#define bitmap_mask        31
#define bitmap_wordlength  32
#define bitmap_fmt "%08x"
#endif

// get the types right
#define bitmap_one        (bitmap_type)1

// we expect 0 <= n and n < b->bits, but it is not verified

void bitmap_set(bitmap *b, int n) {
  int word = n >> bitmap_shift;        // n / bitmap_wordlength
  int position = n & bitmap_mask;    // n % bitmap_wordlength
  b->array[word] |= bitmap_one << position;
}

void bitmap_clear(bitmap *b, int n) {
  int word = n >> bitmap_shift;         // n / bitmap_wordlength
  int position = n & bitmap_mask;       // n % bitmap_wordlength
  b->array[word] &= ~(bitmap_one << position);
}

int bitmap_read(bitmap *b, int n) {
  int word = n >> bitmap_shift;         // n / bitmap_wordlength
  int position = n & bitmap_mask;       // n % bitmap_wordlength
  return (b->array[word] >> position) & 1;
}

bitmap *bitmap_allocate(int bits) {
  // error-checking should be better :-)
  bitmap *b = malloc(sizeof(bitmap));
  b->bits = bits;
  b->words = (bits + bitmap_wordlength - 1) / bitmap_wordlength;
  // divide, but round up for the ceiling
  b->array = calloc(b->words, sizeof(bitmap_type));
  return b;
}

void bitmap_deallocate(bitmap *b) {
  // error-checking should be better :-)
  free(b->array);
  free(b);
}

void bitmap_print(bitmap *b) {
  for (int i = 0; i < b->words; i++) { printf(" " bitmap_fmt, b->array[i]); }
  printf("\n");
}
