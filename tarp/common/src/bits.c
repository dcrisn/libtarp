#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <tarp/bits.h>

/*
 * True if bit at position N in NUM is set, else
 * False if bit is 0.
 */
bool mobit_is_bit_on(uint32_t num, unsigned int bit){
    // an index > than there are bits in NUM
    // e.g. bit 34 in a 32-bit int
    //if (sizeof num * 8 < bit){   // no need for this; if you try and do a bitwise on a
                                    // nonexistent bit, it's simply 0 so it doesn't raise an error
    //    return false;
    //}
    // -1 because we're counting from 1;
    // this makes more intuitive sense and offers a simpler
    // implementation; 
    // we subtract one because e.g. to get bit 1 you would NOT shift left at all;
    // else 1 << 1 would give you 2! ; so you always shift to the left by 1 less
    // than BIT (arg);
    return num & 1<<(bit-1);
}

/*
 * Sets bit BIT in NUM to 1.
 */
uint32_t mobit_set_bit(uint32_t num, unsigned int bit){
    return num |= 1<<(bit-1);
}

/*
 * Clears bit BIT in NUM by setting it to 0.
 */
uint32_t mobit_clear_bit(uint32_t num, unsigned int bit){
    uint32_t mask = ~(1<<(bit-1));
    return num &= mask;
}

/*
 * returns True if bit BIT is set, else False if bit is 0.
 *
 * Bit numbering starts from 1, from the leftmost BITARRAY index
 * (i.e. BITARRAY[0]), but from the RIGHT in the uint32_t integers 
 * at each index.
 * i.e. given a 2 uint32_t bitarray, we'll have 64 bits;
 * Bit 1 is bit 1 in the int at bitarray[0]; bit 64 is bit
 * 32 in the int at bitarray[1];
 */
bool mobit_array_is_bit_on(uint32_t bitarray[], size_t array_size, unsigned int bit){
    bit = bit-1;
    // if asking about a bit at a position > than there are bits in the bitarray
    // prevent indexing the array out of bounds
    if (!bitarray || bit > array_size * sizeof bitarray[0] * 8 ){
        return false;
    }
    int bitarray_element_index = bit / 32; // which uint32_t in the array the bit is in
    int bit_position = bit % 32; // which bit in the bitarray[bitarray_element_index] it is
    return bitarray[bitarray_element_index] & 1<<bit_position;

}


void mobit_array_set_bit(uint32_t bitarray[], size_t array_size, unsigned int bit){
    bit = bit-1;
    // if asking about a bit at a position > than there are bits in the bitarray
    if (!bitarray || bit > array_size * sizeof(bitarray[0]) * 8 ){
        return;
    }
    int bitarray_element_index = bit / 32; // which uint32_t in the array the bit is in
    int bit_position = bit % 32; // which bit in the bitarray[bitarray_element_index] it is
    bitarray[bitarray_element_index] |= 1<<bit_position;
}


void mobit_array_clear_bit(uint32_t bitarray[], size_t array_size, unsigned int bit){
    bit=bit-1;
    // if asking about a bit at a position > than there are bits in the bitarray
    if (!bitarray || bit > array_size * sizeof bitarray[0] * 8 ){
        return;
    }
    int bitarray_element_index = bit / 32; // which uint32_t in the array the bit is in
    int bit_position = bit % 32; // which bit in the bitarray[bitarray_element_index] it is
    uint32_t mask = ~(1<<bit_position);
    bitarray[bitarray_element_index] &= mask;
}


/*
 * Turns on HOW_MANY bits OFFSET bits from the right. 
 * The rightmost bit is at OFFSET 0, the 
 * 32-nd bit in a 32-bit int is at offset 32
 */
size_t mobit_set_bits(size_t num, unsigned int offset, unsigned int how_many){
    // this builds a mask where HOW_MANY bits are turned
    // on, starting from the right
    size_t mask = ~(~0U << how_many);

    // further moving the mask's bits to the left by OFFSET
    // and ORing it with num will do what the function says it does
    return num | (mask << offset);
}

/*
 * Clears HOW_MANY bits (sets them to 0) OFFSET bits 
 * from the right in number NUM.
 */
size_t mobit_clear_bits(size_t num, int offset, int how_many){
    size_t mask = 0;
    // build a mask where N bits from the RIGHT are 1 and the rest 0
    // the mask will then be ANDed with NUM

    // the mask is built by first settng N bits from the right to 1
    // then flippin all the bits
    for (int i = 0;i<how_many;i++){
        mask |= 1;
        // only shift while i < how_many-1
        // this is because if how_many is 1, no
        // shifting is necessary, else you push the bit onto 2!
        if (!(i<how_many-1)){
            break;
        }
        mask<<=1; // push bit to the left in preparation for the next iteration
    }
    // move bits in the correct position in the mask
    mask<<=offset;
    // negate the mask to flip the bits
    mask = ~mask;

    return num & mask;
}

/*
 * Gets HOW_MANY number of bits in NUM, offset OFFSET
 * number of bits from the right.
 *
 * Unlike set_bits above this one shifts NUM to 
 * the right to get its HOW_MANY bits OFFSET bits from
 * the right, rather than shifting the mask to the left; 
 * This is necessarily so, because you want to make the 
 * bits least significant; otherwise getting the fifth and
 * sixth bit from the right would be 2^4 + 2^5, while it 
 * should actually be just 3 (if both bits are on), since
 * that's what you're trying to find out: if both bits 
 * are on; 3 is 00000011, which is easily ANDED with a mask
 * to determine how many bits are on, while
 * 00110000 doesn't tell you that; 
 *
 * In other words, given a num 15, then all rightmost bits
 * will be 1s and the value of NUM will be the same as the result
 * returned. 
 */
size_t mobit_get_bits(size_t num, int offset, int how_many){
    // this builds a mask where HOW_MANY bits are turned
    // on, starting from the right
    size_t mask = ~(~0U << how_many);

    // this shift the whole NUM to the right OFFSET number 
    // of bits; for example if you shift right by 5, then
    // this fifth bit from the right will now be the rightmost
    // bit; since you're now ANDING with the mask created above,
    // you're getting HOW_MANY bits starting from the fifth bit
    return num >> offset & mask;
}

/*
 * <-- bool
 *     true if bit number N is 1, else false if 0
 * 
 * --> str
 *     A string
 *
 * --> n 
 *     The nth bit from the left to get in str. 
 *     Counting starts from 1, from the left.
 */
bool mobit_get_string_bit(char *str, uint32_t n){
    // The reason n gets decremented is that e.g. if you divide
    // 16 by 8 you would get 2, which would mean you would have to
    // look in the third char in STR i.e. str[2]. But n 16 is 
    // actually the last n in the second char of str. This whole
    // problem is due to the fact that counting starts at 0 in arrays,
    // but this function tells the user to start counting from 1 e.g.
    // n 1 is the first n , but char 0 is the first char -- so there's 
    // an inconsistency. It's easily solved by decrementing n by 1 while
    // assuming n counting starts from 1
    assert(n > 0);
    char c = str[--n/ 8];
    n =  n % 8;
    // if calculations are correct, bit should now be e [1,7] i.e.
    // an index in one of the char's in str
    assert(n <= 7);
    
    // get bit offset in char c; the bit at that position can then be 
    // obtained by shifting all bits in char c until that bit is the 
    // rightmost bit
    return (c >> (7-n)) & ON_BIT;
}

/*
 * <-- bool
 *     true if bit number N is 1, else false if 0
 * 
 * --> i
 *     the input key  to get the bit from
 *
 * --> n 
 *     The nth bit from the left to get in str. 
 *     Counting starts from 1, from the left and it must
 *     be 0 < n < 32.
 */
bool mobit_get_int_bit(uint32_t i, uint32_t n){
    // user counts from 1, but the operation here counts from 0
    // so decrement n by 1 first
    assert(n > 0 && n<=32);
    --n;
    
    uint32_t bit = (i >> (31-n)) & ON_BIT;
    //printf("bit is %u, i is %u, n is %u, ON_BIT is %u\n", bit, i, n, ON_BIT);
    return bit;
}
