#ifndef __ISS_HEADER_HH__
#define __ISS_HEADER_HH__

// The header length is 24 bytes.
// For details see:
// http://ns.ph.liv.ac.uk/MTsort-manual/TSformat.html
//
// Note however, they assume that "long" is only 32 bits, which is incorrect
// on 64-bit systems!
typedef struct s_data_header {
    Char_t   id[8];        // contains the string  EBYEDATA
    UInt_t   sequence;   // within the file
    UShort_t stream;      // data acquisition stream number (in the range 1=>4)
    UShort_t tape;        // =1
    UShort_t MyEndian;   // written as a native 1 by the tape server
    UShort_t DataEndian; // written as a native 1 in the hardware structure of $
    UInt_t   dataLen;     // total length of useful data following the header in$
} DATA_HEADER;

#endif
