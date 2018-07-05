// Script to analyse a root tree of ISS data.
// Assumes that the ADC items are in time order
//
// Joonas Konki - 20180705
//

#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

#include "TROOT.h"
#include <TFile.h>
#include <TTree.h>
#include <TMath.h>
#include <TH1I.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TString.h>
#include <TAxis.h>
#include <TRandom.h>

#include "ISSBuffer.hh"
#include "ISSFile.hh"
#include "ISSWord.hh"
#include "ISSHit.hh"

#define MAXID 100
#define MAXHITS 10000 // Maximum number of hits allowed in the event storage
#define ID_EBIS 63      // Module number of the EBIS trigger pulse
#define ID_V1730 2      // V1730 module with 16 ns (?) time resolution
#define MAXBIN 65536

//#define EVENT_WIDTH  250000 // Event width in ADC ticks ( 4 ns ticks -> 1 ms event )
#define EVENT_WIDTH 2500  // Event width in ADC ticks ( 4 ns ticks -> 10 us event )
//#define TRIGGER_DELAY 62500 // Trigger offset in ADC ticks to include events before trigger, not implemented yet

#define COINC_WINDOW 50 // Coincidence window in ADC tics for separate channels
#define COINC_CHANNEL 1 // Coincidence window in ADC tics for same channel (to gather QLong, QShort and FineTime)

#define LOW_THRESHOLD 2000 // Reject events with a lower threshold

#define DATAID_QLONG 0
#define DATAID_QSHORT 1
#define DATAID_FINETIME 3

// Tree entry definition
struct struct_tree_entry {
    unsigned long long global_event_ts; //full V1495 48-bit timestamp of the event (10 ns resolution)
    unsigned long long adc_ts; //full 48-bit timestamp from a V1725 module (8 ns resolution)
    unsigned int       module; // module number
    unsigned int       channel; // channel number
    unsigned short     data_id; // QLong = 0, QShort = 1, FineTiming = 3
    unsigned int       adc_data; // ADC conversion
};

// Single hit in a STUB array, can be X1, X2, E or G
// det_id defines the right 'r', left 'l', top 't' or bottom 'b' side detector
struct stub_hit {
    unsigned long long adc_ts = 0;
    char det_id = 'n';
    int det_no = 0; // not used for STUB array
    float qlong = -1;
    float qshort = -1;
    float finetime = -1;
};

// Container to hold information of coincident hits in one STUB detector (X1,X2,E,G)
// det_id defines the right 'r', left 'l', top 't' or bottom 'b' side detector
struct stub {
    unsigned long long adc_ts = 0; // common time stamp taken from full energy signal
    char det_id = 'n';
    stub_hit x1;  // x1 side energy
    stub_hit x2;  // x2 side energy
    stub_hit e;   // full energy
    stub_hit g;   // guard ring
};

// generic hit for single channel detectors
struct genhit {
    unsigned long long adc_ts = 0;
    int det_no = 0;
    float qlong = -1;
    float qshort = -1;
    float finetime = -1;
};

// Containers for RecoilE, dE and STUB array hits in one event
std::vector <genhit> recoil_e_hits;
std::vector <genhit> recoil_de_hits;
std::vector <stub> stub_array;

// Container of all raw hits within one event
std::vector <struct_tree_entry> eventhits;

// Channel mapping
// STUB channels in module 0 in order X1, X2, E, G
int stub_l_chs[4] = {0,1,3,2};
int stub_t_chs[4] = {5,4,7,6};
int stub_b_chs[4] = {10,11,8,9};
int stub_r_chs[4] = {14,15,12,13};
// Recoil_E chs in module 1 in order E1, E2, E3, E4
int recoil_e_chs[4] = {4,6,13,15};
// Recoil_dE chs in module 1 in order dE1, dE2, dE3, dE4
int recoil_de_chs[4] = {5,7,12,14};

// Histograms
TH1F *hTdiff, *hTdiffalladc, *hTdiffalladclog;
TH1F *hEventLength;
TH1F *hEventTimeDiff;
TH1F *hHitsInEvent;
TH1F *hSumQLong;

TH1F *hSTUBL_e, *hSTUBT_e, *hSTUBB_e, *hSTUBR_e;
TH1F *hSTUBL_x1, *hSTUBT_x1, *hSTUBB_x1, *hSTUBR_x1;
TH1F *hSTUBL_x2, *hSTUBT_x2, *hSTUBB_x2, *hSTUBR_x2;
TH1F *hSTUBL_t, *hSTUBT_t, *hSTUBB_t, *hSTUBR_t;
TH1F *hSTUBL_x1x2_t, *hSTUBT_x1x2_t, *hSTUBB_x1x2_t, *hSTUBR_x1x2_t;
TH1F *hRecoilE_STUB_t;
TH1F *hNSTUBInEvent, *hNRecoilEInEvent;
TH2F *hSTUBL_x1x2, *hSTUBT_x1x2, *hSTUBB_x1x2, *hSTUBR_x1x2;
TH2F *hSTUBL_e_x1x2, *hSTUBT_e_x1x2, *hSTUBB_e_x1x2, *hSTUBR_e_x1x2;
TH2F *hSTUBL_x1px2_e, *hSTUBT_x1px2_e, *hSTUBB_x1px2_e, *hSTUBR_x1px2_e;
TH2F *hSTUBL_x1e, *hSTUBL_x2e, *hSTUBT_x1e, *hSTUBT_x2e, *hSTUBB_x1e, *hSTUBB_x2e, *hSTUBR_x1e, *hSTUBR_x2e;


TH1F *hRecoilE1_e,*hRecoilE2_e,*hRecoilE3_e,*hRecoilE4_e;
TH1F *hRecoilE1_t,*hRecoilE2_t,*hRecoilE3_t,*hRecoilE4_t;

ULong64_t n_global_ts = 0, n_adc = 0, n_qlong = 0, n_qshort = 0, n_finetime = 0, n_traces = 0;
ULong64_t n_events = 0, n_good_events = 0, n_total_hits = 0, n_hits = 0, n_bad_hits = 0, n_good_hits = 0,
          n_overrange = 0, n_noise = 0;
ULong64_t prev_event_ts = 0, prev_adc_ts = 0, first_adc_ts_in_event = 0, first_ever_adc_ts=0,
          last_adc_ts_in_event = 0, first_global_ts = 0;
Double_t percentage = 0.;

Int_t counter = 0;
Int_t errorcounter = 0;

void print_hit(struct_tree_entry *hit) {
    // debugging
	printf("GLOBAL TS: 0x%012llX ADC TS: 0x%012llX MODULE %d CH %d DATAID %d ADCDATA %d\n",
	       hit->global_event_ts, hit->adc_ts, hit->module, hit->channel, hit->data_id, hit->adc_data );
}

// Conditions to cancel some bad hits in the code are defined there
Bool_t is_bad_hit(struct_tree_entry *hit) {
  	//if ( (hit->adc_ts - first_adc_ts_in_event) > 10000000  ) return kTRUE;

    // over range items
    if ( hit->adc_data > 65534 ) {
        n_overrange++;
        return kTRUE;
    }

    // low threshold
    if ( hit->adc_data < LOW_THRESHOLD ) {
        n_noise++;
        return kTRUE;
    }

  	if ( hit->data_id != 0 ) { return kTRUE; } // accept only QLong words

  	//else return kFALSE;
  	return kFALSE;
}

void clear_event() {
    // Clear the containers and deallocate the memory to prepare for the next event
    std::vector<genhit>().swap(recoil_e_hits);
    std::vector<genhit>().swap(recoil_de_hits);
    std::vector<stub>().swap(stub_array);
    std::vector<struct_tree_entry>().swap(eventhits);

}

void populate_recoil_e_array(struct_tree_entry *hit){
    int channel = hit->channel;
    int data_id = hit->data_id;
    genhit newhit;
    newhit.adc_ts = hit->adc_ts;

    int det_number = -1;
    for (int i=0; i<4; i++) {
        if (recoil_e_chs[i] == channel) {
            det_number = i;
            newhit.det_no = i+1;
            break;
        }
    }
    if (det_number == -1) return; // not a mapped recoil E detector -> exit

    switch (data_id) {
        case 0:
            newhit.qlong = hit->adc_data;
            break;
        case 1:
            newhit.qshort = hit->adc_data;
            break;
        case 3:
            newhit.finetime = hit->adc_data;
            break;
        default:
            printf("Something went wrong - data_id not identified\n");
            break;
    }

    // Try to find if a Hit with the same ADC timestamp already exists in the container
    int samedetfound = -1;
    for (unsigned int k=0; k<recoil_e_hits.size(); k++) {
        if (newhit.det_no == recoil_e_hits[k].det_no && TMath::Abs( (Long64_t)(newhit.adc_ts-recoil_e_hits[k].adc_ts) ) < COINC_CHANNEL ) {
        //if (newhit.det_no == recoil_e_hits[k].det_no && (newhit.adc_ts == recoil_e_hits[k].adc_ts )  ) {
            samedetfound = k;
            break;
        }
    }

    if (samedetfound >= 0) { // Put QLong, QShort and FineTime in same Hit that was found
        if(data_id == DATAID_QLONG) recoil_e_hits[samedetfound].qlong = newhit.qlong;
        else if (data_id == DATAID_QSHORT) recoil_e_hits[samedetfound].qshort = newhit.qshort;
        else if (data_id == DATAID_FINETIME) recoil_e_hits[samedetfound].finetime = newhit.finetime;
        else {
            printf("Something went wrong - stubfound loop 1\n");
            return;
        }

    } else { // Add this new Hit to the container
        recoil_e_hits.push_back(newhit);
    }
}


void populate_stub_array (struct_tree_entry *hit) {

    int channel = hit->channel;
    int data_id = hit->data_id;

    stub_hit newhit; // = {0, 'n',0,-1,-1,-1};
    newhit.adc_ts = hit->adc_ts;

    int det_type = -1;
    for (int i=0; i<4; i++) {
        if (stub_l_chs[i] == channel) {
            newhit.det_id = 'l';
            det_type = i;
            break;
        }
        if (stub_r_chs[i] == channel) {
            newhit.det_id = 'r';
            det_type = i;
            break;
        }
        if (stub_t_chs[i] == channel) {
            newhit.det_id = 't';
            det_type = i;
            break;
        }
        if (stub_b_chs[i] == channel) {
            newhit.det_id = 'b';
            det_type = i;
            break;
        }
    }

    if (det_type == -1 ) return; // not a stub hit -> exit

    switch (data_id) {
        case 0:
            newhit.qlong = hit->adc_data;
            break;
        case 1:
            newhit.qshort = hit->adc_data;
            break;
        case 3:
            newhit.finetime = hit->adc_data;
            break;
        default:
            printf("Something went wrong - data_id not identified\n");
            break;
    }

    int stubfound = -1;
    for (unsigned int k=0; k<stub_array.size(); k++) {
        if (newhit.det_id == stub_array[k].det_id && TMath::Abs( (Long64_t)(newhit.adc_ts-stub_array[k].adc_ts) ) < COINC_WINDOW ) {
            stubfound = k;
            break;
        }
    }

    if (stubfound >= 0) {
        switch (det_type) {
            case -1:
                return; // stub channel not identified
            case 0: // X1 ch
                if (stub_array[stubfound].x1.det_id != 'n') { // already has X1 information for either qlong, qshort, or fine timing
                    if(      data_id == DATAID_QLONG)    stub_array[stubfound].x1.qlong = newhit.qlong;
                    else if (data_id == DATAID_QSHORT)   stub_array[stubfound].x1.qshort = newhit.qshort;
                    else if (data_id == DATAID_FINETIME) stub_array[stubfound].x1.finetime = newhit.finetime;
                    else {
                        printf("Something went wrong - stubfound loop 1\n");
                        return;
                    }
                }
                else { // doesn't have information on this detector yet, add it there
                    stub_array[stubfound].x1 = newhit;
                    return;
                }
                break;
            case 1: // X2 ch
                if (stub_array[stubfound].x2.det_id != 'n') { // already has X2 information for either qlong, qshort, or fine timing
                    if(      data_id == DATAID_QLONG)    stub_array[stubfound].x2.qlong = newhit.qlong;
                    else if (data_id == DATAID_QSHORT)   stub_array[stubfound].x2.qshort = newhit.qshort;
                    else if (data_id == DATAID_FINETIME) stub_array[stubfound].x2.finetime = newhit.finetime;
                    else {
                        printf("Something went wrong - stubfound loop 2\n");
                        return;
                    }
                }
                else { // doesn't have information on this detector yet, add it there
                    stub_array[stubfound].x2 = newhit;
                    return;
                }
                break;
            case 2: // E ch
                if (stub_array[stubfound].e.det_id != 'n') { // already has E information for either qlong, qshort, or fine timing
                    if (     data_id == DATAID_QLONG)    stub_array[stubfound].e.qlong = newhit.qlong;
                    else if (data_id == DATAID_QSHORT)   stub_array[stubfound].e.qshort = newhit.qshort;
                    else if (data_id == DATAID_FINETIME) stub_array[stubfound].e.finetime = newhit.finetime;
                    else {
                        printf("Something went wrong - stubfound loop 3\n");
                        return;
                    }
                }
                else { // doesn't have information on this detector yet, add it there
                    stub_array[stubfound].e = newhit;
                    return;
                }
                break;
            case 3: // G ch
                if (stub_array[stubfound].g.det_id != 'n') { // already has E information for either qlong, qshort, or fine timing
                    if (     data_id == DATAID_QLONG)    stub_array[stubfound].g.qlong = newhit.qlong;
                    else if (data_id == DATAID_QSHORT)   stub_array[stubfound].g.qshort = newhit.qshort;
                    else if (data_id == DATAID_FINETIME) stub_array[stubfound].g.finetime = newhit.finetime;
                    else {
                        printf("Something went wrong - stubfound loop 4\n");
                        return;
                    }
                }
                else { // doesn't have information on this detector yet, add it there
                    stub_array[stubfound].g = newhit;
                    return;
                }
                break;
            default:
                printf("Something went wrong - stubfound loop 5\n");
                return;
        }

    } else { // create new STUB and add it to the stub_array
        stub newstub;
        newstub.adc_ts = newhit.adc_ts;
        newstub.det_id = newhit.det_id;
        switch (det_type) {
            case 0:
                newstub.x1 = newhit;
                break;
            case 1:
                newstub.x2 = newhit;
                break;
            case 2:
                newstub.e = newhit;
                break;
            case 3:
                newstub.g = newhit;
                break;
        }
        stub_array.push_back(newstub);
    }
}

// Map the various hits in eventhits to the correct detector containers based on
// an ADC channel mapping
void do_detector_mapping() {
    struct_tree_entry entry;
    for (unsigned int j=0; j<eventhits.size(); j++) {
        entry = eventhits[j];

        switch(entry.module) {
            case 0: // It's the module with STUB channels
                populate_stub_array(&entry);
                break;

            case 1: // It's the Recoil_E / Recoil_dE module
                populate_recoil_e_array(&entry);
                break;


            default: // It's some other MODULE
                break;
        }
    }
}


void fill_histograms (){

    hNSTUBInEvent->Fill( stub_array.size() );
    hNRecoilEInEvent->Fill( recoil_e_hits.size() );

    stub st;
    // Loop through STUB array
    for (unsigned int i = 0; i<stub_array.size(); i++) {
        st = stub_array[i];

        if (st.det_id == 'l') {
            if (st.e.qlong > 0) {
                hSTUBL_e->Fill(st.e.qlong);
                hSTUBL_t->Fill(st.e.adc_ts-first_adc_ts_in_event);
            }
            if (st.x1.qlong > 0) {
                hSTUBL_x1->Fill(st.x1.qlong);
            }
            if (st.x2.qlong > 0) {
                hSTUBL_x2->Fill(st.x2.qlong);
            }

            if (st.e.qlong > 0 && st.x1.qlong > 0 && st.x2.qlong > 0) {
                hSTUBL_e_x1x2->Fill( (st.x1.qlong-st.x2.qlong)/st.e.qlong , st.e.qlong  );
                hSTUBL_x1px2_e->Fill( (st.x1.qlong+st.x2.qlong) , st.e.qlong);
                hSTUBL_x1x2_t->Fill( st.x1.adc_ts-st.x2.adc_ts );
                hSTUBL_x1x2->Fill(st.x1.qlong, st.x2.qlong);
                hSTUBL_x1e->Fill(st.x1.qlong, st.e.qlong);
                hSTUBL_x2e->Fill(st.x2.qlong, st.e.qlong);
            }

        }

        if (st.det_id == 't') {
            if (st.e.qlong > 0) {
                hSTUBT_e->Fill(st.e.qlong);
                hSTUBT_t->Fill(st.e.adc_ts-first_adc_ts_in_event);
            }
            if (st.x1.qlong > 0) {
                hSTUBT_x1->Fill(st.x1.qlong);
            }
            if (st.x2.qlong > 0) {
                hSTUBT_x2->Fill(st.x2.qlong);
            }

            if (st.e.qlong > 0 && st.x1.qlong > 0 && st.x2.qlong > 0) {
                hSTUBT_e_x1x2->Fill( (st.x1.qlong-st.x2.qlong)/st.e.qlong , st.e.qlong  );
                hSTUBT_x1px2_e->Fill( (st.x1.qlong+st.x2.qlong) , st.e.qlong);
                hSTUBT_x1x2_t->Fill( st.x1.adc_ts-st.x2.adc_ts );
                hSTUBT_x1x2->Fill(st.x1.qlong, st.x2.qlong);
                hSTUBT_x1e->Fill(st.x1.qlong, st.e.qlong);
                hSTUBT_x2e->Fill(st.x2.qlong, st.e.qlong);
            }
        }

        if (st.det_id == 'b') {
            if (st.e.qlong > 0) {
                hSTUBB_e->Fill(st.e.qlong);
                hSTUBB_t->Fill(st.e.adc_ts-first_adc_ts_in_event);
            }
            if (st.x1.qlong > 0) {
                hSTUBB_x1->Fill(st.x1.qlong);
            }
            if (st.x2.qlong > 0) {
                hSTUBB_x2->Fill(st.x2.qlong);
            }

            if (st.e.qlong > 0 && st.x1.qlong > 0 && st.x2.qlong > 0) {
                hSTUBB_e_x1x2->Fill( (st.x1.qlong-st.x2.qlong)/st.e.qlong , st.e.qlong  );
                hSTUBB_x1px2_e->Fill( (st.x1.qlong+st.x2.qlong) , st.e.qlong);
                hSTUBB_x1x2_t->Fill( st.x1.adc_ts-st.x2.adc_ts );
                hSTUBB_x1x2->Fill(st.x1.qlong, st.x2.qlong);
                hSTUBB_x1e->Fill(st.x1.qlong, st.e.qlong);
                hSTUBB_x2e->Fill(st.x2.qlong, st.e.qlong);
            }
        }

        if (st.det_id == 'r') {
            if (st.e.qlong > 0) {
                hSTUBR_e->Fill(st.e.qlong);
                hSTUBR_t->Fill(st.e.adc_ts-first_adc_ts_in_event);
            }
            if (st.x1.qlong > 0) {
                hSTUBR_x1->Fill(st.x1.qlong);
            }
            if (st.x2.qlong > 0) {
                hSTUBR_x2->Fill(st.x2.qlong);
            }

            if (st.e.qlong > 0 && st.x1.qlong > 0 && st.x2.qlong > 0) {
                hSTUBR_e_x1x2->Fill( (st.x1.qlong-st.x2.qlong)/st.e.qlong , st.e.qlong  );
                hSTUBR_x1px2_e->Fill( (st.x1.qlong+st.x2.qlong) , st.e.qlong);
                hSTUBR_x1x2_t->Fill( st.x1.adc_ts-st.x2.adc_ts );
                hSTUBR_x1x2->Fill(st.x1.qlong, st.x2.qlong);
                hSTUBR_x1e->Fill(st.x1.qlong, st.e.qlong);
                hSTUBR_x2e->Fill(st.x2.qlong, st.e.qlong);
            }
        }

    } // end loop stub stub_array

    genhit ghit;
    for (unsigned int i = 0; i < recoil_e_hits.size(); i++) {
        ghit = recoil_e_hits[i];

        if (ghit.det_no == 1) {
            if (ghit.qlong > 0) {
                hRecoilE1_e->Fill(ghit.qlong);
                hRecoilE1_t->Fill(ghit.adc_ts - first_adc_ts_in_event);
            }
        } else if (ghit.det_no == 2) {
            if (ghit.qlong > 0) {
                hRecoilE2_e->Fill(ghit.qlong);
                hRecoilE2_t->Fill(ghit.adc_ts - first_adc_ts_in_event);
            }
        } else if (ghit.det_no == 3) {
            if (ghit.qlong > 0) {
                hRecoilE3_e->Fill(ghit.qlong);
                hRecoilE3_t->Fill(ghit.adc_ts - first_adc_ts_in_event);
            }
        } else if (ghit.det_no == 4) {
            if (ghit.qlong > 0) {
                hRecoilE4_e->Fill(ghit.qlong);
                hRecoilE4_t->Fill(ghit.adc_ts - first_adc_ts_in_event);
            }
        } else {
            printf("Something went wrong - Recoil E with det_no not found.\n");
        }

    }

    // Recoil E - STUB coincidences
    for (unsigned int i =0; i< recoil_e_hits.size(); i++) {
        ghit = recoil_e_hits[i];

        for (unsigned int j=0; j<stub_array.size(); j++) {
                st = stub_array[j];
                hRecoilE_STUB_t->Fill(st.adc_ts-ghit.adc_ts);
        }
    }

}


void treat_event(){

    // Fill some statistics histograms
    for (unsigned int j=0; j<eventhits.size(); j++) {
      hTdiff->Fill(eventhits[j].adc_ts - first_adc_ts_in_event);
      hTdiffalladc->Fill(     TMath::Abs( (Long64_t)(eventhits[j].adc_ts - first_ever_adc_ts) )  );
      hTdiffalladclog->Fill(  TMath::Log( (eventhits[j].adc_ts - first_ever_adc_ts)*4e-9 ) );
    }
    hHitsInEvent->Fill(eventhits.size());
    n_good_hits += eventhits.size();
    n_events++;

    do_detector_mapping();

    fill_histograms();

    clear_event();
}

//-----------------------------------------------------------------------------
// Get statistics for a file
void analyse_tree_onlyadcstamps(string infile = "./output_R57-68_onlyadcstamps.root") {

     // Open input file
    TFile *f = new TFile( infile.c_str() );
    if (!f) return;
    printf("Opened input file %s\n", infile.c_str() );

     // Open output file
    string outfile = "./anaoutput_R57-68_onlyadcstamps.root";
    TFile *fo = TFile::Open( outfile.c_str() , "recreate");
    if (!fo) return;
    printf("Opened output file %s\n", outfile.c_str() );

    // Define the histograms
    hTdiff = new TH1F("hTdiff","Time difference of hits in event in ticks", EVENT_WIDTH, 0, EVENT_WIDTH);
    hTdiffalladc = new TH1F("hTdiffalladc","Time difference of any ADC ts to first ever ADC ts", 1e6, 0, 1e9);
    hTdiffalladclog = new TH1F("hTdiffalladclog","log of Time difference of any ADC ts to first ever ADC ts", 200, 0, 20);
    hEventLength = new TH1F("hEventLength","Event length in ticks from 1st and last ADC ts", EVENT_WIDTH, 0, EVENT_WIDTH);
    hHitsInEvent = new TH1F("hHitsInEvent","Number of hits in one event", 100, 0, 100);
    hNSTUBInEvent = new TH1F("hNSTUBInEvent","Number of STUBs in one event", 100, 0, 100);
    hNRecoilEInEvent = new TH1F("hNRecoilEInEvent","Number of RecoilEs in one event", 100, 0, 100);
    hSumQLong= new TH1F("hSumQLong","Sum of all QLong items", MAXBIN, 0, MAXBIN);

    hSTUBL_e = new TH1F("hSTUBL_e","STUB-L energy", 6000, 0, 60000);
    hSTUBT_e = new TH1F("hSTUBT_e","STUB-T energy", 6000, 0, 60000);
    hSTUBB_e = new TH1F("hSTUBB_e","STUB-B energy", 6000, 0, 60000);
    hSTUBR_e = new TH1F("hSTUBR_e","STUB-R energy", 6000, 0, 60000);

    hSTUBL_x1 = new TH1F("hSTUBL_x1","STUB-L x1 energy", MAXBIN, 0, MAXBIN);
    hSTUBL_x2 = new TH1F("hSTUBL_x2","STUB-L x2 energy", MAXBIN, 0, MAXBIN);
    hSTUBT_x1 = new TH1F("hSTUBT_x1","STUB-T x1 energy", MAXBIN, 0, MAXBIN);
    hSTUBT_x2 = new TH1F("hSTUBT_x2","STUB-T x2 energy", MAXBIN, 0, MAXBIN);
    hSTUBB_x1 = new TH1F("hSTUBB_x1","STUB-B x1 energy", MAXBIN, 0, MAXBIN);
    hSTUBB_x2 = new TH1F("hSTUBB_x2","STUB-B x2 energy", MAXBIN, 0, MAXBIN);
    hSTUBR_x1 = new TH1F("hSTUBR_x1","STUB-R x1 energy", MAXBIN, 0, MAXBIN);
    hSTUBR_x2 = new TH1F("hSTUBR_x2","STUB-R x2 energy", MAXBIN, 0, MAXBIN);

    hSTUBL_x1x2 = new TH2F("hSTUBL_x1x2","STUB-L x1 vs x2", 600, 0, 60000, 600, 0, 60000);
    hSTUBL_x1e = new TH2F("hSTUBL_x1e","STUB-L x1 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBL_x2e = new TH2F("hSTUBL_x2e","STUB-L x2 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBT_x1x2 = new TH2F("hSTUBT_x1x2","STUB-T x1 vs x2", 600, 0, 60000, 600, 0, 60000);
    hSTUBT_x1e = new TH2F("hSTUBT_x1e","STUB-T x1 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBT_x2e = new TH2F("hSTUBT_x2e","STUB-T x2 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBB_x1x2 = new TH2F("hSTUBB_x1x2","STUB-B x1 vs x2", 600, 0, 60000, 600, 0, 60000);
    hSTUBB_x1e = new TH2F("hSTUBB_x1e","STUB-B x1 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBB_x2e = new TH2F("hSTUBB_x2e","STUB-B x2 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBR_x1x2 = new TH2F("hSTUBR_x1x2","STUB-R x1 vs x2", 600, 0, 60000, 600, 0, 60000);
    hSTUBR_x1e = new TH2F("hSTUBR_x1e","STUB-R x1 vs e", 600, 0, 60000, 600, 0, 60000);
    hSTUBR_x2e = new TH2F("hSTUBR_x2e","STUB-R x2 vs e", 600, 0, 60000, 600, 0, 60000);

    hSTUBL_t = new TH1F("hSTUBL_t","STUB-L time in event", EVENT_WIDTH, 0, EVENT_WIDTH);
    hSTUBT_t = new TH1F("hSTUBT_t","STUB-T time in event", EVENT_WIDTH, 0, EVENT_WIDTH);
    hSTUBB_t = new TH1F("hSTUBB_t","STUB-B time in event", EVENT_WIDTH, 0, EVENT_WIDTH);
    hSTUBR_t = new TH1F("hSTUBR_t","STUB-R time in event", EVENT_WIDTH, 0, EVENT_WIDTH);

    hSTUBL_e_x1x2 = new TH2F("hSTUBL_e_x1x2","STUB-L x1-x2 over E vs E", 100, -2, 2, 600, 0, 60000);
    hSTUBT_e_x1x2 = new TH2F("hSTUBT_e_x1x2","STUB-T x1-x2 over E vs E", 100, -2, 2, 600, 0, 60000);
    hSTUBB_e_x1x2 = new TH2F("hSTUBB_e_x1x2","STUB-B x1-x2 over E vs E", 100, -2, 2, 600, 0, 60000);
    hSTUBR_e_x1x2 = new TH2F("hSTUBR_e_x1x2","STUB-R x1-x2 over E vs E", 100, -2, 2, 600, 0, 60000);

    hSTUBL_x1px2_e = new TH2F("hSTUBL_x1px2_e","STUB-L x1+x2 vs E", 600, 0, 60000 , 600, 0, 60000);
    hSTUBT_x1px2_e = new TH2F("hSTUBT_x1px2_e","STUB-T x1+x2 vs E", 600, 0, 60000 , 600, 0, 60000);
    hSTUBB_x1px2_e = new TH2F("hSTUBB_x1px2_e","STUB-B x1+x2 vs E", 600, 0, 60000 , 600, 0, 60000);
    hSTUBR_x1px2_e = new TH2F("hSTUBR_x1px2_e","STUB-R x1+x2 vs E", 600, 0, 60000 , 600, 0, 60000);

    hRecoilE1_e = new TH1F("hRecoilE1_e","Recoil E1 energy", MAXBIN, 0, MAXBIN);
    hRecoilE2_e = new TH1F("hRecoilE2_e","Recoil E2 energy", MAXBIN, 0, MAXBIN);
    hRecoilE3_e = new TH1F("hRecoilE3_e","Recoil E3 energy", MAXBIN, 0, MAXBIN);
    hRecoilE4_e = new TH1F("hRecoilE4_e","Recoil E4 energy", MAXBIN, 0, MAXBIN);
    hRecoilE1_t = new TH1F("hRecoilE1_t","Recoil E1 time in event", EVENT_WIDTH, 0, EVENT_WIDTH);
    hRecoilE2_t = new TH1F("hRecoilE2_t","Recoil E2 time in event", EVENT_WIDTH, 0, EVENT_WIDTH);
    hRecoilE3_t = new TH1F("hRecoilE3_t","Recoil E3 time in event", EVENT_WIDTH, 0, EVENT_WIDTH);
    hRecoilE4_t = new TH1F("hRecoilE4_t","Recoil E4 time in event", EVENT_WIDTH, 0, EVENT_WIDTH);

    hRecoilE_STUB_t = new TH1F("hRecoilE_STUB_t","RecoilE - STUB time in event", EVENT_WIDTH*2, -EVENT_WIDTH, EVENT_WIDTH);

    hSTUBL_x1x2_t = new TH1F("hSTUBL_x1x2_t","STUB-L x1-x2 time in event", EVENT_WIDTH*2, -EVENT_WIDTH, EVENT_WIDTH);
    hSTUBT_x1x2_t = new TH1F("hSTUBT_x1x2_t","STUB-T x1-x2 time in event", EVENT_WIDTH*2, -EVENT_WIDTH, EVENT_WIDTH);
    hSTUBB_x1x2_t = new TH1F("hSTUBB_x1x2_t","STUB-B x1-x2 time in event", EVENT_WIDTH*2, -EVENT_WIDTH, EVENT_WIDTH);
    hSTUBR_x1x2_t = new TH1F("hSTUBR_x1x2_t","STUB-R x1-x2 time in event", EVENT_WIDTH*2, -EVENT_WIDTH, EVENT_WIDTH);



    TTree *isstree = (TTree*)f->Get("isstree");
    struct_tree_entry issentry;
    isstree->SetBranchAddress("issentry",&issentry);

    ULong64_t n_entries = isstree->GetEntries();
    //ULong64_t n_entries = 1;
    ULong64_t fraction = n_entries/100;

    printf("Start processing events...\n");
    fflush(stdout);

    clear_event();

    for (ULong64_t i=0; i < n_entries; i++) {

        // Progress indicator
        if ( i % fraction == 0 ) {
		    percentage = (Double_t) (i+1) / (Double_t) n_entries;
		    printf("Processing %llu/%llu (%.1f percent) entries in the tree\r",i+1, n_entries, percentage*100);
		    fflush(stdout);
        }

        isstree->GetEntry(i);

        if ( prev_adc_ts && prev_adc_ts > issentry.adc_ts && errorcounter < 20){
        	printf("TIMESTAMP error! new ADC ts (0x%016llX) older than previous (0x%016llX)\n", issentry.adc_ts, prev_adc_ts);
          errorcounter++; // show only first 20 error messages
        }

        if ( prev_event_ts && issentry.global_event_ts < prev_event_ts && errorcounter < 20 ) {
            printf("TIMESTAMP error! Event ts older than previous...\n");
            errorcounter++; // show only first 20 errors messages
        }

        if (!first_global_ts) first_global_ts = issentry.global_event_ts;
        if ( !first_ever_adc_ts ) first_ever_adc_ts = issentry.adc_ts;

        if (issentry.data_id == 0) hSumQLong->Fill(issentry.adc_data);

       	n_hits++;
		switch( issentry.data_id ) {
			case 0 :
				n_qlong++; break;
			case 1 :
				n_qshort++; break;
			case 3 :
				n_finetime++; break;
			default :
				printf("ADC data has no DATA ID !!!\n");
		}

        // -----------------------------------------------------------------------------------------
        // Event building and ''triggering'' section
        //
        // Fill the event storage vector (eventhits) until the time difference between
        // the first ADC item in the event and the following ADC item is < EVENT_WIDTH
        //
        // Trigger is on any channel
        //
        if ( is_bad_hit(&issentry) ) {
            n_bad_hits++;
            continue;
        }

        if (eventhits.size() > MAXHITS) {
            printf("\n\nMax number of hits in one events reached. Ignoring event and starting a new one (ADC ts: 0x%012llX).\n\n", prev_adc_ts );
            clear_event();
            first_adc_ts_in_event = issentry.adc_ts;
            prev_adc_ts = first_adc_ts_in_event;
        }

        if ( !first_adc_ts_in_event ) {
           // handling of the very first ADC item and starting an event
        	eventhits.push_back(issentry);
        	first_adc_ts_in_event = issentry.adc_ts;
        	prev_adc_ts = first_adc_ts_in_event;
        }
        else if ( ( first_adc_ts_in_event + EVENT_WIDTH ) < issentry.adc_ts ) {
        	// event width passed -> treat the whole event and start a new one
        	hEventLength->Fill( prev_adc_ts - first_adc_ts_in_event );
            treat_event();

            // Start a new event with the current ADC item
         	eventhits.push_back(issentry);
         	first_adc_ts_in_event = issentry.adc_ts;
         	prev_adc_ts = first_adc_ts_in_event;
        }
        else {

        	eventhits.push_back(issentry);
        	prev_adc_ts = issentry.adc_ts;
        }

  	    //if (counter < 500) {
  	    //	print_hit(&issentry);
  	    //	counter++;
  	    //}

        // save the previous global timestamp just for fun
        prev_event_ts = issentry.global_event_ts;

    }

    // treat the last bunch of data in event if there is any
    if (eventhits.size() > 0) treat_event();
    for (unsigned int j = 0; j < eventhits.size(); j++) {
    	print_hit( &eventhits[j] );
    }
    clear_event();

    // Get time difference between first and last global timestamp
    Double_t diff = (Double_t)(issentry.global_event_ts - first_global_ts);
    diff *= 10.e-9; // Convert to seconds

    // Write statistics
    printf("\n RAW statistics -------- \n");
    printf("Acquisition time: %.3f seconds\n", diff);
    printf("Number   ADC words: %llu\n", n_adc);
    printf("Number   QL  words: %llu\n", n_qlong);
    printf("Number   QS  words: %llu\n", n_qshort);
    printf("Number   FT  words: %llu\n", n_finetime);
    printf("Number Trace words: %llu\n", n_traces);
    printf("Number  QL+QS+FT  words: %llu\n", n_qlong+n_qshort+n_finetime);

    printf("\n Event building statistics -------- \n");
    printf("Total number of entries (hits) in the tree: %lld\n", n_entries);
    printf("Total number of events processed: %lld\n", n_events);
    printf("Total number of good hits accepted: %lld\n", n_good_hits);
    printf("Total number of bad hits rejected: %lld\n", n_bad_hits);
    printf("Total number of items considered overrange: %lld\n", n_overrange);
    printf("Total number of items below low threshold: %lld\n", n_noise);

    fo->Write();
    fo->Close();
    f->Close();

}
