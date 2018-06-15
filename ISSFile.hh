#ifndef __ISS_FILE_HH__
#define __ISS_FILE_HH__

#include <TObject.h>
#include <Rtypes.h> // For root types
#include <vector>
#if !defined (__CINT__)
#include <sys/mman.h>
#endif

#include "ISSHeader.hh" // For DATA_HEADER definition

class ISSFile : public TObject {

 private:
   FILE *fp; //! The //! is to keep rootcint happy
   Char_t *ptr; //! 
   ULong64_t len;
   UInt_t blocksize;
   
   // enumeration for errors
   enum err_t {
          ERR_OPEN = -1,            // Unable to open file
          ERR_MAP = -2,             // Unable to map file into memory
          ERR_BAD_FILE = -3,       // File does not have first header
          ERR_BAD_BLOCK_SIZE = -4 // Unable to determine block size
   };

   //..........................................................................
   // Determine the size of the blocks.
   void DetermineBlockSize() {

       // Get a pointer to the first header
       DATA_HEADER *header = (DATA_HEADER *)ptr;

       // Safety check - at least the first header must be EBYEDATA
               if (strncmp(header->id, "EBYEDATA", 8)) {
           fprintf(stderr, "File does not have EBYEDATA header\n");
           throw(ERR_BAD_FILE);
       }

       // Loop over possible block sizes
       for (blocksize = 1 << 8; blocksize < len; blocksize <<= 1) {

           // Get a pointer to the second header in the file, assuming
           // this blocksize
           header = (DATA_HEADER *)(ptr + blocksize);

           // If it is EBYEDATA, this assumption of blocksize must be the
           // right one
           if (!strncmp(header->id, "EBYEDATA", 8)) return;
       }

       // Could not determine block size
       fprintf(stderr, "Unable to determine block size\n");
       throw(ERR_BAD_BLOCK_SIZE);
   };

 public:
   
   //..........................................................................
   // Constructor
   ISSFile(const Char_t *_filename = NULL) {
       len = 0;
       ptr = NULL;
       fp = NULL;
       blocksize = 0;
       // If we were given a filename, open it
       if (_filename) Open(_filename);
   };
   
   //..........................................................................
   // Destructor
   ~ISSFile() {
       Close();
   };
   
   //..........................................................................
   // Open a file and map it into memory
   void Open(const Char_t *_filename) {

       // Force close
       Close();

       // Open file
       fp = fopen(_filename, "rb");
       if (!fp) {
           fprintf(stderr, "Unable to open file %s - %m\n", _filename);
           throw(ERR_OPEN);
       }

       // Get length of file
       fseek(fp, 0, SEEK_END);
       len = (ULong64_t)ftell(fp);
       fseek(fp, 0, SEEK_CUR);

       // Map file
       ptr = (Char_t *)mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(fp), 0);
       if (ptr == MAP_FAILED) {
           fprintf(stderr, "Unable to map file %s - %m\n", _filename);
           fclose(fp);
           throw(ERR_MAP);
       }

       // Determine the block size
       DetermineBlockSize();
   };
   
   //..........................................................................
   // Close the file after unmapping it
   void Close() {
       
       // Unmap
       if (ptr) munmap(ptr, len);
       ptr = NULL;
       len = 0;
       blocksize = 0;

       // Close
       if (fp) fclose(fp);
       fp = NULL;
   };

   //..........................................................................
   // Get total size of file (in bytes)
   inline ULong64_t GetFileSize() {
       return(len);
   };

   //..........................................................................
   // Get block size
   inline UInt_t GetBlockSize() {
       return(blocksize);
   };
   
   //..........................................................................
   // Get total number of blocks in file
   inline UInt_t GetNBlocks() {
       return(len / blocksize);
   };

   //..........................................................................
   // Get the n'th block
   Char_t *GetBlock(UInt_t n = 0) {
       if (n >= len / blocksize) return(NULL);
       return(ptr + n * blocksize);
   };

   //..........................................................................
   // Write some information for debugging purposes
   void Show() {
       if (!fp) {
           printf("No file opened\n");
           return;
       }
       printf("File has %lld bytes\n", len);
       printf("File has %lld blocks of %u bytes = %u bytes\n", len / blocksize,
                blocksize, (UInt_t)(len / blocksize) * blocksize);
   };

   ClassDef(ISSFile, 1)
};

#endif
