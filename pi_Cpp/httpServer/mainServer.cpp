#include "mainServer.h"
#include <string>

int print_out_key (void *cls, enum MHD_ValueKind kind, 
                   const char *key, const char *value)
{
  printf ("%s: %s\n", key, value);
  return MHD_YES;
}

inline int authorised_connection(struct MHD_Connection* connection){
	int fail;
	char* pass = NULL;
	char* user = MHD_basic_auth_get_username_password(connection, &pass);
	fail = ( (user == NULL) ||
				 (0 != strcmp (user, config::HTTPSERVER_USER)) ||
				 (0 != strcmp (pass, config::HTTPSERVER_PASS)) );  
	if (user != NULL) free (user);
	if (pass != NULL) free (pass);
	return fail;
}

inline void convert_arguments(void* cls, TelegramBot*& bot, HttpState*& httpState, 
	SignalState*& signalState, WebGraph*& webGraph){
	void** arrayOfPointers;
	void* element1;
	void* element2;
	void* element3;
	void* element4;

	//convert arguments back (hope this optimises well),
	//this gives us access to all the classes below with all threads
	//having the same functions and variables availible.
	arrayOfPointers = (void**)cls;
	element1 = (void*)*(arrayOfPointers+0);
	element2 = (void*)*(arrayOfPointers+1);
	element3 = (void*)*(arrayOfPointers+2);
	element4 = (void*)*(arrayOfPointers+3);
	bot = (TelegramBot*)element1;
	httpState = (HttpState*)element2;
	signalState = (SignalState*)element3;
	webGraph = (WebGraph*)element4; 
	return;																		 
}

int answer_to_connection(void* cls,struct MHD_Connection* connection, const char* url,
		                     const char* method, const char* version, const char* upload_data,
		                     size_t* upload_data_size, void** con_cls) {
  int ret;  
  int fail;
  struct MHD_Response *response;	

	TelegramBot* bot;
	HttpState* httpState;
	SignalState* signalState;
	WebGraph* webGraph;

	convert_arguments(cls, bot, httpState, signalState, webGraph);
 
	fail = authorised_connection(connection); //check if other party authorised
  if (0 == strcmp(method, "GET")){
		if (NULL == *con_cls) {*con_cls = connection; return MHD_YES;}
		
		//printf ("New %s request for %s using version %s\n", method, url, version);
		

		
		//if user authentication fails
		if (fail){
				const char* page = "<html><body>Go away.</body></html>";
				response = MHD_create_response_from_buffer(strlen (page), (void*) page, 
									 MHD_RESPMEM_PERSISTENT);
				ret = MHD_queue_basic_auth_fail_response(connection, "my realm",response);
			}
		//continue with correct response if authentication is successfull
		else{
				const char* orderRecieved = "<html><body>Order recieved.</body></html>";
				const char* unknown_page = "<html><body>A secret.</body></html>";
				char* page;
				std::string pageString = "<html><body>A ";
				std::string pageString2 = "secret.</body></html>";
				pageString += pageString2;
				
				//if its a state switch command send it to state for processing
				if(url[1] == '|'){		
	
					httpState->m.lock();//lock to indicate new value in url
					httpState->url = url;
					httpState->updated = true;//is atomic
					signalState->runUpdate();

					response = MHD_create_response_from_buffer(strlen (unknown_page), 
					           (void *) orderRecieved, MHD_RESPMEM_PERSISTENT); 
				}
				
				//else webserver request
				else if(0 == strcmp(url, "/dygraph.css")){
					page = webGraph->dyCss;
					response = MHD_create_response_from_buffer(strlen (page), (void *) page, 
				             MHD_RESPMEM_PERSISTENT);
				}
				else if(0 == strcmp(url, "/dygraph.js")){
					page = webGraph->dyjs;
					response = MHD_create_response_from_buffer(strlen (page), (void *) page, 
									   MHD_RESPMEM_PERSISTENT);
				}
				
				else if(0 == strcmp(url, "/graph2")){
					pageString = webGraph->dy_mainPage();
					response = MHD_create_response_from_buffer(pageString.length(), 
             	       (void *) pageString.c_str(), MHD_RESPMEM_MUST_COPY);
				}
				else if(0 == strcmp(url, "/graph3")){
					pageString = *webGraph->plotly_mainPage();	
					//std::cout<<pageString<<"\n";
					response = MHD_create_response_from_buffer(pageString.length(), 
             	       (void *) pageString.c_str(), MHD_RESPMEM_MUST_COPY);
				}
				else{
					response = MHD_create_response_from_buffer(strlen (unknown_page), 
					           (void *) unknown_page, MHD_RESPMEM_PERSISTENT);
				}
				ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
			}
  }//else possible telegram webhook call
  else if (0 == strcmp(method, "POST")){
		
		bot->processMessage(); //entire bot hangs on this function 
		
	}
	//unrecognised or unallowed protocol break connection
	else{ return MHD_NO;}
  
  MHD_destroy_response (response);
  return ret;
}

//used by load_file to find out the file size
//FIXME was static 
long get_file_size (const char *filename)
{
  FILE *fp;

  fp = fopen (filename, "rb");
  if (fp)
    {
      long size;

      if ((0 != fseek (fp, 0, SEEK_END)) || (-1 == (size = ftell (fp))))
        size = 0;

      fclose (fp);

      return size;
    }
  else
    return 0;
}

//used to load the key files into memory
//FIXME was static and not used wanted to get rid of warning
char* load_file (const char *filename)
{
  FILE *fp;
  char* buffer;
  unsigned long size;

  size = get_file_size(filename);
  if (0 == size)
    return NULL;

  fp = fopen(filename, "rb");
  if (! fp)
    return NULL;

  buffer = (char*)malloc(size + 1);
  if (! buffer)
    {
      fclose (fp);
      return NULL;
    }
  buffer[size] = '\0';

  if (size != fread (buffer, 1, size, fp))
    {
      free (buffer);
      buffer = NULL;
    }

  fclose (fp);
  return buffer;
}


int thread_Https_serv(std::shared_ptr<std::mutex> stop, 
											TelegramBot* bot,
											HttpState* httpState,
											SignalState* signalState,
											PirData* pirData,
											SlowData* slowData){
												
  struct MHD_Daemon* daemon;
  char *key_pem;
  char *cert_pem;
  
  key_pem = load_file("server.key");
  cert_pem = load_file("server.pem");

  //check if key could be read
  if ((key_pem == NULL) || (cert_pem == NULL))
  {
    printf ("The key/certificate files could not be read.\n");
    return 1;
  }

	//create a shared memory space for webpage functions
		std::shared_ptr<WebGraph> webGraph = std::make_shared<WebGraph>(pirData, slowData);


	//make an array of pointers used to pass through to the
	//awnser to connection function (the default handler). This array
	//is read only. The pointers better be in shared memory space for 
	//the awnser function to be able to reach them
	void* arrayOfPointers[4] = {bot, httpState, signalState, webGraph.get()};



  daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_SSL,
														 config::HTTPSERVER_PORT, NULL, NULL,
                             &answer_to_connection, (void*)arrayOfPointers,
                             MHD_OPTION_HTTPS_MEM_KEY, key_pem,
                             MHD_OPTION_HTTPS_MEM_CERT, cert_pem,
                             MHD_OPTION_END);
  
  //check if the server started alright                           
  if(NULL == daemon)
    {
      printf ("%s\n", cert_pem);
      //free memory if the server crashed
      free(key_pem);
      free(cert_pem);

      return 1;
    }
    
	//for as long as we cant lock stop we keep the server
	//up. Stop is the shutdown signal.
	(*stop).lock();	
	std::cout<<"shutting https server down gracefully\n";
	
  //free memory if the server stops
  MHD_stop_daemon(daemon);
  free(key_pem);
  free(cert_pem);	
	(*stop).unlock();	  
  return 0;      
}
