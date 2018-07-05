// Script to make a root tree of ISS data file(s).
// Time orders the ADC items only according to ADC timestamps.
// Note: The ADC timestamps could be reset if the DAQ was stopped between the runs!!!
//
// Joonas Konki - 20180705
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
#define ID_V1730 2      // V1730 module with 16 ns (?) time resolution

// Tree entry definition
struct struct_tree_entry {
    unsigned long long global_event_ts; //full V1495 48-bit timestamp of the event (10 ns resolution)
    unsigned long long adc_ts; //full 48-bit timestamp from a V1725 module (8 ns ? resolution)
    unsigned int       module; // module number
    unsigned int       channel; // channel number
    unsigned short     data_id; // QLong = 0, QShort = 1, FineTiming = 3
    unsigned int       adc_data; // ADC conversion
  };

// Storage of hits within one event
std::vector <ISSHit> hits;
TTree *tree;
struct_tree_entry issentry;

// Histograms
TH1I *hStats, *hstatQLong, *hstatQShort;

// Timestamps and statistics
ULong64_t first_adc8_ts = 0, first_global_ts=0, last_adc8_ts=0, last_adc16_ts=0,
          last_global_ts = 0, new_global_ts=0, prev_adc8_ts=0, prev_adc16_ts=0, adc_diff=0,
          global_adc_ts = 0;
ULong64_t n_ebis_pulses = 0, n_info=0, n_adc =0, n_word=0, n_qlong=0, n_qshort=0,
          n_finetime=0, n_traces=0, n_adc_ts=0, n_global_ts=0, n_processed_hits=0,
          n_events=0, nbuffer=0, total_buffer=0;
Int_t counter = 0;

Int_t run_number = 0, prev_run_number = 0;



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

    // debugging
    if (counter < 500 ){
		printf("ADC TS: 0x%012llX MODULE %d CH %d DATAID %d ADCDATA %d\n",
		       issentry.adc_ts, issentry.module, issentry.channel, issentry.data_id, issentry.adc_data );
		counter++;
	}


    tree->Fill();
}

//-----------------------------------------------------------------------------
// Process the hits
void process_hits(UInt_t nhits, ULong64_t event_ts) {

   n_processed_hits += nhits;

   // Sort the hits in time order by their timestamp
   std::sort(hits.begin(), hits.end());

   if (counter < 500) printf("GLOBAL TS: 0x%012llX\n", event_ts);

   // Process hits
   for (UInt_t i = 0; i < nhits; i++) treat_hit(&hits[i], event_ts);

   // Erase the hits that we just processed
   hits.erase(hits.begin(), hits.begin() + nhits);

   //printf("Processed %llu hits from %llu/%llu buffers\r", n_processed_hits, nbuffer,
   //      total_buffer);
   //fflush(stdout);
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
        if (ID_EBIS == w->GetInfoModule()) {

            // Update the global timestamp (event timestamp) just for fun
            if (w->HasExtendedTimestamp()) {
                if ( last_global_ts &&  w->GetFullGlobalTimestamp() < last_global_ts ) {
                    printf("TIMESTAMP error!  new GLOBAL ts (0x%012llX) older than previous (0x%012llX)\n", w->GetFullADCTimestamp(), last_global_ts);

                }

                last_global_ts = w->GetFullGlobalTimestamp();
                n_ebis_pulses++;
                n_global_ts++;
            }
        }

        // Else it's a V17XX module, update the full ADC timestamp
        else {
            if (w->HasExtendedTimestamp()) {

                //if ( global_adc_ts && global_adc_ts + last_adc8_ts > global_adc_ts + w->GetFullADCTimestamp() ) {
                //	printf("TIMESTAMP error! new ADC ts (%llu) older than previous (%llu)\n", global_adc_ts + w->GetFullADCTimestamp(), global_adc_ts +last_adc8_ts);
                //}

                if ( last_adc8_ts && last_adc8_ts > w->GetFullADCTimestamp() && run_number > prev_run_number ){
                	printf("TIMESTAMP error with run file change! new ADC ts (0x%016llX) older than previous (0x%016llX)\n", w->GetFullADCTimestamp(), last_adc8_ts);
                    printf("Old global ADC ts was    (0x%016llX)\n", global_adc_ts);
                    global_adc_ts += last_adc8_ts;
                    global_adc_ts += 250000;
                    printf("New global ADC ts set to (0x%016llX)\n", global_adc_ts);
                    prev_run_number++;
                }

                last_adc8_ts = w->GetFullADCTimestamp();
                n_adc_ts++;

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
        h.Set(module, channel, (global_adc_ts + last_adc8_ts), adc_data, data_id);
        hits.push_back(h);

        // If we reached the maximum number of hits in the storage vector, process half of them
        if (hits.size() > MAXHITS) {
        	printf("Max hits exceeded. Processing!\n");
        	process_hits(MAXHITS/2, last_global_ts);
       	}

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
        if (w->IsTrace()) {
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
    printf("Opening file: %s\n", filename);
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
void make_tree_onlyadcstamps(const Char_t *infile = "../../data/R57_0") {

     // Open output file
    TFile *f = TFile::Open("output_R57-68_onlyadcstamps.root", "recreate");
    if (!f) return;
    // Create a root tree
    tree = new TTree("isstree", "ISS data tree");
    // Define the branches in the root tree using the struct above
    tree->Branch("issentry", &issentry,"global_event_ts/l:adc_ts/l:module/i:channel/i:data_id/i:adc_data/i");

    // Create histograms
    hStats        = new TH1I("hStats",     "Total statistics", MAXID, 0, MAXID);
    hstatQLong    = new TH1I("hstatQLong", "QLong statistics", MAXID, 0, MAXID);
    hstatQShort   = new TH1I("hstatQShort","QShort statistics", MAXID, 0, MAXID);

    // Treat the file
    treat_file(infile);

    // Define more files here ( TODO: read file list from file or give as input)
    run_number++; infile = "../../data/R58_0"; treat_file(infile);
    run_number++; infile = "../../data/R59_0"; treat_file(infile);
    run_number++; infile = "../../data/R60_0"; treat_file(infile);
    run_number++; infile = "../../data/R61_0"; treat_file(infile);
    run_number++; infile = "../../data/R62_0"; treat_file(infile);
    run_number++; infile = "../../data/R63_0"; treat_file(infile);
    run_number++; infile = "../../data/R64_0"; treat_file(infile);
    run_number++; infile = "../../data/R65_0"; treat_file(infile);
    run_number++; infile = "../../data/R66_0"; treat_file(infile);
    run_number++; infile = "../../data/R67_0"; treat_file(infile);
    run_number++; infile = "../../data/R68_0"; treat_file(infile);
/*
    run_number++; infile = "../../data/R21_0"; treat_file(infile);
    run_number++; infile = "../../data/R22_0"; treat_file(infile);
    run_number++; infile = "../../data/R23_0"; treat_file(infile);
    run_number++; infile = "../../data/R24_0"; treat_file(infile);
    run_number++; infile = "../../data/R25_0"; treat_file(infile);
    run_number++; infile = "../../data/R26_0"; treat_file(infile);
    run_number++; infile = "../../data/R27_0"; treat_file(infile);
    run_number++; infile = "../../data/R28_0"; treat_file(infile);
*/

     // Process last bunch of hits if there is any
    if (hits.size() > 0) {
    	printf("Processing last bunch of hits...");
    	process_hits(hits.size(), last_global_ts);
    }

    std::vector<ISSHit>().swap(hits); // deallocate memory and clear vector

    // Get time difference between first and last global timestamp
    Double_t diff = (Double_t)(last_global_ts - first_global_ts);
    diff *= 10e-9; // Convert to seconds

    // Write statistics
    printf("\n -------- \n");
    printf("Acquisition time: %.3f seconds\n", diff);
    printf("Number  data words: %llu\n", n_word);
    printf("Number  info words: %llu\n", n_info);
    printf("Number   global ts: %llu\n", n_global_ts);
    printf("Number      adc ts: %llu\n", n_adc_ts);
    printf("Number   adc words: %llu\n", n_adc);
    printf("Number   QL  words: %llu\n", n_qlong);
    printf("Number   QS  words: %llu\n", n_qshort);
    printf("Number   FT  words: %llu\n", n_finetime);
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
