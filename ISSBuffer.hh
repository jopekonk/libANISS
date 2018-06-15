#ifndef __ISS_BUFFER_HH__
#define __ISS_BUFFER_HH__

#include <Rtypes.h> // For root types
#include "ISSHeader.hh" // For DATA_HEADER definition

class ISSBuffer {

 private:
   Char_t *ptr;           // Pointer to block
   DATA_HEADER *header; // Pointer to header
   ULong64_t *data;      // Pointer to data
   UInt_t nwords;        // Number of 64-bit data words in data
   Int_t swap;            // Swapping mode

   // enumeration for errors
   enum err_t {
       ERR_BAD_HEADER = -1,  // Invalid header
   };

   // enumeration for swapping modes
   enum swap_t {
       SWAP_KNOWN  = 1,  // We know the swapping mode already
       SWAP_WORDS  = 2,  // We need to swap pairs of 32-bit words
       SWAP_ENDIAN = 4   // We need to swap endianness
   };
   
   //..........................................................................
   // Swap endianness of a 32-bit integer 0x01234567 -> 0x67452301
   UInt_t Swap32(UInt_t datum) {
       return((  (datum & 0xFF000000) >> 24) |
                ((datum & 0x00FF0000) >>  8) |
                ((datum & 0x0000FF00) <<  8) |
                ((datum & 0x000000FF) << 24));
   }
   
   //..........................................................................
   // Swap the two halves of a 64-bit integer 0x0123456789ABCDEF ->
   // 0x89ABCDEF01234567
   ULong64_t SwapWords(ULong64_t datum) {
       return(((datum & 0xFFFFFFFF00000000LL) >> 32) |
              ((datum & 0x00000000FFFFFFFFLL) << 32));
   }

   //..........................................................................
   // Swap endianness of a 64-bit integer 0x0123456789ABCDEF ->
   // 0xEFCDAB8967452301
   ULong64_t Swap64(ULong64_t datum) {
       return(((  datum & 0xFF00000000000000LL) >> 56) |
                ((datum & 0x00FF000000000000LL) >> 40) |
                ((datum & 0x0000FF0000000000LL) >> 24) |
                ((datum & 0x000000FF00000000LL) >>  8) |
                ((datum & 0x00000000FF000000LL) <<  8) |
                ((datum & 0x0000000000FF0000LL) << 24) |
                ((datum & 0x000000000000FF00LL) << 40) |
                ((datum & 0x00000000000000FFLL) << 56));
   }

 public:

   //..........................................................................
   // Constructor
   ISSBuffer(Char_t *_ptr = NULL) {
       swap = 0;
       Set(_ptr);
   };
   
   //..........................................................................
   // Set up the buffer
   void Set(Char_t *_ptr) {

       ptr = _ptr;
       header = NULL;
       data = NULL;
       nwords = 0;

       // Safety check
       if (!ptr) return;
       
       // Set up pointers
       ptr = _ptr;
       header = (DATA_HEADER *)ptr;
       data = (ULong64_t *)(ptr + sizeof(DATA_HEADER));

       // Check header is valid
       if (strncmp(header->id, "EBYEDATA", 8)) throw(ERR_BAD_HEADER);
       
       // Determine the number of 64-bit words
       nwords = header->dataLen;
       if (header->MyEndian != 1) nwords = Swap32(nwords);
       nwords /= sizeof(ULong64_t);

       // If we already know the mode, that's all
       if (swap & SWAP_KNOWN) return;

       // See if we can figure out the swapping - the DataEndian word of the
       // header is 1 if the endianness is correct, otherwise swap endianness
       if (header->DataEndian != 1) swap |= SWAP_ENDIAN;

       // However, that is not all, the words may also be swapped, so check
       // for that. Bits 31:28 should always be zero in the timestamp word
       for (UInt_t i = 0; i < nwords; i++) {
           ULong64_t word = (swap & SWAP_ENDIAN) ? Swap64(data[i]) : data[i];
           if (word & 0xF000000000000000LL) {
               swap |= SWAP_KNOWN;
               break;
           }
           if (word & 0x00000000F0000000LL) {
               swap |= SWAP_KNOWN;
               swap |= SWAP_WORDS;
               break;
           }
       }
   };

   //..........................................................................
   // Get number of 64-bit words in data
   inline UInt_t GetNWords() {
       return(nwords);
   };

   //..........................................................................
   // Get nth word
   inline ULong64_t GetWord(UInt_t n = 0) {
       
       // If word number is out of range, return zero
       if (n >= nwords) return(0);

       // Perform byte swapping according to swap mode
       ULong64_t result = data[n];
       if (swap & SWAP_ENDIAN) result = Swap64(result);
       if (swap & SWAP_WORDS)  result = SwapWords(result);
       return(result);
   };

   //..........................................................................
   // Show some information for debugging purposes
   void Show(UInt_t level = 1) {
       if (level < 1) return;
       if (!ptr) {
           printf("No block attached\n");
           return;
       }
       printf("Block %d has %d 64-bit words - ", header->sequence, nwords);
       switch(swap) {
        case SWAP_KNOWN:
           printf("unswapped");
           break;
        case (SWAP_KNOWN | SWAP_ENDIAN):
           printf("swapped endianness");
           break;
        case (SWAP_KNOWN | SWAP_WORDS):
           printf("swapped pairs of 32-bit words");
           break;
        case (SWAP_KNOWN | SWAP_WORDS | SWAP_ENDIAN):
           printf("swapped endianness and pairs of 32-bit words");
           break;
        default:
           printf("unknown byte order");
           break;
       }
       printf("\n");
       if (level < 2) return;
       for (UInt_t i = 0; i < nwords; i++)
          printf("Block %-5d word %-5d is 0x%012llX\n", header->sequence, i, GetWord(i));
   };
};

#endif
