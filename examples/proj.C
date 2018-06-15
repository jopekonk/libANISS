// Script to generate the projections of a file. For each active channel,
// it produces a histogram. The binning of the histogram is the raw binning
// of the Nutaq module, but the bin widths are set according to the
// calibration. The format of the calibration file is the same as that for
// grain. i.e. with lines like
// 32= 1.890477 0.28199703 2.622174E-7 0
// which means for ID=32 (ID=module*32+channel-1, so this is channel 1 of
// module 1). The next three parameters are offset, slope and quadratic term
// of the calibration and the last term is a time offset, which is not used
// by this code.

#include <vector>
#include <algorithm>

#include <TFile.h>
#include <TH1I.h>
#include <TString.h>
#include <TAxis.h>

#include "ISSBuffer.hh"
#include "ISSFile.hh"
#include "ISSWord.hh"

#define MAXID 0x1000      // Maximum number of IDs with 12 bits

// Data from calibration file
Double_t calib[MAXID][3]; // Quadratic calibration terms
Int_t offset[MAXID];      // Time offset in ticks

// Histogram
TH1I *hStats, *h[MAXID];

//-----------------------------------------------------------------------------
// Calibrate a histogram by setting the bin widths appropriately. This leaves
// the binning unchanged.
void Calibrate(TH1 *myh, Double_t cal[3]) {

   int nbins;
   Double_t *bins;
   int i;

   // If the slope is zero, do nothing
   if (cal[1] == 0) return;

   // Generate bins
   nbins = myh->GetNbinsX();
   bins = (Double_t *)calloc(nbins + 1, sizeof(Double_t));
   for (i = 0; i <= nbins; i++)
     bins[i] = cal[0] + (Double_t)i * cal[1] +
     (Double_t)i * (Double_t)i * cal[2];

   // Set bins
   myh->SetBins(nbins, bins);

   // Free
   free(bins);

   return;
}

//-----------------------------------------------------------------------------
// Read the calibration file in the grain format.
void read_calibration(const char *filename = "online.gains") {

   TString s;
   FILE *fp;
   Int_t status, id, count = 0;
   Double_t cal[4];

   // Open the file
   fp = fopen(filename, "r");
   if (!fp) return;

   printf("Reading calibrations...\r");
   fflush(stdout);
   while(1) {
   
      // Read a line
      if (!s.Gets(fp)) break;
   
      // Parse the line: ID=offset slope quadratic time_offset
      status = sscanf(s.Data(), "%d=%lf%lf%lf%lf", &id, cal, cal+1, cal+2,
                      cal+3);
      if (status != 5) continue;
      if (id < 0 || id >= MAXID) continue;
      count++;

      // Store it
      for (Int_t i = 0; i < 3; i++) calib[id][i] = cal[i];
      offset[id] = (Int_t)cal[3];
   }
   printf("Read calibrations for %d channels.\n", count);
   
   // Close the file
   fclose(fp);
}
//-----------------------------------------------------------------------------
// Treat a single word
void treat_word(ISSWord *w) {

   // Ignore all but ADC words
   if (!w->IsADC()) return;
   
   // Ignore pile-up, overrange, underrange, overflow and underflow
   //if (w->GetADCFail()) return;

   // Add to statistics
   UInt_t id = w->GetADCID();
   hStats->AddBinContent(id, 1);

   // Fill projection histogram
   h[id]->Fill(w->GetADCConversion());
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

   // Open file
   ISSFile f(filename);
   
   // Loop over blocks
   for (UInt_t i = 0; i < f.GetNBlocks(); i++) {

      // Set up the buffer with that block
      ISSBuffer b(f.GetBlock(i));

      // Treat the buffer
      treat_buffer(&b);
   }

   // Close file
   f.Close();
}

//-----------------------------------------------------------------------------
// Generate projections for a file
void proj(const Char_t *infile = "../ISS_R6_0",
          const Char_t *outfile = "proj.root",
          const Char_t *calname = "online.gains") {

   // Read the calibration
   read_calibration(calname);
   
   // Open output file
   TFile *f = TFile::Open(outfile, "recreate");
   if (!f) return;

   // Create histograms
   hStats = new TH1I("hStats", "Statistics", MAXID, 0, MAXID);
   for (UInt_t i = 0; i < MAXID; i++)
     h[i] = new TH1I(Form("h%04d", i),
                     Form("Module %d channel %-2d (%s)", (i / 32),
                          (i & 0xF) + 1, (i & 0x10) ? "baseline" : "energy"),
                     100000, 1, 100000);
   
   // Treat the file
   treat_file(infile);

   // Delete empty histograms and calibrate the others
   for (UInt_t i = 0; i < MAXID; i++)
     if (h[i]->Integral() > 0) Calibrate(h[i], calib[i]);
     else                      delete h[i];

   // Write everything
   f->Write();

   // Write statistics
   printf("ID    Integral (excl. failures)\n");
   for (UInt_t i = 0; i < MAXID; i++) {
      UInt_t integral = hStats->GetBinContent(i);
      if (integral > 0) printf("%-5d %d\n", i, integral);
   }

   // Close output root file
   f->Close();
}
