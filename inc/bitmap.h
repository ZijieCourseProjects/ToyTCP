//
// Created by Eric Zhao on 11/10/2022.
//

#ifndef TJU_TCP_INC_BITMAP_H_
#define TJU_TCP_INC_BITMAP_H_
#ifdef bitmap_64
#define bitmap_type unsigned long long int
#else	// assumed to be 32 bits
#define bitmap_type unsigned int
#endif

typedef struct {
  int bits;    // number of bits in the array
  int words;    // number of words in the array
  bitmap_type *array;
} bitmap;

void bitmap_set(bitmap *b, int n);    // n is a bit index
void bitmap_clear(bitmap *b, int n);
int bitmap_read(bitmap *b, int n);

bitmap *bitmap_allocate(int bits);
void bitmap_deallocate(bitmap *b);

void bitmap_print(bitmap *b);

#endif //TJU_TCP_INC_BITMAP_H_
