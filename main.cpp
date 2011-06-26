/**
Player 
*/

#include <CoreServices/CoreServices.h> //for file stuff
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h> //for AUGraph
#include <unistd.h> // used for usleep...
#include <iostream>
#include <boost/thread.hpp>  
#include <boost/shared_ptr.hpp>

#include "buffer.h"

/** * Baud rate for use with TG_Connect() and TG_SetBaudrate(). */
#define TG_BAUD_1200 1200   
#define TG_BAUD_2400 2400
#define TG_BAUD_4800 4800
#define TG_BAUD_9600 9600
#define TG_BAUD_57600 57600
#define TG_BAUD_115200	115200

/** * Data format for use with TG_Connect() and TG_SetDataFormat(). */
#define TG_STREAM_PACKETS	0 
#define TG_STREAM_5VRAW	1 
#define TG_STREAM_FILE_PACKETS 2

/** * Data type that can be requested from TG_GetValue(). */
#define TG_DATA_BATTERY 0 
#define TG_DATA_POOR_SIGNAL 1 
#define TG_DATA_ATTENTION 2
#define TG_DATA_MEDITATION 3
#define TG_DATA_RAW 4
#define TG_DATA_DELTA 5
#define TG_DATA_THETA 6
#define TG_DATA_ALPHA1 7
#define TG_DATA_ALPHA2 8
#define TG_DATA_BETA1 9
#define TG_DATA_BETA2 10
#define TG_DATA_GAMMA1 11
#define TG_DATA_GAMMA2 12
#define TG_DATA_BLINK_STRENGTH 37
#define TG_END_NOTE 13

int g_durations[] = {
  800000, 
  400000, 
  400000, 
  400000, 
  400000, 
  200000,
  200000, 
  200000, 
  200000, 
  200000, 
  200000, 
  200000, 
  100000, 
  100000, 
  100000, 
  50000
};
int g_num_durations = 16;


int   (*TG_GetDriverVersion)() = NULL; 
int   (*TG_GetNewConnectionId)() = NULL;
int   (*TG_Connect)(int, const char *, int, int) = NULL; 
int   (*TG_ReadPackets)(int, int) = NULL; 
float (*TG_GetValue)(int, int) = NULL; 
int   (*TG_Disconnect)(int) = NULL; 
void  (*TG_FreeConnection)(int) = NULL;
int 	(*TG_EnableBlinkDetection)(int, int) = NULL;

using namespace std;

// This call creates the Graph and the Synth unit...
OSStatus	CreateAUGraph (AUGraph &outGraph, AudioUnit &outSynth) {
	OSStatus result;
	//create the nodes of the graph
	AUNode synthNode, limiterNode, outNode;
	
	AudioComponentDescription cd;
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;
  
	require_noerr (result = NewAUGraph (&outGraph), home);
  
	cd.componentType = kAudioUnitType_MusicDevice;
	cd.componentSubType = kAudioUnitSubType_DLSSynth;
  
	require_noerr (result = AUGraphAddNode (outGraph, &cd, &synthNode), home);
  
	cd.componentType = kAudioUnitType_Effect;
	cd.componentSubType = kAudioUnitSubType_PeakLimiter;  
  
	require_noerr (result = AUGraphAddNode (outGraph, &cd, &limiterNode), home);
  
	cd.componentType = kAudioUnitType_Output;
	cd.componentSubType = kAudioUnitSubType_DefaultOutput;  
	require_noerr (result = AUGraphAddNode (outGraph, &cd, &outNode), home);
	
	require_noerr (result = AUGraphOpen (outGraph), home);
	
	require_noerr (result = AUGraphConnectNodeInput (outGraph, synthNode, 0, limiterNode, 0), home);
	require_noerr (result = AUGraphConnectNodeInput (outGraph, limiterNode, 0, outNode, 0), home);
	
	// ok we're good to go - get the Synth Unit...
	require_noerr (result = AUGraphNodeInfo(outGraph, synthNode, 0, &outSynth), home);
  
home:
	return result;
}

// some MIDI constants:
enum {
	kMidiMessage_ControlChange 		= 0xB,
	kMidiMessage_ProgramChange 		= 0xC,
	kMidiMessage_BankMSBControl 	= 0,
	kMidiMessage_BankLSBControl		= 32,
	kMidiMessage_NoteOn 			    = 0x9
};

typedef boost::shared_ptr< con_buffer< std::vector < int > > > input_buffer;

// Play notes based on what you get.
// Middle C = 69
int run_keyboard(AudioUnit* synthUnit, input_buffer buffer, UInt8 midiChannel) {
  OSStatus            result;
  UInt32              major[] = {60,62,64,65,67,69,71};
  UInt32              minor[] = {61,63,66,68,70};
  UInt32              noteSteps[] = {-2,-1,0,1,2};
  UInt32              nextNote;
  UInt32              nextDuration;
  UInt32              lastNote = 67;
  
  std::cout << "starting keyboard." << std::endl;
  while (true) {
    vector <int> data;
    buffer->wait_and_pop(data);
    if (data[TG_END_NOTE]) {
      std::cout << "ending keyboard." << std::endl;
      return 1;  
    }
    
    
//    nextNote = (rand() % 100 > 15)? major[data[TG_DATA_ATTENTION] % 7]: minor[data[TG_DATA_ATTENTION] % 5];
    int nextStep = noteSteps[data[TG_DATA_ATTENTION] % 5];
    nextNote += nextStep;
    if (nextNote > 71) nextNote = 71;
    if (nextNote < 60) nextNote = 60;
    
    nextDuration = 200000;
//    nextDuration = g_durations[data[TG_DATA_DELTA] % g_num_durations];
    
    cout << "Keyboard: " << nextNote << " " << nextDuration << endl;
    
    UInt32 onVelocity = data[TG_DATA_MEDITATION] + 50; // 0-127
    UInt32 noteOnCommand = 	kMidiMessage_NoteOn << 4 | midiChannel;
    require_noerr (result = MusicDeviceMIDIEvent(*synthUnit, noteOnCommand, nextNote, onVelocity, 0), finished);
    usleep (nextDuration);      
    require_noerr (result = MusicDeviceMIDIEvent(*synthUnit, noteOnCommand, nextNote, 0, 0), finished);
  }
  
finished:
  
  return 0;
}

int run_drums(AudioUnit* synthUnit, input_buffer buffer, UInt8 midiChannel) {
  OSStatus            result;
  UInt32              notes[] = {40,50,60,70,80,90,100};
  
  std::cout << "starting drums." << std::endl;
  while (true) {
    vector <int> data;
    buffer->wait_and_pop(data);
    if (data[TG_END_NOTE]) {
      std::cout << "ending drums." << std::endl;
      return 1;  
    }
    
    UInt32 noteNum = data[TG_DATA_ATTENTION] % 7;
    UInt32 onVelocity = data[TG_DATA_MEDITATION] + 50; // 0-127
    UInt32 noteOnCommand = 	kMidiMessage_NoteOn << 4 | midiChannel;
    cout << "Dums: " << noteNum << endl;
    require_noerr (result = MusicDeviceMIDIEvent(*synthUnit, noteOnCommand, notes[noteNum], onVelocity, 0), finished);
    usleep (30000);      
    require_noerr (result = MusicDeviceMIDIEvent(*synthUnit, noteOnCommand, notes[noteNum], 0, 0), finished);
  }
  
finished:
  
  return 0;
}

int main (int argc, const char * argv[]) {
	AUGraph             keyboardGraph = 0;
  AUGraph             drumGraph = 0;
	AudioUnit           keyboardUnit;
  AudioUnit           drumUnit;
	OSStatus            result;
  CFURLRef            bundleURL; 
  CFBundleRef         thinkGearBundle; // bundle reference
  int                 connectionID = -1;	// ThinkGear connection handle /*

  const char*         portname = "/dev/tty.MindWave"; 
	char*               keyboardPath = 0;
  char*               drumPath = 0;
  int                 numPackets = 0; 
  float               signalQuality = 0.0;
  float               blink = 0.0;
  float               lastBlink = 0.0;
  
  input_buffer        drumBuffer(new con_buffer< std::vector < int > > ());
  input_buffer        keyboardBuffer(new con_buffer< std::vector < int > > ());
  FSRef               fsRefKeyboard;
  FSRef               fsRefDrums;
  
	UInt8               keyboardChannel = 0;
  UInt8               drumChannel = 1; 
  
  keyboardPath = const_cast<char*>("/Users/ian/Library/Audio/Sounds/Banks/clivicord.sf2"); // aggorg.sf2 or clivicord.sf2
  drumPath = const_cast<char*>("/Users/ian/Library/Audio/Sounds/Banks/drums.sf2");
	bundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, 
                                            CFSTR("/System/Library/MindLink/ThinkGear.bundle"),
                                            kCFURLPOSIXPathStyle, 
                                            true);
	thinkGearBundle = CFBundleCreate(kCFAllocatorDefault, bundleURL);
  srand ( time(NULL) );
  
	if (thinkGearBundle == NULL) {
    std::cout << "Missing /System/Library/MindLink/ThinkGear.bundle" << std::endl;
    return -1;
	}
  
  TG_GetDriverVersion     =  (int(*)())CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_GetDriverVersion"));
  TG_GetNewConnectionId   =  (int(*)())CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_GetNewConnectionId"));
  TG_Connect              =  (int(*)(int, const char *, int, int))CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_Connect"));
  TG_ReadPackets          =  (int(*)(int, int))CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_ReadPackets"));
  TG_GetValue             =  (float(*)(int, int))CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_GetValue"));
  TG_Disconnect           =  (int(*)(int))CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_Disconnect"));
  TG_FreeConnection       =  (void(*)(int))CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_FreeConnection"));
  TG_EnableBlinkDetection = (int(*)(int, int))CFBundleGetFunctionPointerForName(thinkGearBundle, CFSTR("TG_EnableBlinkDetection"));
  
  if(!TG_GetDriverVersion 
     || !TG_GetNewConnectionId 
     || !TG_Connect 
     || !TG_ReadPackets 
     || !TG_GetValue 
     || !TG_Disconnect 
     || !TG_EnableBlinkDetection 
     || !TG_FreeConnection){
    std::cerr << "Error: Expected functions in ThinkGear.bundle were not found. Are you using the right version?" << std::endl;
    exit(1);
  }
  
  connectionID = TG_GetNewConnectionId();
  std::cout << "Running With ID: " << connectionID << std::endl;
  
  boost::thread keyboardPlayer(&run_keyboard, &keyboardUnit, keyboardBuffer, keyboardChannel);
  boost::thread drumPlayer(&run_drums, &drumUnit, drumBuffer, drumChannel);
  
	require_noerr (result = CreateAUGraph (keyboardGraph, keyboardUnit), home);
  require_noerr (result = CreateAUGraph (drumGraph, drumUnit), home);
	require_noerr (result = FSPathMakeRef ((const UInt8*)keyboardPath, &fsRefKeyboard, 0), home);
  require_noerr (result = FSPathMakeRef ((const UInt8*)drumPath, &fsRefDrums, 0), home);
  
  // malloc the units.
 	require_noerr (result = AudioUnitSetProperty (keyboardUnit,
                                                kMusicDeviceProperty_SoundBankFSRef,
                                                kAudioUnitScope_Global, 0,
                                                &fsRefKeyboard, sizeof(fsRefKeyboard)), home);
  require_noerr (result = AudioUnitSetProperty (drumUnit,
                                                kMusicDeviceProperty_SoundBankFSRef,
                                                kAudioUnitScope_Global, 0,
                                                &fsRefDrums, sizeof(fsRefDrums)), home);
   
	// ok we're set up to go - initialize and start the graph
	require_noerr (result = AUGraphInitialize (keyboardGraph), home);
  require_noerr (result = AUGraphInitialize (drumGraph), home);
  
  //set our banks  
	require_noerr (result = MusicDeviceMIDIEvent(keyboardUnit, 
                                               kMidiMessage_ProgramChange << 4 | keyboardChannel, 
                                               kMidiMessage_BankMSBControl, 0,
                                               0), home);
    
	require_noerr (result = MusicDeviceMIDIEvent(drumUnit, 
                                               kMidiMessage_ProgramChange << 4 | drumChannel, 
                                               kMidiMessage_BankMSBControl, 0,
                                               0), home);
  
	require_noerr (result = AUGraphStart (keyboardGraph), home);
  require_noerr (result = AUGraphStart (drumGraph), home);
  require_noerr (result = TG_Connect(connectionID, portname, TG_BAUD_57600, TG_STREAM_PACKETS), home);
  require_noerr (result = TG_EnableBlinkDetection(connectionID, 1), home);
  
  /**
  int ii;
  for (ii = 0; ii<13; ii++) {
    UInt32 noteOnCommand = 	kMidiMessage_NoteOn << 4 | midiChannelInUse;
    require_noerr (result = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, ii+60, 127, 0), home);
    usleep (1 * 1000 * 1000);      
    require_noerr (result = MusicDeviceMIDIEvent(synthUnit, noteOnCommand, ii+60, 0, 0), home);  
  }
  */
  
  std::cout << "connected." << std::endl;
  usleep(100000);
  
  while(numPackets<100) {    
//    numPackets = TG_ReadPackets(connectionID, -1);
    cout << "i: " << numPackets << endl;
    
    vector <int> dataHolder(TG_END_NOTE + 1);
    dataHolder[TG_DATA_ATTENTION] = rand()%127;
    dataHolder[TG_DATA_MEDITATION] = rand()%127;
    dataHolder[TG_DATA_DELTA] = rand()%80000;
    dataHolder[TG_END_NOTE] = 0;
    if ((rand() % 2) + 1 > 1) {
      keyboardBuffer->push(dataHolder);
    } else {
     // drumBuffer->push(dataHolder);      
    }
    
/**    
    if(numPackets > 0){
      signalQuality = TG_GetValue(connectionID, TG_DATA_POOR_SIGNAL); 
      
      
      //UInt32 noteNum = attention + 20; // 0-127
      //UInt32 onVelocity = meditation + 50; // 0-127
     
      //fprintf(stdout, "\nSig: %3.0f, Att: %3.0f, Med: %3.0f, D: %7.0f, T: %7.0f, A: %7.0f, G: %7.0f, B: %7.0f, Blink: %3.0f", 
      //        signalQuality, attention, meditation, delta, theta, alpha, gamma, beta, blink);
      //fflush(stdout);
      
      cout << "signalQuality: " << signalQuality << endl;
      
      if (signalQuality < 52) {
        vector <int> dataHolder(TG_END_NOTE + 1);
        
        dataHolder[TG_DATA_ATTENTION] = TG_GetValue(connectionID, TG_DATA_ATTENTION);
        dataHolder[TG_DATA_MEDITATION] = (int)TG_GetValue(connectionID, TG_DATA_MEDITATION);
        dataHolder[TG_DATA_DELTA] = (int)TG_GetValue(connectionID, TG_DATA_DELTA);
        dataHolder[TG_DATA_THETA] = (int)TG_GetValue(connectionID, TG_DATA_THETA);
        dataHolder[TG_DATA_ALPHA1] = (int)TG_GetValue(connectionID, TG_DATA_ALPHA1);
        dataHolder[TG_DATA_GAMMA1] = (int)TG_GetValue(connectionID, TG_DATA_GAMMA1);
        dataHolder[TG_DATA_ATTENTION] = (int)TG_GetValue(connectionID, TG_DATA_ATTENTION);
        dataHolder[TG_DATA_ATTENTION] = (int)TG_GetValue(connectionID, TG_DATA_ATTENTION);
        dataHolder[TG_END_NOTE] = 0;
        
        blink = TG_GetValue(connectionID, TG_DATA_BLINK_STRENGTH);  
        
        if (abs(lastBlink - blink) > 10) {
          drumBuffer->push(dataHolder);
        } else {
          keyboardBuffer->push(dataHolder);
        }
        lastBlink = blink;
      }
    }
 */
    numPackets++;
    usleep (10000);        
  }
  
home:
  
  vector <int> dataHolder(TG_END_NOTE + 1);
  dataHolder[TG_END_NOTE] = 1;
  drumBuffer->push(dataHolder);
  keyboardBuffer->push(dataHolder);
  keyboardPlayer.join();
  drumPlayer.join();
  
	if (keyboardGraph) {
		AUGraphStop (keyboardGraph);
		DisposeAUGraph (keyboardGraph);
	}
  if (drumGraph) {
		AUGraphStop (drumGraph);
		DisposeAUGraph (drumGraph);
	}
      
  TG_Disconnect(connectionID); 
  TG_FreeConnection(connectionID);
  
  CFRelease(bundleURL); 
  CFRelease(thinkGearBundle);
  
	return result;
}


