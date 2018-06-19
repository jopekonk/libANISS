// Script to make a root tree of ISS data file(s).
// Groups data items into events separated by the EBIS pulses 
// from the V1495 logic unit. Time orders the adc items within the event.
// Joonas Konki - 20180617
//
// TODO: treat last bunch of data items in a block properly if there is no
//       EBIS trigger following the last adc data words (?)
//

#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

#include <TFile.h>
#include <TTree.h>
#include <TH1I.h>
#include <TH1F.h>
#include <TString.h>
#include <TAxis.h>
#include <TRandom.h>

#include "ISSBuffer.hh"
#include "ISSFile.hh"
#include "ISSWord.hh"
#include "ISSHit.hh"

#define MAXID 100
#define MAXHITS 1000000 // Maximum number of hits allowed in the event storage
#define ID_EBIS 63      // Module number of the EBIS trigger pulse
#define ID_V1730 2      // V1730 module with 16 ns time resolution

// Tree entry definition
struct struct_tree_entry {
    unsigned long long global_event_ts; //full V1495 48-bit timestamp of the event (10 ns resolution)
    unsigned long long adc_ts; //full 48-bit timestamp from a V1725 module (8 ns resolution) 
    unsigned int       module; // module number
    unsigned int       channel; // channel number  
    unsigned short     data_id; // QLong = 1, QShort = 2, FineTiming = 3
    unsigned int       adc_data; // ADC conversion
  };
  
// Storage of hits within one event
std::vector <ISSHit> hits;
TTree *tree;
struct_tree_entry issentry;

// Histograms
TH1I *hStats, *hstatQLong, *hstatQShort;
TRandom *fRand = new TRandom();

// Timestamps and statistics
ULong64_t first_adc8_ts = 0, first_global_ts=0, last_adc8_ts=0, last_adc16_ts=0,
          last_global_ts = 0, new_global_ts=0, prev_adc8_ts=0, prev_adc16_ts=0, adc_diff=0;
ULong64_t n_ebis_pulses = 0, n_info=0, n_adc =0, n_word=0, n_qlong=0, n_qshort=0, 
          n_finetime=0, n_traces=0, n_adc_ts=0, n_global_ts=0, n_processed_hits=0,
          n_events=0, nbuffer=0, total_buffer=0;

//-----------------------------------------------------------------------------
// Treat a single hit
void treat_hit(ISSHit *hit, ULong64_t event_ts) {
    //if (n_ebis_pulses < 2) hit->Show(); // For debugging
    
    // Write entry to root tree
    issentry.global_event_ts = event_ts;
    issentry.adc_ts = hit->GetTimestamp();
    issentry.module = hit->GetModule();
    issentry.channel = hit->GetChannel();
    issentry.data_id = hit->GetDataID();
    issentry.adc_data = hit->GetConversion();
    tree->Fill();   
}

//-----------------------------------------------------------------------------
// Process the hits
void process_hits(UInt_t nhits, ULong64_t event_ts) {

   n_processed_hits += nhits;

   // Sort the hits in time order by their timestamp
   std::sort(hits.begin(), hits.end());
         
   // Process hits
   for (UInt_t i = 0; i < nhits; i++) treat_hit(&hits[i], event_ts);

   // Erase the hits that we just processed
   hits.erase(hits.begin(), hits.begin() + nhits);  
   
   printf("Processed %llu hits from %llu/%llu buffers\r", n_processed_hits, nbuffer,
         total_buffer);
   fflush(stdout);   
};



//-----------------------------------------------------------------------------
// Treat a single word
void treat_word(ISSWord *w) {

    n_word++;
    
    // For info words get full timestamps
    if(w->IsInfo()) {
    
        n_info++;
            
        if (ID_V1730 == w->GetInfoModule() ) return; //ignore V1730 for now...
        
        // If we find a timestamp from the V1495 logic unit (EBIS pulse trigger)
        // write all previous hits to the event root tree and start a new event
        if (ID_EBIS == w->GetInfoModule()) {
        
            // Process all previous hits
            process_hits(hits.size(), last_global_ts); 
        
            // Update the global timestamp (event timestamp)
            if (w->HasExtendedTimestamp()) {
                new_global_ts = w->GetFullGlobalTimestamp();
                last_global_ts = new_global_ts;
                n_ebis_pulses++;
            }
            
            n_events++;
        }
        
        // Else it's a V17XX module, update the full ADC timestamp
        else {
            if (w->HasExtendedTimestamp()) {
                last_adc8_ts = w->GetFullADCTimestamp();
            }
        }
    }
        
    if (!first_adc8_ts) first_adc8_ts = last_adc8_ts;
    if (!first_global_ts) first_global_ts = last_global_ts;
    
    
    // For ADC words fill the hits    
    if (w->IsADC()) {
        
        if (ID_V1730 == w->GetADCModule() ) return; //ignore V1730 for now...
        
        n_adc++;
        UInt_t module = w->GetADCModule();
        UInt_t channel = w->GetADCChannel();
        UInt_t adc_data = w->GetADCConversion();
        UShort_t data_id = w->GetADCDataID();
        UInt_t id = 32*module + channel;
        hStats->AddBinContent(id, 1);
        
        // Create an ISSHit object and add to event storage vector
        ISSHit h;
        h.Set(module, channel, last_adc8_ts, adc_data, data_id);
        hits.push_back(h);
       
        // If we reached the maximum number of hits in event, process half of them
        if (hits.size() > MAXHITS) process_hits(MAXHITS/2, last_global_ts);
        
        if (w->IsQLong()) { 
            hstatQLong->AddBinContent(id, 1);
            n_qlong++;
        }
        if (w->IsQShort()) { 
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
        // Set up the word
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
    printf("\nOpening file: %s\n", filename);
    ISSFile f(filename);
    
    nbuffer = 0;
    total_buffer = f.GetNBlocks();
    
    // Loop over blocks
    for (UInt_t i = 0; i < f.GetNBlocks(); i++, nbuffer++) {
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
void make_tree(const Char_t *infile = "../../data/R20_0") {     
     
     // Open output file
    TFile *f = TFile::Open("output.root", "recreate");
    if (!f) return;
    // Create a root tree
    tree = new TTree("isstree", "ISS data tree");
    // Define the branches in the root tree using the struct above
    tree->Branch("issentry", &issentry.global_event_ts,"global_event_ts/l:adc_ts/l:module/i:channel/i:data_id/i:adc_data/i");

    // Create histograms
    hStats        = new TH1I("hStats",     "Total statistics", MAXID, 0, MAXID);
    hstatQLong    = new TH1I("hstatQLong", "QLong statistics", MAXID, 0, MAXID);
    hstatQShort   = new TH1I("hstatQShort","QShort statistics", MAXID, 0, MAXID);   
    
    // Treat the file
    treat_file(infile);
    
    // Define more files here ( TODO: read file list from file or give as input)
    
    infile = "../../data/R21_0"; treat_file(infile);
    infile = "../../data/R22_0"; treat_file(infile);
    infile = "../../data/R23_0"; treat_file(infile);
    infile = "../../data/R24_0"; treat_file(infile);
    infile = "../../data/R25_0"; treat_file(infile);
    infile = "../../data/R26_0"; treat_file(infile);
    infile = "../../data/R27_0"; treat_file(infile);
    infile = "../../data/R28_0"; treat_file(infile);
    

    // Get time difference between first and last global timestamp
    Double_t diff = (Double_t)(last_global_ts - first_global_ts);
    diff *= 10e-9; // Convert to seconds

    // Write statistics
    printf("\n -------- \n");
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
                 qlong, qshort,  (Double_t)integral / diff);
    }
    // Write everything
    f->Write();
    f->Close();

}

