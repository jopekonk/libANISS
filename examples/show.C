// Script to show the contents of a ISS data format file

#include <cstdlib>
#include <vector>
#include <algorithm>
#include <signal.h>

#include <TStyle.h>
#include <TCanvas.h>
#include <TH1I.h>
#include <TAxis.h>

#include "ISSBuffer.hh"
#include "ISSFile.hh"
#include "ISSWord.hh"
#include "ISSHit.hh"

//-----------------------------------------------------------------------------
// Treat a single buffer
void treat_buffer(ISSBuffer *b) {
   
   static ISSWord w;

   // Show buffer information
   b->Show();
   
   // Loop over words in buffer
   for (UInt_t i = 0; i < b->GetNWords(); i++) {

      // Set up word
      w.Set(b->GetWord(i));

      // Show word information
      w.Show();
   }
}

//-----------------------------------------------------------------------------
// Treat a single file
void treat_file(const Char_t *filename) {

   // Open file
   ISSFile f(filename);

   // Show file information
   f.Show();
   
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
// Show the contents of a ISS file
void show(const Char_t *infile = "../../ISS_R6_0") {

   // Stop root spewing *** Break *** write on a pipe with no one to read it
   // if we interrupt it
   signal(SIGPIPE, (sighandler_t)exit);

   // Treat the file
   treat_file(infile);
}

