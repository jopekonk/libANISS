#ifndef __ISS_WORD_HH__
#define __ISS_WORD_HH__

#define CAEN_V1495_MOD_ID 63

class ISSWord {

 private:
    ULong64_t word;
    ULong64_t last_global_ts;
    ULong64_t last_adc_ts;
    UInt_t ext_global_ts; // Global timestamp from logic unit CAEN V1495  (10 ns resolution)
    UInt_t ext_adc_ts;    // ADC timestamp from ADC unit :    CAEN V1725 ( 8 ns resolution) 
                          //                                    or V1730 (16 ns resolution)

 public:
    
    //..........................................................................
    // Constructor
    ISSWord(ULong64_t _word = 0) {
        ext_global_ts = 0;
        ext_adc_ts = 0;
        Set(_word);
    };

    //..........................................................................
    // Set method
    void Set(ULong64_t _word = 0) {
        word = _word;        
        if (HasExtendedTimestamp()) {
            UShort_t mod_id = GetInfoModule();
            if (CAEN_V1495_MOD_ID == mod_id) ext_global_ts = GetInfoField();
               else ext_adc_ts = GetInfoField();
        }
    };

    //..........................................................................
    // Get the word
    inline ULong64_t GetWord() {
        return(word);
    };
    
    //..........................................................................
    // Get the key (this should always be 2 = TIMESTAMP)
    inline UShort_t GetKey() {
        return((word >> 62) & 3);
    };
    
    //..........................................................................
    // Is it a trace sample??
    inline Bool_t IsSample() {
        return(((word >> 62) & 3) == 0);
    };
    
    //..........................................................................
    // Is it a trace header?
    inline Bool_t IsTraceHeader() {
        return(((word >> 62) & 3) == 1);
    };
    
    //..........................................................................
    // Is it an information word?
    inline Bool_t IsInfo() {
        return(((word >> 62) & 3) == 2);
    };
    
    //..........................................................................
    // Is it an ADC word?
    inline Bool_t IsADC() {
        return(((word >> 62) & 3) == 3);
    };
    
    //..........................................................................
    // Is it an QLong ADC word (PSD) or Energy (PHA)
    inline Bool_t IsQLong() {
        if (!IsADC()) return(kFALSE);
        return( GetADCDataID() == 0 );
    };
    
    //..........................................................................
    // Is it an QShort ADC word (PSD) 
    inline Bool_t IsQShort() {
        if (!IsADC()) return(kFALSE);
        return( GetADCDataID() == 1);
    };
    
    //..........................................................................
    // Is it an Fine Timing ADC word
    inline Bool_t IsFineTiming() {
        if (!IsADC()) return(kFALSE);
        return( GetADCDataID() == 3 );
    };
    
    //..........................................................................
    // Get lowest 28-bits of timestamp
    inline UInt_t GetLowTimestamp() {
        if (!IsInfo()) return(0);
        return(word & 0xFFFFFFF);
    };

    //..........................................................................
    // Has extended timestamp?
    inline Bool_t HasExtendedTimestamp() {
        if (!IsInfo()) return(kFALSE);
        UInt_t code = GetInfoCode();
        if (code == 4) return(kTRUE);
        return(kFALSE);
    };

    //..........................................................................
    // Set the extended part of the global timestamp
    void SetGlobalExtendedTimestamp(UInt_t _ext_ts) {
        ext_global_ts = _ext_ts;
    };
    
    //..........................................................................
    // Set the extended part of the adc timestamp
    void SetADCExtendedTimestamp(UInt_t _ext_ts) {
        ext_adc_ts = _ext_ts;
    };
    
    //..........................................................................
    // Get the full global 48-bit timestamp 
    inline ULong64_t GetFullGlobalTimestamp() {
        if (IsSample()) return(last_global_ts);
        ULong64_t ts = (ULong64_t)ext_global_ts;
        ts <<= 28;
        ts |= (word & 0xFFFFFFF);
        last_global_ts = ts;
        return(ts);
    };
    
    //..........................................................................
    // Get the full ADC 48-bit timestamp 
    inline ULong64_t GetFullADCTimestamp() {
        if (IsSample()) return(last_adc_ts);
        ULong64_t ts = (ULong64_t)ext_adc_ts;
        ts <<= 28;
        ts |= (word & 0xFFFFFFF);
        last_adc_ts = ts;
        return(ts);
    };


    //..........................................................................
    // Get field from an information word (full timestamp)
    inline UInt_t GetInfoField() {
        if (!IsInfo()) return(0);
        return((word >> 32) & 0xFFFFF);
    }

    //..........................................................................
    // Get code from an info code from information word (bits 23:20, word 2)
    inline UInt_t GetInfoCode() {
        if (!IsInfo()) return(0);
        return((word >> 52) & 0xF);
    }
    
    //..........................................................................
    // Get data ID code from an ADC word (bits 23:22, word 2)
    inline UShort_t GetADCDataID() {
        if (!IsADC()) return(0);
        return((word >> 54) & 0x03);
    }
    
    //..........................................................................
    // Get module from an information word (bits 29:24, word 2)
    inline UInt_t GetInfoModule() {
        if (!IsInfo()) return(0);
        return((word >> 56) & 0x3F);
    };

    //..........................................................................
    // Get conversion from ADC
    inline UInt_t GetADCConversion() {
        if (!IsADC()) return(0);
        return((word >> 32) & 0xFFFF);
    };
    
    //..........................................................................
    // Get ID from ADC ( 28:24 mod number, 23:22 data ID, 21:16 channel, word 2)
    inline UInt_t GetADCID() {
        if (!IsADC()) return(0);
        return((word >> 48) & 0xFFF);
    };
    
    //..........................................................................
    // Get channel from ADC (bits 21:16, word 2)
    inline UInt_t GetADCChannel() {
        if (!IsADC()) return(0);
        return(((word >> 48) & 0x3F) );
    };
    
    //..........................................................................
    // Get baseline/energy bit
    inline Bool_t GetADCBE() {
        if (!IsADC()) return(0);
        return((word >> 52) & 1);
    };
    
    //..........................................................................
    // Get module from ADC (bits 28:24, word 2)
    inline UInt_t GetADCModule() {
        if (!IsADC()) return(0);
        return((word >> 56) & 0x1F);
    };
    
    
    //..........................................................................
    // Get number of samples from trace header
    // The sample length defines the number of 14 bit sample data items 
    // following and will be a multiple of 4.
    inline UInt_t GetTraceNSamples() {
        if (!IsTraceHeader()) return(0);
        return((word >> 32) & 0xFFFF);
    };
    
    //..........................................................................
    // Get ID from trace header (13 bits 28:16 channel ident, word 2)
    inline UInt_t GetTraceID() {
        if (!IsTraceHeader()) return(0);
        return((word >> 48) & 0x1FFF);
    };
    
    //..........................................................................
    // Get channel from trace header (6 bits 21:16, word 2)
    inline UInt_t GetTraceChannel() {
        if (!IsTraceHeader()) return(0);
        return(((word >> 48) & 0x3F));
    };
    
    //..........................................................................
    // Get baseline/energy bit (Not used at ISS?)
    inline Bool_t GetTraceBE() {
        if (!IsADC()) return(0);
        return((word >> 52) & 1);
    };
    
    //..........................................................................
    // Get module from trace header (5 bits 28:24, word 2)
    inline UInt_t GetTraceModule() {
        if (!IsTraceHeader()) return(0);
        return((word >> 56) & 0x1F);
    };

    //..........................................................................
    // Get trace/raw flag (Not used at ISS?)
    inline UInt_t GetTraceRawFlag() {
        if (!IsTraceHeader()) return(kFALSE);
        return((word >> 52) & 1);
    };
    
    //..........................................................................
    void Show(UInt_t level = 1) {
        const Char_t *keys[] = {"SAMPLE", "TRACE", "INFO", "ADC"};
        const Char_t *infos[] = {"UNDEFINED", "Pile-up", "Pause", "Resume",
            "TS", "WhiteRabbit", "Aida", "ExtTs", "ScanningTable",
            "OverRange", "UnderRange", "Overflow", "Underflow", "TrigSeq",
            "DataLink", "SHARC"};
        UInt_t key = GetKey();
        UInt_t code = GetInfoCode();
        if (level < 1) return;
        printf("Word: 0x%016llX Timestamp: 0x%012llX Type: %-6s \t",
                 word, GetFullADCTimestamp(), keys[key]);
        switch(key) {
         case 0:
            break;
         case 1:
            printf("Module: %d Channel: %-2d %c Nsamples: %d %s",
                     GetTraceModule(), GetTraceChannel(), GetTraceBE() ? 'B' : 'E',
                     GetTraceNSamples(), GetTraceRawFlag() ? "RAW" : "TRACE");
            break;
         case 2:
            printf("Module: %d Code: %d %-8s Field: 0x%05X", GetInfoModule(),
                     code, infos[code], GetInfoField());
            break;
         case 3:
            printf("Module: %d Channel: %-2d %c Conversion: %4d",
                     GetADCModule(), GetADCChannel(), GetADCBE() ? 'B' : 'E',
                     GetADCConversion());
            break;
        }
        printf("\n");
    }
};

#endif