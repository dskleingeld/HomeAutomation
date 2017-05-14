#include "mainState.h"
#include "majorStates/default.h"
#include "majorStates/GoingToSleep.h"
#include "majorStates/SleepInterrupt.h"
#include "majorStates/Minimal.h"

inline void startNewState(State* currentState, StateData &stateData){
	switch(stateData.newState){
		case AWAY:
		//currentState = new Default();
		break;			
		case SLEEPING:
		break;
		case DEFAULT_S:
		currentState = new Default(stateData);
		break;
		case GOINGTOSLEEP_S:
		currentState = new GoingToSleep(stateData);
		break;
		case SLEEPINTERRUPT_S:
		currentState = new SleepInterrupt(stateData);
		break;
		case MINIMAL_S:
		currentState = new Minimal(stateData);
		break;
		case WAKEUP:
		//currentState = new Wakeup();
		break;
	}
}

void thread_state_management(std::shared_ptr<std::atomic<bool>> notShuttingdown,
	SignalState* signalState, SensorState* sensorState, MpdState* mpdState, 
	Mpd* mpd, HttpState* httpState){

	StateData stateData(sensorState, mpdState, mpd, httpState);
	State* currentState = new Default(stateData);
	
	std::unique_lock<std::mutex> lk(signalState->m);
	while(*notShuttingdown){
		signalState->cv.wait(lk);//wait for new sensor data or forced update.
		std::cout<<"running update\n";		

		stateData.currentTime = (uint32_t)time(nullptr);
		if(currentState->stillValid()){
			currentState->updateOnSensors();
			if(httpState->updated){
				currentState->updateOnHttp();
				delete currentState;
				startNewState(currentState, stateData);				
			}
		}
		else{
			delete currentState;
			startNewState(currentState, stateData);
		}
	}
}
