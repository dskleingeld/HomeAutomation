#include "StoreData.h"

StoreData::StoreData(){
	
	mkdir("data", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	//open file in binairy mode with appending write operations
  atmospherics_file = fopen("data/atmospheric.binDat", "a+b");
  pirs_file = fopen("data/pirs.binDat", "a+b");  
  //TODO maybe load buffer from file?
}

StoreData::~StoreData(){
  fclose(atmospherics_file);
  fclose(pirs_file);
}



void StoreData::write_pir(unsigned char data[4]){
	fwrite(data, 4, 4*sizeof(unsigned char), pirs_file);	
	}

void StoreData::write_atmospheric(unsigned char data[18]){ }

void StoreData::write_plants(unsigned char data[]){ }


void StoreData::read_pir(unsigned char data[4], int line){
  fseek(pirs_file, 4*(line), SEEK_SET); 
  fread(data, 1, 4, pirs_file);
  }

void StoreData::read_atmospheric(unsigned char data[18], int line){ }

void StoreData::read_plants(unsigned char data[], int line){ }

////////////////////////////////////////////////FIXME beneath here

PirData::PirData(StoreData& dataStorage){
  prevData[1] = 0;//0 as in no pirs measured
  t_begin = GetMilliSec();
  Record[0] = 0;
  Record[1] = 0;
}

bool PirData::isTimeStampPackage(unsigned char susp_time[4],  unsigned char susp_data[4]){
  if (susp_time[2] != susp_data[0]){
    if (susp_time[3] != susp_data[1]){ return true; }    
  }
  return false;
}

/*
void StoreData::pir_getBetweenTime(int T1, int T2){

  //TODO  implement software buffer for last N writes
  //TODO  implement file read/write locks but first get things working
  //TODO  in a single threaded way
  
  unsigned char[SOFT_BUFFERSIZE]
  
  //check if needed timestamp is in buffer
  
  
  
  int endOfFile = getEndOfFile();
  
  T_start = getClosestTimeStamp(1)
  T_end = getClosestTimeStamp(endOfFile)

  if (T_start < T1){
    T1_rel = T1 - T_start
    T_rel = T_end - T_start
    T1_guess = 
  
}
*/

uint32_t PirData::getClosestTimeStamp(int lineNumber){
  //takes a line number and finds the closest full timestamp before that line 
  //number returns that timestamp to the user
  
  uint32_t unix_time;
  unsigned char susp_time[4];
  unsigned char susp_data[4];
  int n = 0;

  dataStorage.read_pir(susp_time, lineNumber-n);
  do {
    std::memcpy(susp_data, susp_time, 4);
    //read packages in front of sups_data, store in susp_time
    n++;
    dataStorage.read_pir(susp_time, lineNumber-n);
  } while (!isTimeStampPackage(susp_time, susp_data));//TODO check double negative
  
  //conversion back to full unix time  
  unix_time = (uint32_t)susp_time[1] << 24 | //shift high part 
              (uint32_t)susp_time[0] << 16 | //
              (uint32_t)susp_time[3] << 8  | //shift and add low part
              (uint32_t)susp_time[2];
  
  std::cout << "Reading timestamp: "
		 			<< +susp_time[0] << " "
					<< +susp_time[1] << " "
			 		<< +susp_time[2] << " "
			 		<< +susp_time[3] << "\n";
  
  std::cout << unix_time << "\n";
  return unix_time;
}

	
long int PirData::unix_timestamp() {
  time_t t = std::time(0);
  long int now = static_cast<long int> (t);
  return now;
}

long long PirData::GetMilliSec(){
  struct timeval tp;
  gettimeofday(&tp, NULL);
  //get current timestamp in milliseconds
  long long mslong = (long long) tp.tv_sec * 1000 + tp.tv_usec / 1000; 
  return mslong;
}


void PirData::putTimestamp(long int timestamp){

  unsigned char towrite[4];
  
  //conversion to bytes
  towrite[2] = timestamp & 0xff;         //store low part in last two bytes
  towrite[3] = (timestamp >> 8) & 0xff;  
  towrite[0] = (timestamp >> 16) & 0xff; //store high part in first 2 bytes
  towrite[1] = (timestamp >> 24) & 0xff;  
  
  dataStorage.write_pir(towrite);
  std::cout << "writing TIMESTAMP PIR PACKAGE: "
  		 			<< +towrite[0] << " "
						<< +towrite[1] << " "
				 		<< +towrite[2] << " "
				 		<< +towrite[3] << "\n";
}	
	
void PirData::putData(unsigned char data[2]){	
  long int timestamp;				
  unsigned char buffer[4];
  
	timestamp = unix_timestamp();	
  uint16_t timeLow = (uint16_t) (timestamp >> 16);// shifting right 16 times gives /2^16
	
	if(!TimeStampSet_first & (timeLow <= HALFDAYSEC)){
	  //second condition is needed to get a timestamp after a restart
	  putTimestamp(timestamp);
	  TimeStampSet_first = true;
	  TimeStampSet_second = false;
	}
	else if(!TimeStampSet_second & (timeLow > HALFDAYSEC)){
	  putTimestamp(timestamp); 
	  TimeStampSet_second = true;
	  TimeStampSet_first = false;
	}

  //store timeLow in buffer
  buffer[0] = timeLow & 0xff;
  buffer[1]	= (timeLow >> 8) & 0xff;

	std::memcpy(buffer+2, data, 2);	
	//TODO call write funct
	
	dataStorage.write_pir(buffer);
  std::cout << "writing NORMAL PIR PACKAGE: "
  		 			<< +buffer[0] << " "
						<< +buffer[1] << " "
				 		<< +buffer[2] << " "
				 		<< +buffer[3] << "\n";

}


void PirData::convertNotation(unsigned char B[2]){
  unsigned char B_ones, B_zeros;

  B_ones  =  B[0] & B[1]; //if one and noted as correct (one) store as one
  B_zeros = (B[0] ^ B[1]) & B[1]; //if zero and noted as correct: if (zero and one) only if also one

  B[0] = B_ones;
  B[1] = B_zeros; //back to old notation [one or zero][correct or not]  
}

void PirData::combine(unsigned char B[2]){
  short int A_ones, A_zeros, B_ones, B_zeros;
  short int F_ones, F_zeros; //zeros is 1 if a zero was confirmed at that place

  unsigned char prevData_new[2];  
  
  //First previous runs data to new notation (kept in old for same check)
  std::memcpy(prevData_new, prevData,2);
  convertNotation(prevData_new);
  
  A_ones  = prevData[0];
  A_zeros = prevData[1];

  B_ones = B[0];
  B_zeros = B[1];
  
  F_ones = (A_ones & ~ B_zeros) | B_ones; //if was one and not zero now or if one now = one
  F_zeros = (A_zeros & ~ B_ones) | B_zeros; //if was zero and not one now or if zero now = one
                                            //also one here as one indicates a correct zero in B_zeros
  
  B[0] = F_ones;
  B[1] = F_zeros;
}

void PirData::binData(unsigned char data[2]){
  long long timepassed;
  timepassed = GetMilliSec() - t_begin;

  if (timepassed < PIR_DT){
    //add movement values to pir
    Record[0] = Record[0] | data[0]; 
    Record[1] = Record[1] | data[1];
  }
  else{
    //write values collected till now
    putData(Record);  
    t_begin = GetMilliSec();
    
    //reset pir to new values
    Record[0] = 0;
    Record[1] = 0;
  }
}

bool PirData::isNotSame(unsigned char data[2]){
  if ((data[0] == prevData[0]) & (data[1] == prevData[1])){ return false;}
  else{return true;}
}

void PirData::process(unsigned char data[2]){
  unsigned char combinedCorrect;
  unsigned char newCorrect;
  

  if (isNotSame(data)){
      
    combinedCorrect = prevData[1] & data[1]; 
    newCorrect = data[1];
    std::memcpy(prevData, data, 2); //save before notation is changed

    convertNotation(data);      
    if (combinedCorrect > newCorrect){ //would comparing with prev data increase knowledge?
      combine(data); //combine data with newer data overriding older data
    }      
    binData(data); //bin on time and write when neccesairy    
  }
}

//
