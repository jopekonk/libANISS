// Script to determine the statistics for each channel in an ISS data file.

#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

#include <TFile.h>
#include <TH1I.h>
#include <TH1F.h>
#include <TString.h>
#include <TAxis.h>
#include <TRandom.h>

#include "ISSBuffer.hh"
#include "ISSFile.hh"
#include "ISSWord.hh"

//#define MAXID 0x1000        // Maximum number of IDs with 12 bits
#define MAXID 100

// Histograms
TH1I *hStats, *hstatQLong, *hstatQShort;
TH1F *hQLong[MAXID], *hQShort[MAXID], *hglobalTSdiff, *hADCTSdiff;
TRandom *fRand = new TRandom();

// Timestamps and statistics
ULong64_t first_adc_ts = 0, first_global_ts = 0, last_adc8_ts, last_adc16_ts,
          last_global_ts, prev_global_ts=0, prev_adc8_ts=0, prev_adc16_ts=0, 
          global_diff=0, adc_diff=0;
ULong64_t n_ebis_pulses = 0, n_info=0, n_adc =0, n_word=0, n_qlong=0, n_qshort=0, 
          n_finetime=0, n_traces=0, n_adc_ts=0, n_global_ts=0;



//-----------------------------------------------------------------------------
// Treat a single word
void treat_word(ISSWord *w) {

    n_word++;
    
    // If it has an extended timestamp get the timestamp
    if (w->HasExtendedTimestamp()) {
        if (63 == w->GetInfoModule()) {
            prev_global_ts = last_global_ts;
            last_global_ts = w->GetFullGlobalTimestamp();
            if (last_global_ts < prev_global_ts) printf("ERROR: New TS < old TS");
            global_diff = last_global_ts - prev_global_ts;
            n_global_ts++;
        } else if ( 2 > w->GetInfoModule() ) {
            prev_adc8_ts = last_adc8_ts;
            last_adc8_ts = w->GetFullADCTimestamp();
            adc_diff = last_adc8_ts-prev_adc8_ts;
            hADCTSdiff->Fill(adc_diff*8); // 8 ns resolution in V1725
            n_adc_ts++;
        }
        else {
          prev_adc16_ts = last_adc16_ts;
          last_adc16_ts = w->GetFullADCTimestamp();
          adc_diff = last_adc16_ts-prev_adc16_ts;
          hADCTSdiff->Fill(adc_diff*16); // 16 ns resolution in V1730
          n_adc_ts++;
        }          
    }
        
    if (!first_adc_ts) first_adc_ts = last_adc8_ts;
    if (!first_global_ts) first_global_ts = last_global_ts;
    
    // For info words get errors
    if (w->IsInfo()) {
        n_info++;
        UInt_t code    = w->GetInfoCode();
        UInt_t field  = w->GetInfoField();
        UInt_t module = w->GetInfoModule();
        UInt_t id      = module;
        
        // TS from V1495 module
        if (63 == module) {
            //hglobalTSdiff->AddBinContent(global_diff, 1);
            hglobalTSdiff->Fill(global_diff*10.e-9);
            n_ebis_pulses++;            
        }
        
        return; // info word has been processed
    }
    
    // For ADC words get statistics
    if (w->IsADC()) {
        n_adc++;
        UInt_t module = w->GetADCModule();
        UInt_t channel = w->GetADCChannel();
        UInt_t adc_data = w->GetADCConversion();
        UInt_t id = 32*module + channel;
        hStats->AddBinContent(id, 1);
        //float raw = adc_data + 0.5 - fRand->Uniform();
        
        if (w->IsQLong()) { 
            hQLong[id]->Fill(adc_data); 
            hstatQLong->AddBinContent(id, 1);
            n_qlong++;
        }
        if (w->IsQShort()) { 
            hQShort[id]->Fill(adc_data); 
            hstatQShort->AddBinContent(id, 1);
            n_qshort++;
        }
        if (w->IsFineTiming()) {             
            n_finetime++;
        }
        if (w->IsSample()) {             
            n_traces++;
        }
    }
 }

//-----------------------------------------------------------------------------
// Treat a single buffer
void treat_buffer(ISSBuffer *b) {

    static ISSWord w;

    // Loop over words in buffer
    for (UInt_t i = 0; i < b->GetNWords(); i++) {
        
        // Set up word
        w.Set(b->GetWord(i));

        // Treat the word
        treat_word(&w);
    }
}

//-----------------------------------------------------------------------------
// Treat a single file
void treat_file(const Char_t *filename) {

    ISSBuffer b;

    // Open file
    ISSFile f(filename);
    
    // Loop over blocks
    for (UInt_t i = 0; i < f.GetNBlocks(); i++) {

        // Set up the buffer with that block
        b.Set(f.GetBlock(i));

        // Treat the buffer
        treat_buffer(&b);
    }

    // Close file
    f.Close();
}

//-----------------------------------------------------------------------------
// Get statistics for a file
void stats(const Char_t *infile = "../../ISS_R2_8") {     
     
     // Open output file
    TFile *f = TFile::Open("stats.root", "recreate");
    if (!f) return;

    // Create histograms
    hStats        = new TH1I("hStats",     "Total statistics", MAXID, 0, MAXID);
    hstatQLong    = new TH1I("hstatQLong", "QLong statistics", MAXID, 0, MAXID);
    hstatQShort   = new TH1I("hstatQShort","QShort statistics", MAXID, 0, MAXID);
    
    hglobalTSdiff = new TH1F("hglobalTSdiff" ,"Difference between trigger timestamps in seconds", 10000, 0, 10);
    hADCTSdiff    = new TH1F("hADCTSdiff" ,   "Difference between ADC timestamps in ns", 10000, 0, 10000);
    
    for (UInt_t i = 0; i < MAXID; i++) {
      hQLong[i] = new TH1F( Form("hQLong%04d", i) ,"QLong spectrum", 65536, 0, 65536);
      hQShort[i] = new TH1F(Form("hQShort%04d", i),"QShort spectrum", 65536, 0, 65536);
    }
    // Treat the file
    treat_file(infile);
    

    // Get time difference between first and last timestamp
    Double_t diff = (Double_t)(last_global_ts - first_global_ts);
    diff *= 10e-9; // Convert to seconds

    // Write statistics
    printf("Acquisition time: %.3f seconds\n", diff);
    printf("Number data words: %llu\n", n_word);
    printf("Number info words: %llu\n", n_info);
    printf("Number global ts: %llu\n", n_global_ts);
    printf("Number    adc ts: %llu\n", n_adc_ts);
    printf("Number  adc words: %llu\n", n_adc);
    printf("Number  QL  words: %llu\n", n_qlong);
    printf("Number  QS  words: %llu\n", n_qshort);
    printf("Number  FT  words: %llu\n", n_finetime);
    printf("Number Trace words: %llu\n", n_traces);
    printf("Number  QL+QS+FT  words: %llu\n", n_qlong+n_qshort+n_finetime);
    printf("Number of EBIS pulses (readout timestamps): %llu\n", n_ebis_pulses);
    printf("ID     Total        QLong      QShort  Rate [/s]\n");
    for (UInt_t i = 0; i < MAXID; i++) {
        UInt_t integral    = hStats->GetBinContent(i);
        UInt_t qlong       = hstatQLong->GetBinContent(i);
        UInt_t qshort      = hstatQShort->GetBinContent(i);
        if (integral <= 0) continue;
        printf("%-5d %-10d %-10d %-10d %-10.3f\n", i, integral,
                 qlong, qshort, 
                 (Double_t)integral / diff);
    }
    // Write everything
    f->Write();
    f->Close();

}

