/*
ad5933-test
    Reads impedance values from the AD5933 over I2C and prints them serially.
*/

#include <Wire.h>
#include "AD5933.h"
#include <SerialCommand.h>

#define START_FREQ  (80000)
#define FREQ_INCR   (1000)
#define NUM_INCR    (40)
#define REF_RESIST  (10000)

double gain[NUM_INCR+1];
int phase[NUM_INCR+1];

struct SweepParams_type{
  unsigned long start_freq;
  unsigned long freq_incr;
  unsigned long num_incr;
  unsigned long index = 0;
} currentSweepParams;

SerialCommand sCmd(Serial);         // SerialCommand parser object on USB stream

void setup(void)
{
  // Setup callbacks for SerialCommand commands
  sCmd.addCommand("IA.RREG?",  IA_RREG_sCmd_query_handler);   // raw read register byte
  sCmd.addCommand("IA.WREG",   IA_WREG_sCmd_config_handler);  // raw write register byte
  
  sCmd.addCommand("IA.RESET!",      IA_RESET_sCmd_action_handler); // reset to default state
  sCmd.addCommand("IA.START_FREQ",  IA_START_FREQ_sCmd_config_handler); // 
  sCmd.addCommand("IA.FREQ_INCR",   IA_FREQ_INCR_sCmd_config_handler); //
  sCmd.addCommand("IA.NUM_INCR",    IA_NUM_INCR_sCmd_config_handler); //
  sCmd.addCommand("IA.SWEEP.INIT!",  IA_SWEEP_INIT_sCmd_action_handler); //starts first excitation frequency
  sCmd.addCommand("IA.SWEEP.START!", IA_SWEEP_INIT_sCmd_action_handler); //starts up ADC for measurement
  
  sCmd.setDefaultHandler(UNRECOGNIZED_sCmd_default_handler);        // Handler for command that isn't matched  (says "What?")
  //----------------------------------------------------------------------------
  // Begin I2C
  Wire.begin();

  // Begin serial at 9600 baud for output
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo/Teensy only
  }

  // Perform initial configuration. Fail if any one of these fail.
  if (!(AD5933::reset() &&
        AD5933::setInternalClock(true) &&
        AD5933::setStartFrequency(START_FREQ) &&
        AD5933::setIncrementFrequency(FREQ_INCR) &&
        AD5933::setNumberIncrements(NUM_INCR) &&
        AD5933::setPGAGain(PGA_GAIN_X1)))
        {
            Serial.println("### FAILED in initialization!");
            while (true) ;
        }
}

void loop(void)
{
  int num_bytes = sCmd.readSerial();      // fill the buffer
  if (num_bytes > 0){
    sCmd.processCommand();  // process the command
  }
  delay(10);
}


void IA_RESET_sCmd_action_handler(SerialCommand this_sCmd) {
  AD5933::reset();
}


void IA_RREG_sCmd_query_handler(SerialCommand this_sCmd) {
  int addr;
  byte data;
  bool success;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("### Error: IA.RREG? requires 1 argument (int addr)\n"));
  }
  else{
    addr = atoi(arg);
    success = AD5933::getByte(addr, &data);
    if(success){
      this_sCmd.print(data);
      this_sCmd.print('\n');
    } else{
      this_sCmd.print(F("### Error: IA.RREG? -> AD5933::getByte call failed"));
    }
  }
}

void IA_WREG_sCmd_config_handler(SerialCommand this_sCmd) {
  int addr;
  byte value;
  bool success;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("### Error: IA.WREG requires 2 arguments (int addr, byte value), none given\n"));
  }
  else{
    addr = atoi(arg);
    arg = this_sCmd.next();
    if (arg == NULL){
        this_sCmd.print(F("### Error: IA.WREG requires 2 arguments (int addr, byte value), 1 given\n"));
    }
    else{
      value = atoi(arg);
      success = AD5933::sendByte(addr, value);
      if(!success){
        this_sCmd.print(F("### Error: IA.WREG -> AD5933::sendByte call failed"));
      }
    }
  }
}

void IA_START_FREQ_sCmd_config_handler(SerialCommand this_sCmd) {
  unsigned long int value;
  bool success;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("### Error: IA.START_FREQ requires 1 arguments (unsigned long value), none given\n"));
  }
  else{
    value = atoi(arg);
    success = AD5933::setStartFrequency(value);
    if(!success){
      this_sCmd.print(F("### Error: IA.START_FREQ -> AD5933::setStartFrequency call failed"));
      currentSweepParams.start_freq = 0;
    } else{
      currentSweepParams.start_freq = value;
    }
  }
}

void IA_FREQ_INCR_sCmd_config_handler(SerialCommand this_sCmd) {
  unsigned long int value;
  bool success;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("### Error: IA.FREQ_INC requires 1 arguments (unsigned long value), none given\n"));
  }
  else{
    value = atoi(arg);
    success = AD5933::setIncrementFrequency(value);
    if(!success){
      this_sCmd.print(F("### Error: IA.FREQ_INCR -> AD5933::setIncrementFrequency call failed"));
      currentSweepParams.freq_incr = 0;
    } else{
      currentSweepParams.freq_incr = value;
    }
  }
}

void IA_NUM_INCR_sCmd_config_handler(SerialCommand this_sCmd) {
  unsigned long int value;
  bool success;
  char *arg = this_sCmd.next();
  if (arg == NULL){
    this_sCmd.print(F("### Error: IA.NUM_INC requires 1 arguments (unsigned long value), none given\n"));
  }
  else{
    value = atoi(arg);
    success = AD5933::setNumberIncrements(value);
    if(!success){
      this_sCmd.print(F("### Error: IA.NUM_INCR -> AD5933::setIncrementFrequency call failed"));
      currentSweepParams.num_incr = 0;
    } else{
      currentSweepParams.num_incr = value;
    }
  }
}

void IA_SWEEP_INIT_sCmd_action_handler(SerialCommand this_sCmd) {
  // Initialize the frequency sweep, starts excitation
  if (!(AD5933::setPowerMode(POWER_STANDBY) &&            // place in standby
        AD5933::setControlMode(CTRL_INIT_START_FREQ)))    // init start freq
  {
     Serial.println("### Error: IA.SWEEP.INIT! -> Could not initialize frequency sweep");
  }
}

void IA_SWEEP_START_sCmd_action_handler(SerialCommand this_sCmd) {
  // triggers the ADC to begin after settling cycles
  if (!AD5933::setControlMode(CTRL_START_FREQ_SWEEP))   // begin frequency sweep
  {
     Serial.println("### Error: IA.SWEEP.START! -> Could not start frequency sweep");
  }
}


// Unrecognized command
void UNRECOGNIZED_sCmd_default_handler(SerialCommand this_sCmd)
{
  SerialCommand::CommandInfo command = this_sCmd.getCurrentCommand();
  this_sCmd.print(F("### Error: command '"));
  this_sCmd.print(command.name);
  this_sCmd.print(F("' not recognized ###\n"));
}
