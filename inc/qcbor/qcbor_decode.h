/*==============================================================================
 Copyright (c) 2016-2018, The Linux Foundation.
 Copyright (c) 2018-2020, Laurence Lundblade.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and the following
 disclaimer in the documentation and/or other materials provided
 with the distribution.
 * Neither the name of The Linux Foundation nor the names of its
 contributors, nor the name "Laurence Lundblade" may be used to
 endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 =============================================================================*/


#ifndef qcbor_decode_h
#define qcbor_decode_h


#include "qcbor/qcbor_common.h"
#include "qcbor/qcbor_private.h"
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#if 0
} // Keep editor indention formatting happy
#endif
#endif


/**
@file qcbor_decode.h

Q C B O R    D e c o d e

 This section just discusses decoding assuming familiarity with the general
 description of this encoder / decoder in section XXX.
 
 Encoded CBOR can be viewed to have a tree structure
 where the lead nodes are non-aggregate types like
 integers and strings and the intermediate nodes are
 either arrays or maps. Fundamentally, all decoding
 is a pre-order traversal of the tree. Calling
 GetNext() repeatedly will perform this.
 
 This pre-order traversal gives natural decoding of
 arrays where the array members are taken
 in order, but does not give natural decoding of
 maps where access by label is usually preferred.
 Using the EnterMap and GetByLabel methods,
 map items can be accessed by label. EnterMap
narrows decoding to a particular map. GetByLabel
 allows decoding the item of a particular label in
 the particular map. This can be used with nested
 maps by calling EnterMapByLabel.
 
 When EnterMap is called, pre-order traversal
 continues to work. There is a cursor that is run
 over the tree with calls to GetNext. This can be
 intermixed with calls to GetByLabel. The pre-order
 traversal is limited just to the map entered. Attempts
 to GetNext beyond the end of the map will give
 the HIT END error.
 
  There is also EnterArray to decode arrays. It will
 narrow the traversal to the extent of the array
 entered.
 
 GetByLabel supports duplicate label detection
 and will result in an error if the map has
 duplicate labels.
 
 GetByLabel is implemented by performing the
 pre-order traversal of the map to find the labeled
 item everytime it is called. It doesn't build up
 a hash table, a binary search tree or some other
 efficiently searchable structure internally. For simple
 trees this is fine and for high-speed CPUs this is
 fine, but for complex trees on slow CPUs,
 it may have performance issues (these have
 not be quantified yet). One way ease this is
 to use GetItems which allows decoding of
 a list of items expected in an map in one
 traveral.
 
 Like encoding, decoding maintains an
 internal error state. Once a call to the
 decoder returns an error, this error state
 is entered and subsequent decoder calls
 do nothing. This allows for prettier and cleaner
 decoding code. The only error check needed
 is in the Finish call. 
 
 An easy and clean way to use this decoder
 is to always use EnterMap and EnterArray
 for each array or map. They will error
 if the input CBOR is not the expected
 array or map.  Then use GetInt, GetString
 to get the individual items of of the
 maps and arrays making use of the
 internal error tracking provided by this
 decoder. The only error check needed
 is the call to Finish.
  
 In some CBOR protocols, the type of
 a data item may be variable. Maybe even
 the type of one data item is dependent
 on another. In such designs, GetNext has
 to be used and the internal error checking
 can't be relied upon.
 
 
*/

/**
 The decode mode options.
 */
typedef enum {
   /** See QCBORDecode_Init() */
   QCBOR_DECODE_MODE_NORMAL = 0,
   /** See QCBORDecode_Init() */
   QCBOR_DECODE_MODE_MAP_STRINGS_ONLY = 1,
   /** See QCBORDecode_Init() */
   QCBOR_DECODE_MODE_MAP_AS_ARRAY = 2
   /* This is stored in uint8_t in places; never add values > 255 */
} QCBORDecodeMode;





/* Do not renumber these. Code depends on some of these values. */
/** The data type is unknown, unset or invalid. */
#define QCBOR_TYPE_NONE           0
// TODO: comment
#define QCBOR_TYPE_ANY            1

/** Type for an integer that decoded either between @c INT64_MIN and
    @c INT32_MIN or @c INT32_MAX and @c INT64_MAX. Data is in member
    @c val.int64. */
#define QCBOR_TYPE_INT64          2
/** Type for an integer that decoded to a more than @c INT64_MAX and
     @c UINT64_MAX.  Data is in member @c val.uint64. */
#define QCBOR_TYPE_UINT64         3
/** Type for an array. The number of items in the array is in @c
    val.uCount. */
#define QCBOR_TYPE_ARRAY          4
/** Type for a map; number of items in map is in @c val.uCount. */
#define QCBOR_TYPE_MAP            5
/** Type for a buffer full of bytes. Data is in @c val.string. */
#define QCBOR_TYPE_BYTE_STRING    6
/** Type for a UTF-8 string. It is not NULL-terminated. Data is in @c
    val.string.  */
#define QCBOR_TYPE_TEXT_STRING    7
/** Type for a positive big number. Data is in @c val.bignum, a
    pointer and a length. */
#define QCBOR_TYPE_POSBIGNUM      9
/** Type for a negative big number. Data is in @c val.bignum, a
    pointer and a length. */
#define QCBOR_TYPE_NEGBIGNUM     10
/** Type for [RFC 3339] (https://tools.ietf.org/html/rfc3339) date
    string, possibly with time zone. Data is in @c val.dateString */
#define QCBOR_TYPE_DATE_STRING   11
/** Type for integer seconds since Jan 1970 + floating-point
    fraction. Data is in @c val.epochDate */
#define QCBOR_TYPE_DATE_EPOCH    12
/** A simple type that this CBOR implementation doesn't know about;
    Type is in @c val.uSimple. */
#define QCBOR_TYPE_UKNOWN_SIMPLE 13

/** A decimal fraction made of decimal exponent and integer mantissa.
    See @ref expAndMantissa and QCBOREncode_AddDecimalFraction(). */
#define QCBOR_TYPE_DECIMAL_FRACTION            14

/** A decimal fraction made of decimal exponent and positive big
    number mantissa. See @ref expAndMantissa and
    QCBOREncode_AddDecimalFractionBigNum(). */
#define QCBOR_TYPE_DECIMAL_FRACTION_POS_BIGNUM 15

/** A decimal fraction made of decimal exponent and negative big
    number mantissa. See @ref expAndMantissa and
    QCBOREncode_AddDecimalFractionBigNum(). */
#define QCBOR_TYPE_DECIMAL_FRACTION_NEG_BIGNUM 16

/** A floating-point number made of base-2 exponent and integer
    mantissa.  See @ref expAndMantissa and
    QCBOREncode_AddBigFloat(). */
#define QCBOR_TYPE_BIGFLOAT      17

/** A floating-point number made of base-2 exponent and positive big
    number mantissa.  See @ref expAndMantissa and
    QCBOREncode_AddBigFloatBigNum(). */
#define QCBOR_TYPE_BIGFLOAT_POS_BIGNUM      18

/** A floating-point number made of base-2 exponent and negative big
    number mantissa.  See @ref expAndMantissa and
    QCBOREncode_AddBigFloatBigNum(). */
#define QCBOR_TYPE_BIGFLOAT_NEG_BIGNUM      19

/** Type for the value false. */
#define QCBOR_TYPE_FALSE         20
/** Type for the value true. */
#define QCBOR_TYPE_TRUE          21
/** Type for the value null. */
#define QCBOR_TYPE_NULL          22
/** Type for the value undef. */
#define QCBOR_TYPE_UNDEF         23
/** Type for a floating-point number. Data is in @c val.float. */
#define QCBOR_TYPE_FLOAT         26
/** Type for a double floating-point number. Data is in @c val.double. */
#define QCBOR_TYPE_DOUBLE        27
/** For @ref QCBOR_DECODE_MODE_MAP_AS_ARRAY decode mode, a map that is
     being traversed as an array. See QCBORDecode_Init() */
#define QCBOR_TYPE_MAP_AS_ARRAY  32

#define QCBOR_TYPE_BREAK         31 // Used internally; never returned

#define QCBOR_TYPE_OPTTAG       254 // Used internally; never returned



/*
 Approx Size of this:
   8 + 8 + 1 + 1 + 1 + (1 padding) + (4 padding) = 24 for first part
                                                  (20 on a 32-bit machine)
   16 bytes for the val union
   16 bytes for label union
   total = 56 bytes (52 bytes on 32-bit machine)
 */

/**
 The main data structure that holds the type, value and other info for
 a decoded item returned by QCBORDecode_GetNext() and
 QCBORDecode_GetNextWithTags().
 */
typedef struct _QCBORItem {
   /** Tells what element of the @c val union to use. One of @c
       QCBOR_TYPE_XXXX */
   uint8_t  uDataType;
   /** How deep the nesting from arrays and maps is. 0 is the top
    level with no arrays or maps entered. TODO: udpate this comment*/
   uint8_t  uNestingLevel;
    /** Tells what element of the label union to use. */
   uint8_t  uLabelType;
   /** 1 if allocated with string allocator, 0 if not. See
       QCBORDecode_SetMemPool() or QCBORDecode_SetUpAllocator() */
   uint8_t  uDataAlloc;
   /** Like @c uDataAlloc, but for label. */
   uint8_t  uLabelAlloc;
   /** If less than @c uNestingLevel, this item was the last one
       in an arry or map and closed out at least
       one nesting level */
   uint8_t  uNextNestLevel;

   /** The union holding the item's value. Select union member based
       on @c uDataType */
   union {
      /** The value for @c uDataType @ref QCBOR_TYPE_INT64. */
      int64_t     int64;
      /** The value for uDataType @ref QCBOR_TYPE_UINT64. */
      uint64_t    uint64;
      /** The value for @c uDataType @ref QCBOR_TYPE_BYTE_STRING and
          @ref QCBOR_TYPE_TEXT_STRING. */
      UsefulBufC  string;
      /** The "value" for @c uDataType @ref QCBOR_TYPE_ARRAY or @ref
          QCBOR_TYPE_MAP -- the number of items in the array or map.
          It is @c UINT16_MAX when decoding indefinite-lengths maps
          and arrays. */
      uint16_t    uCount;
      /** The value for @c uDataType @ref QCBOR_TYPE_DOUBLE. */
      double      dfnum;
      /** The value for @c uDataType @ref QCBOR_TYPE_DATE_EPOCH. */
      struct {
         int64_t  nSeconds;
         double   fSecondsFraction;
      } epochDate;
      /** The value for @c uDataType @ref QCBOR_TYPE_DATE_STRING. */
      UsefulBufC  dateString;
      /** The value for @c uDataType @ref QCBOR_TYPE_POSBIGNUM and
           @ref QCBOR_TYPE_NEGBIGNUM. */
      UsefulBufC  bigNum;
      /** The integer value for unknown simple types. */
      uint8_t     uSimple;
#ifndef QCBOR_CONFIG_DISABLE_EXP_AND_MANTISSA
      /** @anchor expAndMantissa

          The value for bigfloats and decimal fractions.  The use of the
          fields in this structure depend on @c uDataType.

          When @c uDataType is a @c DECIMAL_FRACTION, the exponent is
          base-10. When it is a @c BIG_FLOAT it is base-2.

          When @c uDataType is a @c POS_BIGNUM or a @c NEG_BIGNUM then the
          @c bigNum part of @c Mantissa is valid. Otherwise the
          @c nInt part of @c Mantissa is valid.

          See @ref QCBOR_TYPE_DECIMAL_FRACTION,
          @ref QCBOR_TYPE_DECIMAL_FRACTION_POS_BIGNUM,
          @ref QCBOR_TYPE_DECIMAL_FRACTION_NEG_BIGNUM,
          @ref QCBOR_TYPE_BIGFLOAT, @ref QCBOR_TYPE_BIGFLOAT_POS_BIGNUM,
          and @ref QCBOR_TYPE_BIGFLOAT_NEG_BIGNUM.

          Also see QCBOREncode_AddDecimalFraction(), QCBOREncode_AddBigFloat(),
          QCBOREncode_AddDecimalFractionBigNum() and
          QCBOREncode_AddBigFloatBigNum().
       */
      struct {
         int64_t nExponent;
         union {
            int64_t    nInt;
            UsefulBufC bigNum;
         } Mantissa;
      } expAndMantissa;
#endif
      uint64_t    uTagV;  // Used internally during decoding
   } val;

   /** Union holding the different label types selected based on @c
       uLabelType */
   union {
      /** The label for @c uLabelType @ref QCBOR_TYPE_BYTE_STRING and
          @ref QCBOR_TYPE_TEXT_STRING */
      UsefulBufC  string;
      /** The label for @c uLabelType for @ref QCBOR_TYPE_INT64 */
      int64_t     int64;
      /** The label for @c uLabelType for @ref QCBOR_TYPE_UINT64 */
      uint64_t    uint64;
   } label;

   /** Bit indicating which tags (major type 6) on this item. See
       QCBORDecode_IsTagged().  */
   uint64_t uTagBits;

} QCBORItem;



#define QCBOR_CONVERT_TYPE_INT64     0x01
#define QCBOR_CONVERT_TYPE_UINT64    0x02
#define QCBOR_CONVERT_TYPE_FLOAT     0x04
#define QCBOR_CONVERT_TYPE_DOUBLE    0x40
#define QCBOR_CONVERT_TYPE_BIGFLOAT  0x08
#define QCBOR_CONVERT_TYPE_DECIMAL_FRACTION 0x10
#define QCBOR_CONVERT_TYPE_BIG_NUM  0x20


/**
  @brief The type defining what a string allocator function must do.

  @param[in] pAllocateCxt  Pointer to context for the particular
                            allocator implementation What is in the
                            context is dependent on how a particular
                            string allocator works. Typically, it
                            will contain a pointer to the memory pool
                            and some booking keeping data.
 @param[in] pOldMem         Points to some memory allocated by the
                            allocator that is either to be freed or
                            to be reallocated to be larger. It is
                            @c NULL for new allocations and when called as
                            a destructor to clean up the whole
                            allocation.
 @param[in] uNewSize        Size of memory to be allocated or new
                            size of chunk to be reallocated. Zero for
                            a new allocation or when called as a
                            destructor.

 @return   Either the allocated buffer is returned, or @ref
           NULLUsefulBufC. @ref NULLUsefulBufC is returned on a failed
           allocation and in the two cases where there is nothing to
           return.

 This is called in one of four modes:

 Allocate -- @c uNewSize is the amount to allocate. @c pOldMem is @c
 NULL.

 Free -- @c uNewSize is 0. @c pOldMem points to the memory to be
 freed.  When the decoder calls this, it will always be the most
 recent block that was either allocated or reallocated.

 Reallocate -- @c pOldMem is the block to reallocate. @c uNewSize is
 its new size.  When the decoder calls this, it will always be the
 most recent block that was either allocated or reallocated.

 Destruct -- @c pOldMem is @c NULL and @c uNewSize is 0. This is called
 when the decoding is complete by QCBORDecode_Finish(). Usually the
 strings allocated by a string allocator are in use after the decoding
 is completed so this usually will not free those strings. Many string
 allocators will not need to do anything in this mode.

 The strings allocated by this will have @c uDataAlloc set to true in
 the @ref QCBORItem when they are returned. The user of the strings
 will have to free them. How they free them, depends on the string
 allocator.

 If QCBORDecode_SetMemPool() is called, the internal MemPool will be
 used. It has its own internal implementation of this function, so
 one does not need to be implemented.
 */
typedef UsefulBuf (* QCBORStringAllocate)(void *pAllocateCxt, void *pOldMem, size_t uNewSize);


/**
 This only matters if you use the built-in string allocator by setting
 it up with QCBORDecode_SetMemPool(). This is the size of the overhead
 needed by QCBORDecode_SetMemPool(). The amount of memory available
 for decoded strings will be the size of the buffer given to
 QCBORDecode_SetMemPool() less this amount.

 If you write your own string allocator or use the separately
 available malloc based string allocator, this size will not apply.
 */
#define QCBOR_DECODE_MIN_MEM_POOL_SIZE 8


/**
 This is used by QCBORDecode_SetCallerConfiguredTagList() to set a
 list of tags beyond the built-in ones.

 See also QCBORDecode_GetNext() for general description of tag
 decoding.
 */
typedef struct {
   /** The number of tags in the @c puTags. The maximum size is @ref
       QCBOR_MAX_CUSTOM_TAGS. */
   uint8_t uNumTags;
   /** An array of tags to add to recognize in addition to the
       built-in ones. */
   const uint64_t *puTags;
} QCBORTagListIn;


/**
 This is for QCBORDecode_GetNextWithTags() to be able to return the
 full list of tags on an item. It is not needed for most CBOR protocol
 implementations. Its primary use is for pretty-printing CBOR or
 protocol conversion to another format.

 On input, @c puTags points to a buffer to be filled in and
 uNumAllocated is the number of @c uint64_t values in the buffer.

 On output the buffer contains the tags for the item.  @c uNumUsed
 tells how many there are.
 */
typedef struct {
   uint8_t uNumUsed;
   uint8_t uNumAllocated;
   uint64_t *puTags;
} QCBORTagListOut;


/**
 QCBORDecodeContext is the data type that holds context decoding the
 data items for some received CBOR.  It is about 100 bytes, so it can
 go on the stack.  The contents are opaque, and the caller should not
 access any internal items.  A context may be re used serially as long
 as it is re initialized.
 */
typedef struct _QCBORDecodeContext QCBORDecodeContext;


/**
 Initialize the CBOR decoder context.

 @param[in] pCtx         The context to initialize.
 @param[in] EncodedCBOR  The buffer with CBOR encoded bytes to be decoded.
 @param[in] nMode        See below and @ref QCBORDecodeMode.

 Initialize context for a pre-order traversal of the encoded CBOR
 tree.

 Most CBOR decoding can be completed by calling this function to start
 and QCBORDecode_GetNext() in a loop.

 If indefinite-length strings are to be decoded, then
 QCBORDecode_SetMemPool() or QCBORDecode_SetUpAllocator() must be
 called to set up a string allocator.

 If tags other than built-in tags are to be recognized and recorded in
 @c uTagBits, then QCBORDecode_SetCallerConfiguredTagList() must be
 called. The built-in tags are those for which a macro of the form @c
 CBOR_TAG_XXX is defined.

 Three decoding modes are supported.  In normal mode, @ref
 QCBOR_DECODE_MODE_NORMAL, maps are decoded and strings and integers
 are accepted as map labels. If a label is other than these, the error
 @ref QCBOR_ERR_MAP_LABEL_TYPE is returned by QCBORDecode_GetNext().

 In strings-only mode, @ref QCBOR_DECODE_MODE_MAP_STRINGS_ONLY, only
 text strings are accepted for map labels.  This lines up with CBOR
 that converts to JSON. The error @ref QCBOR_ERR_MAP_LABEL_TYPE is
 returned by QCBORDecode_GetNext() if anything but a text string label
 is encountered.

 In @ref QCBOR_DECODE_MODE_MAP_AS_ARRAY maps are treated as special
 arrays.  They will be return with special @c uDataType @ref
 QCBOR_TYPE_MAP_AS_ARRAY and @c uCount, the number of items, will be
 double what it would be for a normal map because the labels are also
 counted. This mode is useful for decoding CBOR that has labels that
 are not integers or text strings, but the caller must manage much of
 the map decoding.
 */
void QCBORDecode_Init(QCBORDecodeContext *pCtx, UsefulBufC EncodedCBOR, QCBORDecodeMode nMode);


/**
 @brief Set up the MemPool string allocator for indefinite-length strings.

 @param[in] pCtx         The decode context.
 @param[in] MemPool      The pointer and length of the memory pool.
 @param[in] bAllStrings  If true, all strings, even of definite
                         length, will be allocated with the string
                         allocator.

 @return Error if the MemPool was less than @ref QCBOR_DECODE_MIN_MEM_POOL_SIZE.

 indefinite-length strings (text and byte) cannot be decoded unless
 there is a string allocator configured. MemPool is a simple built-in
 string allocator that allocates bytes from a memory pool handed to it
 by calling this function.  The memory pool is just a pointer and
 length for some block of memory that is to be used for string
 allocation. It can come from the stack, heap or other.

 The memory pool must be @ref QCBOR_DECODE_MIN_MEM_POOL_SIZE plus
 space for all the strings allocated.  There is no overhead per string
 allocated. A conservative way to size this buffer is to make it the
 same size as the CBOR being decoded plus @ref
 QCBOR_DECODE_MIN_MEM_POOL_SIZE.

 This memory pool is used for all indefinite-length strings that are
 text strings or byte strings, including strings used as labels.

 The pointers to strings in @ref QCBORItem will point into the memory
 pool set here. They do not need to be individually freed. Just
 discard the buffer when they are no longer needed.

 If @c bAllStrings is set, then the size will be the overhead plus the
 space to hold **all** strings, definite and indefinite-length, value
 or label. The advantage of this is that after the decode is complete,
 the original memory holding the encoded CBOR does not need to remain
 valid.

 If this function is never called because there is no need to support
 indefinite-length strings, the internal MemPool implementation should
 be dead-stripped by the loader and not add to code size.
 */
QCBORError QCBORDecode_SetMemPool(QCBORDecodeContext *pCtx, UsefulBuf MemPool, bool bAllStrings);


/**
 @brief Sets up a custom string allocator for indefinite-length strings

 @param[in] pCtx                 The decoder context to set up an
                                 allocator for.
 @param[in] pfAllocateFunction   Pointer to function that will be
                                 called by QCBOR for allocations and
                                 frees.
 @param[in] pAllocateContext     Context passed to @c
                                 pfAllocateFunction.
 @param[in] bAllStrings          If true, all strings, even of definite
                                 length, will be allocated with the
                                 string allocator.

 indefinite-length strings (text and byte) cannot be decoded unless
 there a string allocator is configured. QCBORDecode_SetUpAllocator()
 allows the caller to configure an external string allocator
 implementation if the internal string allocator is not suitable. See
 QCBORDecode_SetMemPool() to configure the internal allocator. Note
 that the internal allocator is not automatically set up.

 The string allocator configured here can be a custom one designed and
 implemented by the caller.  See @ref QCBORStringAllocate for the
 requirements for a string allocator implementation.

 A malloc-based string external allocator can be obtained by calling
 @c QCBORDecode_MakeMallocStringAllocator(). It will return a function
 and pointer that can be given here as @c pAllocatorFunction and @c
 pAllocatorContext. It uses standard @c malloc() so @c free() must be
 called on all strings marked by @c uDataAlloc @c == @c 1 or @c
 uLabelAlloc @c == @c 1 in @ref QCBORItem.

 Note that an older version of this function took an allocator
 structure, rather than single function and pointer.  The older
 version @c QCBORDecode_MakeMallocStringAllocator() also implemented
 the older interface.
 */
void QCBORDecode_SetUpAllocator(QCBORDecodeContext *pCtx,
                                QCBORStringAllocate pfAllocateFunction,
                                void *pAllocateContext,
                                bool bAllStrings);

/**
 @brief Configure list of caller-selected tags to be recognized.

 @param[in] pCtx       The decode context.
 @param[out] pTagList  Structure holding the list of tags to configure.

 This is used to tell the decoder about tags beyond those that are
 built-in that should be recognized. The built-in tags are those with
 macros of the form @c CBOR_TAG_XXX.

 The list pointed to by @c pTagList must persist during decoding.  No
 copy of it is made.

 The maximum number of tags that can be added is @ref
 QCBOR_MAX_CUSTOM_TAGS.  If a list larger than this is given, the
 error will be returned when QCBORDecode_GetNext() is called, not
 here.

 See description of @ref QCBORTagListIn.
 */
void QCBORDecode_SetCallerConfiguredTagList(QCBORDecodeContext *pCtx, const QCBORTagListIn *pTagList);


/**
 @brief Gets the next item (integer, byte string, array...) in
        preorder traversal of CBOR tree.

 @param[in]  pCtx          The decoder context.
 @param[out] pDecodedItem  Holds the CBOR item just decoded.

 @retval QCBOR_ERR_INDEFINITE_STRING_CHUNK  Not well-formed, one of the
                                            chunks in indefinite-length
                                            string is wrong type.

 @retval QCBOR_ERR_ARRAY_OR_MAP_STILL_OPEN  Not well-formed, array or map
                                            not closed.

 @retval QCBOR_ERR_UNSUPPORTED     Not well-formed, input contains
                                   unsupported CBOR.

 @retval QCBOR_ERR_HIT_END         Not well-formed, unexpectedly ran out
                                   of bytes.

 @retval QCBOR_ERR_BAD_TYPE_7      Not well-formed, bad simple type value.

 @retval QCBOR_ERR_BAD_BREAK       Not well-formed, break occurs where
                                   not allowed.

 @retval QCBOR_ERR_EXTRA_BYTES     Not well-formed, unprocessed bytes at
                                   the end.

 @retval QCBOR_ERR_BAD_INT         Not well-formed, length of integer is
                                   bad.

 @retval QCBOR_ERR_BAD_OPT_TAG     Invalid CBOR, tag on wrong type.

 @retval QCBOR_ERR_ARRAY_TOO_LONG  Implementation limit, array or map
                                   too long.

 @retval QCBOR_ERR_INT_OVERFLOW    Implementation limit, negative
                                   integer too large.

 @retval QCBOR_ERR_DATE_OVERFLOW   Implementation limit, date larger
                                   than can be handled.

 @retval QCBOR_ERR_ARRAY_NESTING_TOO_DEEP  Implementation limit, nesting
                                           too deep.

 @retval QCBOR_ERR_STRING_ALLOCATE Resource exhaustion, string allocator
                                   failed.

 @retval QCBOR_ERR_MAP_LABEL_TYPE  Configuration error / Implementation
                                   limit encountered a map label this is
                                   not a string on an integer.

 @retval QCBOR_ERR_NO_STRING_ALLOCATOR  Configuration error, encountered
                                        indefinite-length string with no
                                        allocator configured.
 @retval QCBOR_ERR_NO_MORE_ITEMS   No more bytes to decode. The previous
                                   item was successfully decoded. This
                                   is usually how the non-error end of
                                   a CBOR stream / sequence is detected.

 @c pDecodedItem is filled in with the value parsed. Generally, the
 following data is returned in the structure:

 - @c uDataType which indicates which member of the @c val union the
   data is in. This decoder figures out the type based on the CBOR
   major type, the CBOR "additionalInfo", the CBOR optional tags and
   the value of the integer.

 - The value of the item, which might be an integer, a pointer and a
   length, the count of items in an array, a floating-point number or
   other.

 - The nesting level for maps and arrays.

 - The label for an item in a map, which may be a text or byte string
   or an integer.

 - The CBOR optional tag or tags.

 See documentation on in the data type @ref _QCBORItem for all the
 details on what is returned.

 This function handles arrays and maps. When first encountered a @ref
 QCBORItem will be returned with major type @ref QCBOR_TYPE_ARRAY or
 @ref QCBOR_TYPE_MAP. @c QCBORItem.val.uCount will indicate the number
 of Items in the array or map.  Typically, an implementation will call
 QCBORDecode_GetNext() in a for loop to fetch them all. When decoding
 indefinite-length maps and arrays, @c QCBORItem.val.uCount is @c
 UINT16_MAX and @c uNextNestLevel must be used to know when the end of
 a map or array is reached.

 Nesting level 0 is the outside top-most nesting level. For example,
 in a CBOR structure with two items, an integer and a byte string
 only, both would be at nesting level 0.  A CBOR structure with an
 array open, an integer and a byte string, would have the integer and
 byte string as nesting level 1.

 Here is an example of how the nesting level is reported with no arrays
 or maps at all.

 @verbatim
 CBOR Structure           Nesting Level
 Integer                    0
 Byte String                0
 @endverbatim

 Here is an example of how the nesting level is reported with a simple
 array and some top-level items.

 @verbatim
 Integer                    0
 Array (with 2 items)       0
 Byte String                1
 Byte string                1
 Integer                    0
 @endverbatim


 Here's a more complex example
 @verbatim

 Map with 2 items           0
 Text string                1
 Array with 3 integers      1
 integer                    2
 integer                    2
 integer                    2
 text string                1
 byte string                1
 @endverbatim

 In @ref _QCBORItem, @c uNextNestLevel is the nesting level for the
 next call to QCBORDecode_GetNext(). It indicates if any maps or
 arrays were closed out during the processing of the just-fetched @ref
 QCBORItem. This processing includes a look-ahead for any breaks that
 close out indefinite-length arrays or maps. This value is needed to
 be able to understand the hierarchical structure. If @c
 uNextNestLevel is not equal to @c uNestLevel the end of the current
 map or array has been encountered. This works the same for both
 definite and indefinite-length arrays.

 This decoder support CBOR type 6 tagging. The decoding of particular
 given tag value may be supported in one of three different ways.

 First, some common tags are fully and transparently supported by
 automatically decoding them and returning them in a @ref QCBORItem.
 These tags have a @c QCBOR_TYPE_XXX associated with them and manifest
 pretty much the same as a standard CBOR type. @ref
 QCBOR_TYPE_DATE_EPOCH and the @c epochDate member of @ref QCBORItem
 is an example.

 Second are tags that are automatically recognized, but not decoded.
 These are tags that have a @c \#define of the form @c CBOR_TAG_XXX.
 These are recorded in the @c uTagBits member of @ref QCBORItem. There
 is an internal table that maps each bit to a particular tag value
 allowing up to 64 tags on an individual item to be reported (it is
 rare to have more than one or two). To find out if a particular tag
 value is set call QCBORDecode_IsTagged() on the @ref QCBORItem.  See
 also QCBORDecode_GetNextWithTags().

 Third are tags that are not automatically recognized, because they
 are proprietary, custom or more recently registered with [IANA]
 (https://www.iana.org/assignments/cbor-tags/cbor-tags.xhtml). The
 internal mapping table has to be configured to recognize these. Call
 QCBORDecode_SetCallerConfiguredTagList() to do that. Then
 QCBORDecode_IsTagged() will work with them.

 The actual decoding of tags supported in the second and third way
 must be handled by the caller. Often this is simply verifying that
 the expected tag is present on a map, byte string or such.  In other
 cases, there might a complicated map structure to decode.

 See @ref Tags-Overview for a description of how to go about creating
 custom tags.

 This tag decoding design is to be open-ended and flexible to be able
 to handle newly defined tags, while using very little memory, in
 particular keeping @ref QCBORItem as small as possible.

 If any error occurs, \c uDataType and \c uLabelType will be set
 to \ref QCBOR_TYPE_NONE. If there is no need to know the specific
 error, \ref QCBOR_TYPE_NONE can be checked for and the return value
 ignored.

 Errors fall in several categories as noted in list above:

 - Not well-formed errors are those where there is something
 syntactically and fundamentally wrong with the CBOR being
 decoded. Encoding should stop completely.

 - Invalid CBOR is well-formed, but still not correct. It is probably
 best to stop decoding, but not necessary.

 - This implementation has some size limits. They should rarely be
 encountered. If they are it may because something is wrong with the
 CBOR, for example an array size is incorrect.

 - Resource exhaustion. This only occurs when a string allocator is
 configured to handle indefinite-length strings as other than that,
 this implementation does no dynamic memory allocation.

 - There are a few CBOR constructs that are not handled without some
 extra configuration. These are indefinite length strings and maps
 with labels that are not strings or integers. See QCBORDecode_Init().

 */
QCBORError QCBORDecode_GetNext(QCBORDecodeContext *pCtx, QCBORItem *pDecodedItem);


/**
 @brief Gets the next item including full list of tags for item.

 @param[in]  pCtx          The decoder context.
 @param[out] pDecodedItem  Holds the CBOR item just decoded.
 @param[in,out] pTagList   On input array to put tags in; on output
                           the tags on this item. See
                           @ref QCBORTagListOut.

 @return See return values for QCBORDecode_GetNext().

 @retval QCBOR_ERR_TOO_MANY_TAGS  The size of @c pTagList is too small.

 This works the same as QCBORDecode_GetNext() except that it also
 returns the full list of tags for the data item. This function should
 only be needed when parsing CBOR to print it out or convert it to
 some other format. It should not be needed to implement a CBOR-based
 protocol.  See QCBORDecode_GetNext() for the main description of tag
 decoding.

 Tags will be returned here whether or not they are in the built-in or
 caller-configured tag lists.

 CBOR has no upper bound of limit on the number of tags that can be
 associated with a data item though in practice the number of tags on
 an item will usually be small, perhaps less than five. This will
 return @ref QCBOR_ERR_TOO_MANY_TAGS if the array in @c pTagList is
 too small to hold all the tags for the item.

 (This function is separate from QCBORDecode_GetNext() so as to not
 have to make @ref QCBORItem large enough to be able to hold a full
 list of tags. Even a list of five tags would nearly double its size
 because tags can be a @c uint64_t ).
 */
QCBORError QCBORDecode_GetNextWithTags(QCBORDecodeContext *pCtx, QCBORItem *pDecodedItem, QCBORTagListOut *pTagList);


/**
 @brief Determine if a CBOR item was tagged with a particular tag

 @param[in] pCtx    The decoder context.
 @param[in] pItem   The CBOR item to check.
 @param[in] uTag    The tag to check, one of @c CBOR_TAG_XXX.

 @return 1 if it was tagged, 0 if not

 See QCBORDecode_GetNext() for the main description of tag
 handling. For tags that are not fully decoded a bit corresponding to
 the tag is set in in @c uTagBits in the @ref QCBORItem. The
 particular bit depends on an internal mapping table. This function
 checks for set bits against the mapping table.

 Typically, a protocol implementation just wants to know if a
 particular tag is present. That is what this provides. To get the
 full list of tags on a data item, see QCBORDecode_GetNextWithTags().

 Also see QCBORDecode_SetCallerConfiguredTagList() for the means to
 add new tags to the internal list so they can be checked for with
 this function.
 */
int QCBORDecode_IsTagged(QCBORDecodeContext *pCtx, const QCBORItem *pItem, uint64_t uTag);


/**
 @brief Check whether all the bytes have been decoded and maps and arrays closed.

 @param[in]  pCtx  The context to check.

 @retval  QCBOR_ERR_ARRAY_OR_MAP_STILL_OPEN   The CBOR is not well-formed
 as some map or array was not closed off. This should always be treated as an
 unrecoverable error.
 
 @retval QCBOR_ERR_EXTRA_BYTES The CBOR was decoded correctly and
 all maps and arrays are closed, but some of the bytes in the input were not consumed.
 This may or may not be considered an error.
 
 @retval QCBOR_SUCCES There were no errors and all bytes were
 consumed.
 
 This should always be called to determine if all maps and arrays
 where correctly closed and that the CBOR was well-formed.
 
 This calls the destructor for the string allocator, if one is in use.
 
 Some CBOR protocols use a CBOR sequence [RFC 8742]
 (https://tools.ietf.org/html/rfc8742) .  A CBOR sequence typically
 doesn't start out with a map or an array. The end of the CBOR is
 determined in some other way, perhaps by external framing, or by the
 occurrence of some particular CBOR data item or such. The buffer given
 to decode must start out with valid CBOR, but it can have extra bytes
 at the end that are not CBOR or CBOR that is to be ignored.
 
 QCBORDecode_Finish() should still be called when decoding CBOR
 Sequences to check that the input decoded was well-formed. If the
 input was well-formed and there are extra bytes at the end @ref
 QCBOR_ERR_EXTRA_BYTES will be returned.  This can be considered a
 successful decode.
 */
QCBORError QCBORDecode_Finish(QCBORDecodeContext *pCtx);



/**
 @brief Get the decoding error.

 @param[in] pCtx    The decoder context.
 @returns The decoding error.

 All decoding functions except GetNext() do not return an error.
 Instead they set an internal error state. Once an error has
 occured, no further decoding will be performed even if further
 decoding functions are called.

 The error will be returned when QCBORDecode_Finish() finish is
 called. This can make call sequence for decoding a given
 CBOR protocol very clean and simple in many cases.

 Note that no reference to the decoded data should be made until
 after QCBORDecode_Finish() is called as it will not be valid
 after a decoding error has occured.

 This will not work for protocols where the expected data items
 depend on preceding data items existence, type, label or value.
 In that case call this function to see there is no error
 before examining data items before QCBORDecode_Finish() is
 called.

 Some errors, like integer conversion overflow, date string
 format may not affect the flow of a protocol. The protocol
 decoder may wish to proceed even if they occur. In that case
 QCBORDecode_GetAndResetError() may be called after these
 data items are fetched.
 */
static QCBORError QCBORDecode_GetError(QCBORDecodeContext *pCtx);

/**
 @brief Get and reset the decoding error.

 @param[in] pCtx    The decoder context.
 @returns The decoding error.

 This returns the same as QCBORDecode_GetError() and also
 resets the error state to \ref QCBOR_SUCCESS.
 */

static QCBORError QCBORDecode_GetAndResetError(QCBORDecodeContext *pCtx);



/**
 @brief Decode next item as a signed 64-bit integer.

 @param[in] pCtx   The decode context
 @param[out] pnValue  64-bit integer with item

 On error, the decoder internal error state is set.

 The CBOR data item to decode must be a positive or negative integer. If not
 \ref QCBOR_ERR_UNEXPECTED_TYPE is set.

 CBOR can represent negative integers smaller than can be represetned in
 an int64_t. \ref QCBOR_ERR_INT_OVERFLOW is returned in this case.

 See also QCBORDecode_GetInt64Convert() and QCBORDecode_GetInt64ConvertAll().
 */
static void QCBORDecode_GetInt64(QCBORDecodeContext *pCtx, int64_t *pnValue);


/**
 @brief Decode next item as a signed 64-bit integer with basic conversions

 @param[in] pCtx   The decode context
 @param[in] uOptions The integer conversion options.
 @param[out] pnValue  64-bit integer with item

 The CBOR data item must be either a positive integer, negative integer or floating-point number.
 \c uOptions is one of XXX and controls which conversions will be performed.

 See also QCBORDecode_GetInt64ConvertAll() which will perform the same conversions
 as this and a lot more at the cost of adding more object code to your executable.

 On error, this sets the decoder last error.  If the data item is of a type that
 can't be decoded by this function, \ref QCBOR_ERR_UNEXPECTED_TYPE is set. If
 the data item can be decode, but the option requesting it is not set, then
 \ref QCBOR_ERR_UNEXPECTED_TYPE will be set. If the data item is too large
 or small to be represented as a 64-bit signed integer, \ref QCBOR_ERR_CONVERSION_UNDER_OVER_FLOW
 us set.

 When converting floating-point values, the integer is rounded to the nearest integer using
 llround(). By default, floating-point suport is enabled for QCBOR. If it is turned off,
 then floating-point conversion is not available.
 
 */
static void QCBORDecode_GetInt64Convert(QCBORDecodeContext *pCtx, uint32_t uOptions, int64_t *pnValue);


/**
 @brief Decode next item as a signed 64-bit integer with conversions

 @param[in] pCtx   The decode context
 @param[in] uOptions The integer conversion options.
 @param[out] pnValue  64-bit integer with item

 This is the same as QCBORDecode_GetInt64Convert() but supports many more conversions at
 the cost of adding more object code to the executable.

 The additiona data item types that are suported are positive and negative bignums,
 decimal fractions and big floats, including decimal fractions and big floats that use bignums.
 Not that all these types can support numbers much larger that can be represented by
 in a 64-bit integer, so \ref QCBOR_ERR_CONVERSION_UNDER_OVER_FLOW will
 often be encountered.
 */
void QCBORDecode_GetInt64ConvertAll(QCBORDecodeContext *pCtx, uint32_t uOptions, int64_t *pnValue);

/**
 @brief Decode next item as an unsigned 64-bit integer.

 @param[in] pCtx   The decode context
 @param[out] puValue  64-bit integer with item

 The sames as QCBORDecode_GetInt64(), but returns an unsigned integer and thus
 can only decode CBOR positive integers. \ref QCBOR_ERR_NUMBER_SIGN_CONVERSION
 is set if the input is a negative integer.

 See also QCBORDecode_GetUint64Convert() and QCBORDecode_GetUint64ConvertAll().
*/
static void QCBORDecode_GetUInt64(QCBORDecodeContext *pCtx, uint64_t *puValue);

/**
 @brief Decode next item as an unsigned 64-bit integer with basic conversions.

 @param[in] pCtx   The decode context
 @param[out] puValue  64-bit integer with item

 The sames as QCBORDecode_GetInt64Convert(), but returns an unsigned integer and thus
 sets \ref QCBOR_ERR_NUMBER_SIGN_CONVERSION
 is set if the value to be decoded is negatve.

 See also QCBORDecode_GetUint64Convert() and QCBORDecode_GetUint64ConvertAll().
*/
static void QCBORDecode_GetUInt64Convert(QCBORDecodeContext *pCtx, uint32_t uOptions, uint64_t *puValue);

/**
 @brief Decode next item as an unsigned 64-bit integer with conversions

 @param[in] pCtx   The decode context
 @param[out] puValue  64-bit integer with item

 The sames as QCBORDecode_GetInt64ConvertAll(), but returns an unsigned integer and thus
 sets \ref QCBOR_ERR_NUMBER_SIGN_CONVERSION
 is set if the value to be decoded is negatve.

 See also QCBORDecode_GetUint64Convert() and QCBORDecode_GetUint64ConvertAll().
*/
void QCBORDecode_GetUInt64ConvertAll(QCBORDecodeContext *pCtx, uint32_t uOptions, uint64_t *puValue);


/**
 @brief Decode next item as a floating-point value.

 @param[in] pCtx   The decode context
 @param[out] pValue  The returned floating-point value.

 On error, the decoder internal error state is set.

 The CBOR data item to decode must be a hafl-precision, single-precision
 or double-precision floating-point value.  If not
 \ref QCBOR_ERR_UNEXPECTED_TYPE is set.

 See also QCBORDecode_GetDoubleConvert() and QCBORDecode_GetDoubleConvertAll().
*/
static void QCBORDecode_GetDouble(QCBORDecodeContext *pCtx, uint32_t uOptions, double *pValue);


/**
 @brief Decode next item as a floating-point value with basic conversion.

 @param[in] pCtx   The decode context
 @param[out] pValue  The returned floating-point value.

 On error, the decoder internal error state is set.

 The CBOR data item to decode must be a hafl-precision, single-precision
 or double-precision floating-point value or a positive or negative integer.  If not
 \ref QCBOR_ERR_UNEXPECTED_TYPE is set.

 Positive and negative integers can always be converted to floating-point,
 so this always succeeds.

 Note that a large 64-bit integer can have more precision than even a
 double floating-point value, so there is loss of precision in some conversions.

 See also QCBORDecode_GetDouble() and QCBORDecode_GetDoubleConvertAll().
*/
static void QCBORDecode_GetDoubleConvert(QCBORDecodeContext *pCtx, uint32_t uOptions, double *pValue);


/**
 @brief Decode next item as a floating-point value with conversion.

 @param[in] pCtx   The decode context
 @param[out] pValue  The returned floating-point value.

 On error, the decoder internal error state is set.

 In addition to conversions supported by QCBORDecode_GetDoubleConvert(),
 conversion from positive and negative bignums, decimal fractions and big floats
 are supported.

 Big numbers, decimal fractions and big floats that are too small or too large
 to be reprented as a floating-point number will be returned as plus or minus
 zero or infinity. There is also often loss of precision in the conversion.

 See also QCBORDecode_GetDoubleConvert() and QCBORDecode_GetDoubleConvert().
*/
void QCBORDecode_GetDoubleConvertAll(QCBORDecodeContext *pCtx, uint32_t uOptions, double *pValue);



void QCBORDecode_GetBytes(QCBORDecodeContext *pCtx,  UsefulBufC *pValue);

void QCBORDecode_GetText(QCBORDecodeContext *pCtx,  UsefulBufC *pValue);

void QCBORDecode_GetPosBignum(QCBORDecodeContext *pCtx,  UsefulBufC *pValue);

void QCBORDecode_GetNegBignum(QCBORDecodeContext *pCtx,  UsefulBufC *pValue);




/* Next item must be map or this generates an error.
 
 
This puts the decoder in map mode which narrows
decoding to the map entered and enables use of
getting items by label.
 
 Nested maps can be decoded like this by entering
 each map in turn.

  Call QCBORDecode_ExitMap() to exit the current map
 decoding level. When all map decoding layers are exited
 then map mode is fully exited.
 
 While in map mode, GetNext works as usual on the
 map and the standard in-order traversal cursor
 is maintained. Attempts to get items off the end of the
 map will give error XXX (rather going to the next
 item after the map as it would when not in map
 mode).
 
 You can rewind the inorder traversal cursor to the
 beginning of the map with RewindMap().
 
 Exiting leaves the cursor at the
 data item following the last entry in the map.
 
 Entering and Exiting map mode consumes the whole
 map and its contents as a GetNext after exiting
 will return the item after the map. */
static QCBORError QCBORDecode_EnterMap(QCBORDecodeContext *pCtx);


void QCBORDecode_ExitMap(QCBORDecodeContext *pCtx);

/*
 Indicate if decoding is in map mode more not.
 */
bool QCBORDecode_InMapMode(QCBORDecodeContext *pCtxt);


/*
 Restarts fetching of items in a map to the start of the
 map. This is for GetNext. It has no effect on
 GetByLabel (which always searches from the start).
 */
void QCBORDecode_RewindMap(QCBORDecodeContext *pCtxt);


QCBORError QCBORDecode_EnterArray(QCBORDecodeContext *pCtx);


void QCBORDecode_ExitArray(QCBORDecodeContext *pCtx);

QCBORError QCBORDecode_EnterArrayFromMapN(QCBORDecodeContext *pMe, int64_t uLabel);


QCBORError QCBORDecode_EnterArrayFromMapSZ(QCBORDecodeContext *pMe, const char  *szLabel);


//QCBORError QCBORDecode_EnterMapX(QCBORDecodeContext *pCtx,  MapDecode *pMap);

                     


/*
 Get an item out of a map.
 
 Decoding must be in map mode for this to work.
 
 
 
Seek to the beginning of the map.
Consume items looking for the nLabel.
Always go through the whole map and always look for duplicates.
Return the item found, if no errors.

Allow specification of type required.



*/
QCBORError QCBORDecode_GetItemInMap(QCBORDecodeContext *pCtx,
                         int64_t nLabel,
                         uint8_t qcbor_type,
                         QCBORItem *pItem);


QCBORError QCBORDecode_GetItemInMapSZ(QCBORDecodeContext *pCtx,
const char *szLabel,
uint8_t qcbor_type,
QCBORItem *pItem);

/*
 This gets several labeled items out of a map.
 
 pItemArray is an array of items terminated by an item
 with uLabelType QCBOR_TYPE_NONE.
 
 On input the the array of items is the list of labels to fetch
 items for.
 
 On output the array is the data items found. If the label
 wasn't found, uDataType is QCBOR_TYPE_NONE.
 
 This is a CPU-efficient way to decode a bunch of items in a map. It
 is more efficient than scanning each individually because the map
 only needs to be traversed once.
 
 If any duplicate labels are detected, this returns an error.
 
 This will return maps and arrays that are in the map, but
 provides no way to descend into and decode them.
 
 */
QCBORError QCBORDecode_GetItemsInMap(QCBORDecodeContext *pCtx, QCBORItem *pItemList);



QCBORError QCBORDecode_GetIntInMap(QCBORDecodeContext *pCtx, int64_t nLabel, int64_t *pInt);

void QCBORDecode_GetIntInMapSZ(QCBORDecodeContext *pCtx, const char *szLabel, int64_t *pInt);


void QCBORDecode_GetBstrInMapSZ(QCBORDecodeContext *pCtx, const char *szLabel, UsefulBufC *pBstr);

void QCBORDecode_GetTextInMapSZ(QCBORDecodeContext *pCtx, const char *szLabel, UsefulBufC *pBstr);


/*
  Find a map in a map by integer label and enter it.
 
 This will do duplicate detection on the particular label.
 
 Call QCBORDecode_ExitMap() to return to the mode / level
 from before this was called.
 
 Seek to to the beginning of the map.
 Consume items looking for nLabel
 */
QCBORError QCBORDecode_EnterMapFromMap(QCBORDecodeContext *pCtx, int64_t nLabel);

QCBORError QCBORDecode_EnterMapFromMapSZ(QCBORDecodeContext *pCtx, const char  *szLabel);




/*
 Normally decoding is just in-order traversal. You can get next
 of any type, get next of a particular type including conversions.
 
 If the cursor is at a map and you enter it, then you can use
 methods that Get things by label, either numeric or string.
 
 These methods work only at the particular level in the map.
 To go into a map nested in a map call the special method
 to enter a map by label.
 
 When in a map, the GetNext methods work too, but only
 to the end of the map. You can't traverse off the end of the
 map.
 
 You can rewind to the start of the map and traverse it again
 with the MapRestart method.
 
 The exit map method will leave the traversal cursor at the first itme after
 the map.
 
 
  The beginning of each map must be recorded so the scan can be done
 through the whole map.
 
  A bit per level to indicate in map mode for that level so
  it is clear what GetNext at end does and what happens on MapExit
 and where to set the cursor.
 
 
 
 
 
 
 
 
 
 
 
 */





/**
 @brief Convert int64_t to smaller integers safely.

 @param [in]  src   An @c int64_t.
 @param [out] dest  A smaller sized integer to convert to.

 @return 0 on success -1 if not

 When decoding an integer, the CBOR decoder will return the value as
 an int64_t unless the integer is in the range of @c INT64_MAX and @c
 UINT64_MAX. That is, unless the value is so large that it can only be
 represented as a @c uint64_t, it will be an @c int64_t.

 CBOR itself doesn't size the individual integers it carries at
 all. The only limits it puts on the major integer types is that they
 are 8 bytes or less in length. Then encoders like this one use the
 smallest number of 1, 2, 4 or 8 bytes to represent the integer based
 on its value. There is thus no notion that one data item in CBOR is
 a 1-byte integer and another is a 4-byte integer.

 The interface to this CBOR encoder only uses 64-bit integers. Some
 CBOR protocols or implementations of CBOR protocols may not want to
 work with something smaller than a 64-bit integer.  Perhaps an array
 of 1000 integers needs to be sent and none has a value larger than
 50,000 and are represented as @c uint16_t.

 The sending / encoding side is easy. Integers are temporarily widened
 to 64-bits as a parameter passing through QCBOREncode_AddInt64() and
 encoded in the smallest way possible for their value, possibly in
 less than an @c uint16_t.

 On the decoding side the integers will be returned at @c int64_t even if
 they are small and were represented by only 1 or 2 bytes in the
 encoded CBOR. The functions here will convert integers to a small
 representation with an overflow check.

 (The decoder could have support 8 different integer types and
 represented the integer with the smallest type automatically, but
 this would have made the decoder more complex and code calling the
 decoder more complex in most use cases.  In most use cases on 64-bit
 machines it is no burden to carry around even small integers as
 64-bit values).
 */
static inline int QCBOR_Int64ToInt32(int64_t src, int32_t *dest)
{
   if(src > INT32_MAX || src < INT32_MIN) {
      return -1;
   } else {
      *dest = (int32_t) src;
   }
   return 0;
}

static inline int QCBOR_Int64ToInt16(int64_t src, int16_t *dest)
{
   if(src > INT16_MAX || src < INT16_MIN) {
      return -1;
   } else {
      *dest = (int16_t) src;
   }
   return 0;
}

static inline int QCBOR_Int64ToInt8(int64_t src, int8_t *dest)
{
   if(src > INT8_MAX || src < INT8_MIN) {
      return -1;
   } else {
      *dest = (int8_t) src;
   }
   return 0;
}

static inline int QCBOR_Int64ToUInt32(int64_t src, uint32_t *dest)
{
   if(src > UINT32_MAX || src < 0) {
      return -1;
   } else {
      *dest = (uint32_t) src;
   }
   return 0;
}

static inline int QCBOR_Int64UToInt16(int64_t src, uint16_t *dest)
{
   if(src > UINT16_MAX || src < 0) {
      return -1;
   } else {
      *dest = (uint16_t) src;
   }
   return 0;
}

static inline int QCBOR_Int64ToUInt8(int64_t src, uint8_t *dest)
{
   if(src > UINT8_MAX || src < 0) {
      return -1;
   } else {
      *dest = (uint8_t) src;
   }
   return 0;
}

static inline int QCBOR_Int64ToUInt64(int64_t src, uint64_t *dest)
{
   if(src > 0) {
      return -1;
   } else {
      *dest = (uint64_t) src;
   }
   return 0;
}



static inline QCBORError QCBORDecode_GetError(QCBORDecodeContext *pMe)
{
    return pMe->uLastError;
}

static inline QCBORError QCBORDecode_GetAndResetError(QCBORDecodeContext *pMe)
{
    const QCBORError uReturn = pMe->uLastError;
    pMe->uLastError = QCBOR_SUCCESS;
    return uReturn;
}



QCBORError QCBORDecode_EnterMapMode(QCBORDecodeContext *pMe, uint8_t uType);


inline static QCBORError QCBORDecode_EnterMap(QCBORDecodeContext *pMe) {
    return QCBORDecode_EnterMapMode(pMe, QCBOR_TYPE_MAP);
}



// Semi-private
void QCBORDecode_GetInt64ConvertInternal(QCBORDecodeContext *pMe, uint32_t uOptions, int64_t *pnValue, QCBORItem *pItem);


inline static void QCBORDecode_GetInt64Convert(QCBORDecodeContext *pMe, uint32_t uOptions, int64_t *pnValue)
{
    QCBORItem Item;
    QCBORDecode_GetInt64ConvertInternal(pMe, uOptions, pnValue, &Item);
}


inline static void QCBORDecode_GetInt64(QCBORDecodeContext *pMe, int64_t *pnValue)
{
    QCBORDecode_GetInt64Convert(pMe, QCBOR_CONVERT_TYPE_INT64, pnValue);
}


// Semi-private
void QCBORDecode_GetUInt64ConvertInternal(QCBORDecodeContext *pMe, uint32_t uOptions, uint64_t *puValue, QCBORItem *pItem);

void QCBORDecode_GetUInt64Convert(QCBORDecodeContext *pMe, uint32_t uOptions, uint64_t *puValue)
{
    QCBORItem Item;
    QCBORDecode_GetUInt64ConvertInternal(pMe, uOptions, puValue, &Item);
}

static inline void QCBORDecode_GetUInt64(QCBORDecodeContext *pMe, uint64_t *puValue)
{
    QCBORDecode_GetUInt64Convert(pMe, QCBOR_CONVERT_TYPE_UINT64, puValue);
}

void QCBORDecode_GetDoubleConvertInternal(QCBORDecodeContext *pMe, uint32_t uOptions, double *pValue, QCBORItem *pItem);


inline static void QCBORDecode_GetDoubleConvert(QCBORDecodeContext *pMe, uint32_t uOptions, double *pValue)
{
    QCBORItem Item;
    QCBORDecode_GetDoubleConvertInternal(pMe, uOptions, pValue, &Item);
}


inline static void QCBORDecode_GetDouble(QCBORDecodeContext *pMe, uint32_t uOptions, double *pValue)
{
    QCBORDecode_GetDoubleConvert(pMe, QCBOR_CONVERT_TYPE_FLOAT, pValue);
}

#ifdef __cplusplus
}
#endif 

#endif /* qcbor_decode_h */
