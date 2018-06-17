#ifndef __ISS_HIT_HH__
#define __ISS_HIT_HH__

#include <Rtypes.h> // For root types
#include <vector>

class ISSHit {

private:
    ULong64_t ts; // Full 48-bit timestamp from the CAEN ADC
    UInt_t  conversion; // ADC conversion
    Short_t module;  // ADC Module number
    Short_t channel; // ADC Channel number
    Short_t data_id; // ADC datum data ID, QLong = 0, QShort = 1, FineTiming = 3
    //The Data Id will be dependent on the particular CAEN module and firmware option.
    //PHA =0 energy; =2 baseline; = 3 fine timing
    //PSD =0 Qlong; =1 Qshort; =2 baseline; = 3 fine timing <--- at ISS !
    
    std::vector <UShort_t> trace;

public:
    //..........................................................................
    // Constructor
    ISSHit(UShort_t _module = -1, UShort_t _channel = -1, ULong64_t _ts = -1, 
             UInt_t _conversion = -1, UShort_t _data_id = -1 ) {
        Set(_module, _channel, _ts, _conversion, _data_id);
    }

    //..........................................................................
    // Set values
    void Set(UShort_t _module, UShort_t _channel, ULong64_t _ts, UInt_t _conversion,
                UShort_t _data_id) {
        module = _module;
        channel = _channel;
        ts = _ts;
        conversion = _conversion;
        data_id = _data_id;
        trace.clear();
    }

    //..........................................................................
    // Add a sample to the trace
    void AddSample(UShort_t _word) {
        trace.push_back(_word);
    };
    
    //..........................................................................
    // Get Module
    inline UShort_t GetModule() {
        return(module);
    };

    //..........................................................................
    // Get ADC channel number
    inline UShort_t GetChannel() {
        return(channel);
    };

    //..........................................................................
    // Get timestamp
    inline ULong64_t GetTimestamp() {
        return(ts);
    };

    //..........................................................................
    // Get conversion
    inline UInt_t GetConversion() {
        return(conversion);
    };

    //..........................................................................
    // Get data ID
    inline UShort_t GetDataID() {
        return(data_id);
    };

    //..........................................................................
    // Get number of samples in trace
    inline UInt_t GetNSamples() {
        return(trace.size());
    };
    
    //..........................................................................
    // Get trace
    inline UInt_t GetSample(UInt_t i = 0) {
        if (i >= trace.size()) return(0);
        return(trace[i]);
    };

    //..........................................................................
    // Get trace
    inline const UShort_t *GetTrace() {
        return(&trace[0]);
    };
    
    //..........................................................................
    // Comparison operator for sorting
    bool operator< (const ISSHit rhs) const {
        return(ts < rhs.ts);
    };

    //..........................................................................
    // Show some information for debugging
    void Show(UInt_t level = 1) {
        if (level < 1) return;
        printf("MODULE %-4d CHANNEL %-4d DATAID %-4d TS 0x%012llX Conversion %04d\n",
                 module, channel, data_id, ts, conversion);
    }
};

#endif
