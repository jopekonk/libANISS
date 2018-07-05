# libANISS
A simple library for processing ISS data files from the MIDAS DAQ system using ROOT.

# Usage
Compile the main library (libANISS.so) by using 'make' in the main folder.
The example codes in the 'examples' folder can be run with e.g.

    root -l show.C+
    root -l stats.C+
    
To create a .root file and perform analysis with the ADC timestamps, one needs to do it in two steps.
First we will make an output ROOT tree that has all the ADC items in time order.

    root -l make_tree_onlyadcstamps.C+
Then we sort through the ROOT tree and create another analysis output ROOT tree containing all the histograms.
    
    root -l analyse_tree_onlyadcstamps.C+


---

### Acknowledgements
The library is written in the same spirit and borrows quite a lot of code from libGreat written by Nigel Warr.
