#include "telegramBot.h"

//def enableWebhook():
//#   enable the webhook and upload the certificate 
    //params = {'url': 'https://deviousd.duckdns.org:8443/'}
    //r = requests.get("https://api.telegram.org/bot"+config.token+"/setWebhook", 
                      //params=params,
                      //files={'certificate' : open('/home/pi/bin/homeAutomation/data/PUBLIC.pem', 'r')})
    //print("server replies:",r.json())

std::string BotToken = "109451485:AAHAOt1NP3V8zK7TTkcIlwn-ofd7ubta08E";
std::string baseBotUrl = "https://api.telegram.org/bot";
std::string baseBotAPIUrl = baseBotUrl+BotToken;


TelegramBot::TelegramBot()
:HttpGetPostPut(baseBotAPIUrl){
	testv = std::make_shared<bool>();
	*testv = false;
}

void TelegramBot::test(){
	*testv = true;
	std::cout<<"pointer in test: "<<*testv<<"\n";
	
}

void TelegramBot::processMessage(){
	
	std::cout<<"pointer in process: "<<*testv<<"\n";
}

void TelegramBot::setWebHook(){
	std::cout<<post("/setWebhook",
	
	
}