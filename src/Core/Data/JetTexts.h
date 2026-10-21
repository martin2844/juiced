#pragma once
// The lines of scripts\jettext<language letter>.txt the code reads (the save device's and the network's texts, read
// by 0x004FAC80 as Core/Data/Language.h's TextTable reads them), by their number, each with its English text. The
// save device's flag k shows line kJetFlags + k. [0x...]: the stock's global the code reads the line from.

#include <cstddef>

namespace oj {

enum JetText : size_t {
    kJetNoAutosave = 0,          // "Do you want to continue without saving? This will disable Autosave."
    kJetAutosaveDisabled = 1,    // "Autosave is disabled. It will be enabled after the next save."
    kJetPleaseWait = 3,          // "Please wait..."
    kJetAtLeast = 4,             // " At least "
    kJetAreRequired = 5,         // " are required"
    kJetHiScoresNotLoaded = 13,  // "Unable to load hi-scores!"
    kJetCannotSave = 14,         // "You will not be able to save your progress."
    kJetFlags = 63,              // flag 0's ("Checking hard disk..."); flag k's 63 + k
    kJetSignedOnElsewhere = 107,  // [0x0075F09C] "You have been disconnected from GameSpy because another person .."
    kJetNoLongerAvailable = 108,  // [0x0075F0A0] "This game session is no longer available."
    kJetCreateOne = 109,          // [0x0075F0A4] "No game sessions found. Do you want to create one?"
    kJetNetworkLost = 110,        // [0x0075F0A8] "Connection to network lost. Please try again."
    kJetDirectInputProblem = 111,  // "There is a problem with your DirectInput registry settings."
    kJetSeeReadMe = 112,           // "Please see the ReadMe file for further assistance."
    kJetDirectInputTitle = 113,    // "DirectInput problem detected"
};

}  // namespace oj
