/*
 * Created by Eda Yildirim
 *  (2014 DESY)
 *
 *  email:eda.yildirim@cern.ch
 *
 *  modified by: Thomas Eichhorn thomas.eichhorn@desy.de
 */

// alibava includes ".h"
#include "AlibavaRunHeaderImpl.h"
#include "AlibavaEventImpl.h"
#include "AlibavaBaseProcessor.h"
#include "AlibavaPedNoiCalIOManager.h"

// marlin includes ".h"
#include "marlin/Processor.h"
#include "marlin/Exceptions.h"
#include "marlin/Global.h"

#if defined ( USE_AIDA ) || defined ( MARLIN_USE_AIDA )
// aida includes <.h>
#include <marlin/AIDAProcessor.h>
#endif

// lcio includes <.h>
#include <lcio.h>
#include <UTIL/CellIDEncoder.h>
#include <UTIL/CellIDDecoder.h>

// ROOT includes ".h"
#include "TObject.h"

// system includes <>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <algorithm>

using namespace std;
using namespace lcio;
using namespace marlin;
using namespace alibava;

AlibavaBaseProcessor::AlibavaBaseProcessor ( std::string processorName ) : Processor ( processorName ),
_rootObjectMap ( ),
_inputCollectionName ( ALIBAVA::NOTSET ),
_outputCollectionName ( ALIBAVA::NOTSET ),
_pedestalFile ( ALIBAVA::NOTSET ),
_calibrationFile ( ALIBAVA::NOTSET ),
_pedestalCollectionName ( ALIBAVA::NOTSET ),
_noiseCollectionName ( ALIBAVA::NOTSET ),
_chargeCalCollectionName ( ALIBAVA::NOTSET ),
_channelsToBeUsed ( ),
_skipMaskedEvents ( false ),
_numberOfSkippedEvents ( 0 ),
_chipSelection ( ),
_pedestalMap ( ),
_noiseMap ( ),
_chargeCalMap ( ),
_isPedestalValid ( false ),
_isNoiseValid ( false ),
_isCalibrationValid ( false )
{
    // Modify processor description
    _description = "AlibavaBaseProcessor";

    // Reset all the final arrays
    setAllMasksTo ( false );
}

// Checks if the root object exists in _rootObjectMap
bool AlibavaBaseProcessor::doesRootObjectExists ( std::string aHistoName )
{
    map < string,TObject* > ::const_iterator it = _rootObjectMap.find ( aHistoName );
    return it != _rootObjectMap.end ( );
}

// Input/Output Collection
// Getter and setter for _inputCollectionName
void AlibavaBaseProcessor::setInputCollectionName ( std::string inputCollectionName )
{
    _inputCollectionName = inputCollectionName;
}

std::string AlibavaBaseProcessor::getInputCollectionName ( )
{
    return _inputCollectionName;
}

// Getter and setter for _outputCollectionName
void AlibavaBaseProcessor::setOutputCollectionName ( std::string outputCollectionName )
{
    _outputCollectionName = outputCollectionName;
}

std::string AlibavaBaseProcessor::getOutputCollectionName ( )
{
    return _outputCollectionName;
}

// Pedestal and Noise
// Used to set pedestal and noise values
// Note that chipSelection has to be set first!
void AlibavaBaseProcessor::setPedestals ( )
{
    // First get selected chips
    EVENT::IntVec selectedchips = getChipSelection ( );
    if ( selectedchips.size ( ) == 0 )
    {
	streamlog_out ( ERROR5 ) << "No selected chips found! Couldn't set the pedestal, noise values!" << endl;
    }

    AlibavaPedNoiCalIOManager man;
    EVENT::FloatVec vec_float;

    // For each selected chip get and save pedestal and noise values
    for ( unsigned int ichip = 0; ichip < selectedchips.size ( ); ichip++ )
    {
	int chipnum = selectedchips[ichip];

	// If pedestalCollectionName is set
	if ( getPedestalCollectionName ( ) != string ( ALIBAVA::NOTSET ) )
	{
	    // Get pedestal for this chip
	    vec_float.clear ( );
	    vec_float = man.getPedNoiCalForChip ( _pedestalFile,_pedestalCollectionName, chipnum );
	    _pedestalMap.insert ( make_pair ( chipnum, vec_float ) );
	}
	else
	{
	    streamlog_out ( DEBUG5 ) << "The pedestal values for chip " << chipnum << " are not set, since pedestalCollectionName is not set!" << endl;
	}

	// If noiseCollectionName is set
	if ( getNoiseCollectionName ( ) != string ( ALIBAVA::NOTSET ) )
	{
	    // Get noise for this chip
	    vec_float.clear ( );
	    vec_float = man.getPedNoiCalForChip ( _pedestalFile,_noiseCollectionName, chipnum );
	    _noiseMap.insert ( make_pair ( chipnum, vec_float ) );
	}
	else
	{
	    streamlog_out ( DEBUG5 ) << "The noise values for chip " << chipnum << " are not set, since noiseCollectionName is not set!" << endl;
	}
    }
    checkPedestals ( );
}

void AlibavaBaseProcessor::checkPedestals ( )
{
    _isPedestalValid = false;
    _isNoiseValid = false;

    // First get selected chips
    EVENT::IntVec selectedchips = getChipSelection ( );
    if ( selectedchips.size ( ) == 0 )
    {
	streamlog_out ( ERROR5 ) << "No selected chips found! Couldn't set the pedestal and noise values!" << endl;
    }
    EVENT::FloatVec vec_float;

    // For each selected chip get and save pedestal and noise values
    for ( unsigned int ichip = 0; ichip < selectedchips.size ( ); ichip++ )
    {
	int chipnum = selectedchips[ichip];
	if ( getPedestalCollectionName ( ) != string ( ALIBAVA::NOTSET ) )
	{
	    // Check pedestal values for this chip
	    vec_float.clear ( );
	    vec_float = _pedestalMap[chipnum];
	    if ( int ( vec_float.size ( ) ) != ALIBAVA::NOOFCHANNELS )
	    {
		streamlog_out ( ERROR5 ) << "The pedestal values for chip " << chipnum << " are not set properly!" << endl;
	    }
	    else
	    {
		_isPedestalValid = true;
	    }
	}
	if ( getNoiseCollectionName ( ) != string ( ALIBAVA::NOTSET ) )
	{
	    // Check noise values for this chip
	    vec_float.clear ( );
	    vec_float = _noiseMap[chipnum];
	    if ( int ( vec_float.size ( ) ) != ALIBAVA::NOOFCHANNELS )
	    {
		streamlog_out ( ERROR5 ) << "The noise values for chip " << chipnum << " are not set properly!" << endl;
	    }
	    else
	    {
		_isNoiseValid = true;
	    }
	}
    }

    if ( _isPedestalValid )
    {
	streamlog_out ( MESSAGE5 ) << "The pedestal values for all selected chips are valid!" << endl;
    }
    else
    {
	streamlog_out ( WARNING5 ) << "The pedestal values for all selected chips are not set properly!" << endl;
    }
    if ( _isNoiseValid )
    {
	streamlog_out ( MESSAGE5 ) << "The noise values for all selected chips are valid!" << endl;
    }
    else
    {
	streamlog_out ( WARNING5 ) << "The noise values for all selected chips are not set properly!" << endl;
    }
}

// Pedestal
// Getter and setter for _pedestalCollectionName
void AlibavaBaseProcessor::setPedestalCollectionName ( std::string pedestalCollectionName )
{
    _pedestalCollectionName = pedestalCollectionName;
}

std::string AlibavaBaseProcessor::getPedestalCollectionName ( )
{
    return _pedestalCollectionName;
}

// To access the pedestal values of a chip
EVENT::FloatVec AlibavaBaseProcessor::getPedestalOfChip ( int chipnum )
{
    return _pedestalMap[chipnum];
}

// To access the pedestal value of a channel
float AlibavaBaseProcessor::getPedestalAtChannel ( int chipnum, int channum )
{
    if ( isPedestalValid ( ) )
    {
	EVENT::FloatVec vec_float = getPedestalOfChip ( chipnum );
	return vec_float[channum];
    }
    else
    {
	streamlog_out ( ERROR5 ) << "The pedestal values for chip " << chipnum << " is not set properly!" << endl;
	return 0;
    }
}

bool AlibavaBaseProcessor::isPedestalValid ( )
{
    return _isPedestalValid;
}

// Noise
// Getter and setter for _noiseCollectionName
void AlibavaBaseProcessor::setNoiseCollectionName ( std::string noiseCollectionName )
{
    _noiseCollectionName = noiseCollectionName;
}

std::string AlibavaBaseProcessor::getNoiseCollectionName ( )
{
    return _noiseCollectionName;
}

// To access the noise values of a chip
EVENT::FloatVec AlibavaBaseProcessor::getNoiseOfChip ( int chipnum )
{
    return _noiseMap[chipnum];
}

// To access the noise value of a channel
float AlibavaBaseProcessor::getNoiseAtChannel ( int chipnum, int channum )
{
    if ( isNoiseValid ( ) )
    {
	EVENT::FloatVec vec_float = getNoiseOfChip ( chipnum );
	return vec_float[channum];
    }
    else
    {
	// streamlog_out ( ERROR5 ) << "The noise values for chip " << chipnum << " are not set properly!" << endl;
	return 0;
    }
}

bool AlibavaBaseProcessor::isNoiseValid ( )
{
    return _isNoiseValid;
}

// Charge Calibration
// Used to set pedestal and noise values
// Note that chipSelection has to be set first!
void AlibavaBaseProcessor::setCalibration ( )
{
    // first get selected chips
    EVENT::IntVec selectedchips = getChipSelection ( );
    if ( selectedchips.size ( ) == 0 )
    {
	streamlog_out ( ERROR5 ) << "No selected chips found! Couldn't set calibration values!" << endl;
    }

    AlibavaPedNoiCalIOManager man;
    EVENT::FloatVec vec_float;

    // For each selected chip get and save pedestal and noise values
    for ( unsigned int ichip = 0; ichip < selectedchips.size ( ); ichip++ )
    {
	int chipnum = selectedchips[ichip];

	// Get charge calibration for this chip
	vec_float.clear ( );
	vec_float = man.getPedNoiCalForChip ( _calibrationFile,_chargeCalCollectionName, chipnum );
	_chargeCalMap.insert ( make_pair ( chipnum, vec_float ) );

    }
    checkCalibration ( );
}

void AlibavaBaseProcessor::checkCalibration ( )
{
    _isCalibrationValid = true;

    // First get selected chips
    EVENT::IntVec selectedchips = getChipSelection ( );
    if ( selectedchips.size ( ) == 0 )
    {
	streamlog_out ( ERROR5 ) << "No selected chips found! Couldn't set calibration values!" << endl;
	_isCalibrationValid = false;
    }
    EVENT::FloatVec vec_float;

    // For each selected chip get and save pedestal and noise values
    for ( unsigned int ichip = 0; ichip < selectedchips.size ( ); ichip++ )
    {
	int chipnum = selectedchips[ichip];

	// Check pedestal values for this chip
	vec_float.clear ( );
	vec_float = _chargeCalMap[chipnum];
	if ( int ( vec_float.size ( ) ) != ALIBAVA::NOOFCHANNELS )
	{
	    streamlog_out ( ERROR5 ) << "The charge calibration values for chip " << chipnum << " are not set properly!" << endl;
	    _isCalibrationValid = false;
	}
    }

    if ( _isCalibrationValid )
    {
	streamlog_out ( MESSAGE5 ) << "The calibration values for all selected chips are valid!" << endl;
    }
}

// Getter and setter for _chargeCalCollectionName
void AlibavaBaseProcessor::setChargeCalCollectionName ( std::string chargeCalCollectionName )
{
    _chargeCalCollectionName = chargeCalCollectionName;
}

std::string AlibavaBaseProcessor::getChargeCalCollectionName ( )
{
    return _chargeCalCollectionName;
}

// To access the charge calibration values of a chip
EVENT::FloatVec AlibavaBaseProcessor::getChargeCalOfChip ( int chipnum )
{
    return _chargeCalMap[chipnum];
}

// To access the charge calibration value of a channel
float AlibavaBaseProcessor::getChargeCalAtChannel ( int chipnum, int channum )
{
    if ( _isCalibrationValid )
    {
	EVENT::FloatVec vec_float = getChargeCalOfChip ( chipnum );
	return vec_float[channum];
    }
    else
    {
	streamlog_out ( ERROR5 ) << "The noise values for chip " << chipnum << " are not set properly!" << endl;
	return 0;
    }
}

// Others
// Getter and setter for _chipSelection
void AlibavaBaseProcessor::setChipSelection ( EVENT::IntVec chipselection )
{
    _chipSelection = chipselection;
}

EVENT::IntVec AlibavaBaseProcessor::getChipSelection ( )
{
    return _chipSelection;
}

// Get number of selected chips
unsigned int AlibavaBaseProcessor::getNumberOfChips ( )
{
    return _chipSelection.size();
}

// Returns true if the chip is in the list of selected chip
bool AlibavaBaseProcessor::isChipValid ( int ichip )
{
    for ( unsigned int i = 0; i < _chipSelection.size ( ); i++ )
    {
	if ( _chipSelection[i] == ichip ) return true;
    }
    // If it reaches here it means that the chip number is not in the selected chip numbers, then return false
    return false;
}

// Returns true if the channel number is valid
bool AlibavaBaseProcessor::isChannelValid ( int ichan )
{
    if ( ichan < ALIBAVA::NOOFCHANNELS )
    {
	return true;
    }
    else
    {
	return false;
    }
}

// To access the noise value of a channel
bool AlibavaBaseProcessor::isMasked ( int ichip, int ichan )
{
    // If channels to be used not identified use all channels
    if ( _channelsToBeUsed.size ( ) == 0 )
    {
	return false;
    }
    else if ( isChipValid ( ichip ) && isChannelValid ( ichan ) )
    {
	return _isMasked[ichip][ichan];
    }
    else
    {
	streamlog_out ( ERROR5 ) << "Trying to access mask value of non existing chip: " << ichip << " channel: " << ichan << ". Returning zero." << endl;
	return 0;
    }
}

void AlibavaBaseProcessor::setChannelsToBeUsed ( )
{
    // Let's decode this StringVec

    /* The format of _channelsToBeUsed string parameter should be like
     * $0:5-20$ $0:30-100$ $1:50-70$ means from chip 0 channels between 5-20 and 30-100, from chip 1 channels between 50-70
     * will be used (all numbers included).
     * The rest will be masked and not used
     * Note that the numbers should be in ascending order
    */
    streamlog_out ( DEBUG5 ) << "Setting channels to be used!" << endl;

    // First mask all channels, later we will unmask the ones selected
    setAllMasksTo ( true );
    streamlog_out ( DEBUG5 ) << "All channels masked for now!" << endl;

    // Now loop over strings to decode it
    for ( unsigned int istr = 0; istr < _channelsToBeUsed.size ( ); istr++ )
    {
	string istring = _channelsToBeUsed[istr];
	int onchip, fromchannel, tochannel;
	decodeMaskingString ( istring, &onchip, &fromchannel, &tochannel );
	streamlog_out ( DEBUG5 ) << "Processing channel selection: " << istring << " ... un-masking channels from " << fromchannel << " to " << tochannel << " on chip " << onchip << endl;
	if ( isMaskingValid ( onchip, fromchannel, tochannel ) )
	{
	    // Unmask the selected channels
	    for ( int ichan = fromchannel; ichan < tochannel + 1; ichan++ )
	    {
		_isMasked[onchip][ichan]=false;
	    }
	}
    }
    printChannelMasking ( );
}

void AlibavaBaseProcessor::decodeMaskingString ( string istring, int *onchip, int *fromchannel, int *tochannel )
{
    // Lets first set everything to -1 to check later
    *onchip = -1;
    *fromchannel = -1;
    *tochannel = -1;

    size_t tmppos = 0;
    string tmpstring;
    while ( tmppos < istring.size ( ) )
    {

	// First find the pos of " character
	tmppos = istring.find ( "$", 0 );
	tmppos = tmppos+1;

	// Find the chip number
	tmpstring = getSubStringUpToChar ( istring, ":", tmppos );
	*onchip = atoi ( tmpstring.c_str ( ) );
	tmppos = tmppos + tmpstring.size ( ) + 1;

	// Start chan
	tmpstring = getSubStringUpToChar ( istring, "-", tmppos );
	*fromchannel = atoi ( tmpstring.c_str ( ) );
	tmppos = tmppos + tmpstring.size ( ) + 1;

	if ( tmppos >= istring.size ( ) )
	{
	    break;
	}

	// End chan
	tmpstring = getSubStringUpToChar ( istring, "$", tmppos );
	tmppos = tmppos + tmpstring.size ( ) + 1;
	*tochannel = atoi ( tmpstring.c_str ( ) );
    }
}

bool AlibavaBaseProcessor::isMaskingValid ( int onchip, int fromchannel, int tochannel )
{

    bool isValid = true;

    // Check that channel ranges read correctly
    // Channels should be in ascending order!
    if ( !isChipValid ( onchip ) )
    {
	streamlog_out ( ERROR5 ) << "Chip selection on channel masking is not valid on chip: " << onchip << endl;
	isValid = false;
    }
    if ( !isChannelValid ( fromchannel ) || !isChannelValid ( tochannel ) )
    {
	streamlog_out ( ERROR5 ) << "Channel selection on channel masking is not valid!" << endl;
	isValid = false;
    }
    // Start channel cannot be bigger than end chan
    if ( fromchannel > tochannel )
    {
	streamlog_out ( ERROR5 ) << "Channel range on channel masking is not valid! Channels should be in ascending order!" << endl;
	isValid = false;
    }
    return isValid;
}

void AlibavaBaseProcessor::printChannelMasking ( )
{
    int channelprintnum = 16;

    streamlog_out ( MESSAGE5 ) << "ChannelsToBeUsed set as: ";
    for ( unsigned int istr = 0; istr < _channelsToBeUsed.size ( ); istr++ )
    {
	streamlog_out ( MESSAGE5 ) << _channelsToBeUsed[istr] << ", ";
    }
    streamlog_out ( MESSAGE5 ) << endl;

    streamlog_out ( MESSAGE5 ) << "****************************" << endl;
    streamlog_out ( MESSAGE5 ) << "* Applied Channel Masking: *" << endl;

    for ( unsigned int i = 0; i < _chipSelection.size ( ); i++ )
    {
	int ichip = _chipSelection[i];
	streamlog_out ( MESSAGE5 ) << "*** Chip " << ichip << " ***";
	for ( int ichan = 0; ichan < ALIBAVA::NOOFCHANNELS; )
	{
	    if ( ichan % channelprintnum == 0 )
	    {
		streamlog_out ( MESSAGE5 ) <<endl;
		streamlog_out ( MESSAGE5 ) << " Channels " << ichan << " - " << ichan + channelprintnum - 1 << " : ";
	    }
	    streamlog_out ( MESSAGE5 ) << isMasked ( ichip, ichan ) << " ";
	    ichan++;
	}
	streamlog_out ( MESSAGE5 ) << endl;
    }
    streamlog_out ( MESSAGE5 ) << "****************************" << endl;
}

// Returns the min chip number
unsigned int AlibavaBaseProcessor::getMinChipNumber ( )
{
    // The chipSelection should be in ascending order!
    // This is guaranteed with AlibavaConverter::checkIfChipSelectionIsValid ( )
    return _chipSelection.front ( );
}

// Returns the max chip number
unsigned int AlibavaBaseProcessor::getMaxChipNumber ( )
{
    // The chipSelection should be in ascending order!
    // This is guaranteed with AlibavaConverter::checkIfChipSelectionIsValid ( )
    return _chipSelection.back ( );
}

void AlibavaBaseProcessor::setAllMasksTo ( bool abool )
{
    for ( int i = 0; i < ALIBAVA::NOOFCHIPS; i++ )
    {
	for ( int j = 0; j < ALIBAVA::NOOFCHANNELS; j++ )
	{
	    _isMasked[i][j] = abool;
	}
    }
}

// Only valid for AlibavaData not for AlibavaClusters
int AlibavaBaseProcessor::getChipNum ( TrackerDataImpl * trkdata )
{
    CellIDDecoder < TrackerDataImpl > chipIDDecoder ( ALIBAVA::ALIBAVADATA_ENCODE );
    int chipnum = static_cast < int > ( chipIDDecoder ( trkdata ) [ALIBAVA::ALIBAVADATA_ENCODE_CHIPNUM] );
    return chipnum;
}
