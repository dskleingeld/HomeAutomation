#include "mainState.h"

//decode url to a command to change a state or pass to a function
void MainState::httpSwitcher(const char* raw_url){

	if(0 == strcmp(raw_url, "/lamps")){
		std::cout<<"lamps call has been made\n";
		
	}

}

void MainState::parseCommand(Command toParse){
	
	switch(toParse){
		case LIGHTS_ALLON:
		
		break;
		
		case LIGHTS_ALLOFF:
		
		break;
		
		
	}
}

MainState::MainState(){
	
	lightValues = std::make_shared<std::array<int, 5>>();
	lightValues_updated = std::make_shared<int>();
	lightValues_mutex = std::make_shared<std::mutex>();
	
	userState = std::make_shared<user>();
	userState_updated= std::make_shared<int>();
	userState_mutex = std::make_shared<std::mutex>();
}



void MainState::pre_scan(){}

//determine if and if so which command should be send to the ..
void MainState::update_lights(){}

void MainState::update_music(){}

void MainState::update_computer(){}
//end of determining functions
