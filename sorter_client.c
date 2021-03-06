//SYSTEMS PROGRAMMING PROJECT 3

#include "sorter_client.h"

//Thread_Node * head_thread;
pthread_mutex_t thread_count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t slock = 			PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t csv_node_lock = 	PTHREAD_MUTEX_INITIALIZER;
char * category = NULL;
char * sort_request;
int totalThreadCount;
struct addrinfo *result, *rp;
struct addrinfo hints;


// given a tid, addtid adds a new thread_node containing said tid to
// the thread node linked list
Thread_Node * addtid(pthread_t tid, Thread_Node * headThread){
	//printf("in addtid\n");
	Thread_Node * newNode = (Thread_Node *)malloc(sizeof (Thread_Node) );
	if (newNode == NULL){
		fprintf(stderr, "newNode malloc failed\n");
		exit(0);
	}
	newNode->tid = tid;
	//printf("Adding tid %d\n", tid);
	newNode->next = headThread;
	headThread = newNode;
	return headThread;
}

char * encode_wholerow(char * wholerow){
	//***may want to trim wholerow later
	//char * str_size = malloc(6 * sizeof(char));			//size of wholerow
	//char * str_size_size = malloc(2 * sizeof(char));	//size of (size of wholerow)
	char str_size[6];
	char str_size_size[2];
	char wholerowCpy[ strlen(wholerow) + 1 ];
	char * wholerow_encoding = NULL;
	char * cPtr;	
	strcpy(wholerowCpy, wholerow);
	cPtr = wholerowCpy;				
	if (*(cPtr + (strlen(wholerowCpy)-1)) == '\n' )
		*(cPtr + (strlen(wholerowCpy)-1)) = '\0';
	
	
	//this section prints out each array position & its corresponding char
	/*
	int i = 0;
	for (i = 0; i <= strlen(wholerowCpy); i++){
		if (i % 8 == 0){
			printf("\n[%4d] = %c ", i, wholerowCpy[i]);
			continue;
		}
		printf("[%4d] = %c ", i, wholerowCpy[i]);
	}
	printf("\n");
	fflush(stdout);
	*/
	sprintf(str_size, "%d", (int)strlen(wholerowCpy) );
	sprintf(str_size_size, "%d", (int)strlen(str_size) );
	
	//wholerow encoding will be formated as follows
	//<sizeof(sizeof wholerow)><sizeof wholerow><wholerow>
	//here we malloc an extra 2 bytes to account for \0 and ?
	wholerow_encoding = 
		malloc( strlen(str_size) + strlen(str_size_size) + strlen(wholerowCpy) + 2);
	sprintf(wholerow_encoding, "%s?%s%s", str_size_size, str_size, wholerowCpy);
	
	
	
	return wholerow_encoding;
}

void * sendcsv(Thread_Args * args){
	char * csvpath = (char*)malloc( strlen( args->path) + strlen(args->entry_name) + 3); 
	if (csvpath == NULL){
		fprintf(stderr, "csvpath malloc failed\n");
		exit(0);
	}
	sprintf(csvpath, "%s/%s", args->path, args->entry_name);
	int sockfd = args->socketfd;
	free(args->path);
	free(args->entry_name);
	free(args);
	
	int csvfd = open(csvpath, O_RDONLY);
	FILE * fp;
	char line[1500];
	char confirmation[500]; //meseage will be "Complete_Sort"
	memset(confirmation, 0, sizeof(confirmation) );
	char * encLine;
	ssize_t writtenBytes = 0;
	
	pthread_mutex_lock(&slock);
	
	for (rp = result; rp != NULL; rp = rp->ai_next){
		sockfd = socket(rp->ai_family, rp->ai_socktype,
							   rp->ai_protocol);
		if (sockfd == -1)
			continue;
		
		if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;		//success
		
		close(sockfd);
		
	}
	if (rp == NULL){
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	
	
	
	
	fp = fopen(csvpath, "r");
	//printf("sending sr: [%s]\n", sort_request);
	write( sockfd, sort_request, strlen(sort_request) );
	fgets(line, 1500*sizeof(char), fp); //skip first line 
    while( fgets(line, 1500*sizeof(char), fp) !=NULL ){
		encLine = encode_wholerow(line);
		writtenBytes = write(sockfd, encLine, strlen(encLine) );
		//printf("sent %d bytes \n%s\n\n", writtenBytes, encLine);
		free(encLine);
		//printf("\n");
	} 
	write(sockfd, "0?", 2);
    
    close(csvfd);
	//wait for confirmation
	while( strstr(confirmation, "Complete_Sort") == NULL ){
		read(sockfd, confirmation, sizeof(confirmation) );
		//printf("confirmation string: [%s]\n", confirmation);
		sleep(2);
	}
	
	pthread_mutex_unlock(&slock);
	return NULL;
}
 
/* enter directory is a function intended for multithreading situations.
 * you must pass it a Thread_Args struct which will contain the name of 
 * the directory you wish to traverse in the entry_name member
 * and the path to the directory you are currently in in the path member
 * of the struct. This function will create threads when it finds a csv
 * file to sort and it will also create threads to traverse 
 * subdirectories*/
void * enter_directory(Thread_Args * args){
	//dirpath will hold {(parent directory path)/(current directory)}
	//printf("in enter_direcory. args->path: [%s], args->entry_name: [%s]\n", args->path, args->entry_name);
	int sockfd = args->socketfd;
	DIR * current_dir;
	struct dirent *entry;
	Thread_Node * headThread;
	pthread_mutex_t listLock = PTHREAD_MUTEX_INITIALIZER;
	
	//make space for dirpath. +3 to account for 2 "\0"s & 1 "/"
	char * dirpath = (char*)malloc( strlen( args->path) + strlen(args->entry_name) + 3); 
	if (dirpath == NULL){
		fprintf(stderr, "dirpath malloc failed\n");
		exit(0);
	}
	//headThread = (Thread_Node*)malloc(sizeof(Thread_Node)); //head of thread list 
    headThread = NULL;
    //the first time this is called, the full directory name will be in
    //path only and entryname will be // cuz its imposible for a file
    //to actually be called //
    if(strcmp(args->entry_name, "//")==0 ){
		strcpy(dirpath, args->path); 
	}
    else{
		strcpy(dirpath, args->path);
		strcat(dirpath, "/");
		strcat(dirpath, args->entry_name);
	}
	//no longer need given args, so we can free it since it was dynamically allocated
	free(args->path);
	free(args->entry_name);
	free(args);
	
	current_dir = opendir(dirpath);
	
	if (current_dir == NULL) {
		printf("directory could not be opened\n");
		return;
	}
	//if (current_dir) printf("entering %s\n", dirpath);
	while ( (entry = readdir(current_dir)) != NULL) {
		if (strcmp((char*)entry->d_name, ".") == 0 || strcmp((char*)entry->d_name, "..") == 0){
			continue;
		}
		//if entry is a directory we create a thread to enter that directory
		if (entry->d_type == DT_DIR) {
			pthread_t tid;
			Thread_Args * enter_dir_args = 	malloc(sizeof(Thread_Args) );
			if(enter_dir_args == NULL){
				fprintf(stderr, "enter_dir_args malloc failed\n");
				exit(0);
			}
			enter_dir_args->path =			malloc(sizeof(char) * (strlen(dirpath) + 1) );
			if(enter_dir_args->path == NULL){
				fprintf(stderr, "enter_dir_args->path malloc failed\n");
				exit(0);
			}
			enter_dir_args->entry_name =	malloc(sizeof(char) * (strlen((char*)entry->d_name) + 1) );
			if(enter_dir_args == NULL){
				fprintf(stderr, "enter_dir_args->entry_name malloc failed\n");
				exit(0);
			}
			//printf("load enter_dir_args with path[%s] & name[%s]\n", dirpath, (char*)entry->d_name);
			strcpy(enter_dir_args->path, dirpath);
			strcpy(enter_dir_args->entry_name, (char*)entry->d_name);
			enter_dir_args->socketfd = sockfd;
			//printf("creating new thread for [%s]\n", (char*)entry->d_name);
			pthread_create(&tid, NULL, (void*)enter_directory, (void*)enter_dir_args);
			//printf("Adding tid %d\n", tid);
			
			headThread = addtid(tid, headThread);
		}
		//if entry is csv we will create a thread to sort it. this thread will add its sorted list to the
		//master list
		else if (is_csv((char*)entry->d_name) && strstr((char*)entry->d_name, "-sorted-") == NULL ) {
			//printf("%s is a csv\n", (char*)entry->d_name);
			pthread_t tid;
			Thread_Args * sortcsv_args = malloc(sizeof(Thread_Args));
			if(sortcsv_args == NULL){
				fprintf(stderr, "sortcsv_args malloc failed\n");
				exit(0);
			}
			sortcsv_args->path = malloc(sizeof(char)*(strlen(dirpath) + 1));
			if(sortcsv_args == NULL){
				fprintf(stderr, "sortcsv_args->path malloc failed\n");
				exit(0);
			}
			sortcsv_args->entry_name =   malloc(sizeof(char)*(strlen((char*)entry->d_name)+1));
			if(sortcsv_args->entry_name == NULL){
				fprintf(stderr, "sortcsv_args->entry_name malloc failed\n");
				exit(0);
			}
			strcpy(sortcsv_args->path, dirpath);
			strcpy(sortcsv_args->entry_name, (char*)entry->d_name);
			sortcsv_args->socketfd = sockfd;
			
			//check first line*******************************************************************
			char * csvpath = (char*)malloc( strlen( dirpath) + strlen(entry->d_name) + 3); 
			if (csvpath == NULL){
				fprintf(stderr, "csvpath malloc failed\n");
				exit(0);
			}
			sprintf(csvpath, "%s/%s", dirpath, entry->d_name);
			FILE * inputFile = fopen(csvpath, "r");
			char *token;						// used to store tokens
			char *tokenizedLine[32];
			char * linesPtr;
			char lines[1500];
			fgets(lines, 1500*sizeof(char), inputFile);
			linesPtr = &lines[0];
			rewind(inputFile); 
			fclose(inputFile);
			
			int catCount = 0;
			int categoryExists = 0;
      		while(token=strsep(&linesPtr,",")){
        		tokenizedLine[catCount] = (char*)malloc(sizeof(char)*100);
        		if(tokenizedLine[catCount] == NULL){
					fprintf(stderr, "tokenizedLine[%d] malloc failed\n", catCount);
					exit(0);
				}
				strcpy(tokenizedLine[catCount], token);
				trim(tokenizedLine[catCount]);
				if(strcmp(tokenizedLine[catCount], category) == 0 ){
					categoryExists = 1;
				}
				catCount++;
			}
			if (categoryExists == 0){
				//fprintf(stderr, "thread [%d] category [%s] was not found in %s\n", pthread_self(), category, csvpath);
				continue;
			}//first line check over.**********************************************************
			
			//-----------------------prepare data in file for transport-----------------------
			pthread_create(&tid, NULL, (void*)sendcsv, (void*)sortcsv_args);
			headThread = addtid(tid, headThread);
			
			
			//printf("spawing sort thread for [%s]\n", entry->d_name);
			//pthread_create(&tid, NULL, (void*)sortcsv, (void*)sortcsv_args);
			//headThread = addtid(tid, headThread);
		}
		//if entry is neither directory or a csv we skip it
		else{
			//printf("continuing\n");
			continue;
		}
	}
	
	//joins---------------------------------------------------------------
	Thread_Node * temp = headThread;
	int threadCount = 0;
	temp = headThread;
	while(temp){
		threadCount++;
		//printf("%d\n", temp->tid);
		pthread_join(temp->tid, NULL);
		//add in delete later
		temp = temp->next;
	}
	pthread_mutex_lock(&thread_count_lock);
	totalThreadCount = totalThreadCount + threadCount;
	pthread_mutex_unlock(&thread_count_lock);
	//printf("returning from directory %s\n", dirpath);
	closedir(current_dir);
	
	
	return NULL; 
}

//char* category = NULL;
int main(int argc, char ** argv) {

	char * input_dir_name = malloc(2*sizeof(char));//input_dir_name holds the name of the input directory
	char * output_dir_name = malloc(2*sizeof(char));//output_dir_name holds the name of the output directory
	char * outFileName;
	char * host_name = NULL;
	char * port_number = NULL;
	char * megaString;	
	char typeFlag;
	int port_num;
	int client_socket;
	int i,j,k;
	struct sockaddr_in server_address;
	Thread_Args * enter_dir_args;
	DIR * dir;
	FILE * outFilePtr;

	// traverse commandline to initialize variables
	if (
			( argc == 7 && strcmp(argv[1], "-c") != 0 )
			|| ( argc != 7 &&
				argc != 9 &&
				argc != 11  ) ){
		printf("incorrect input, please enter paramaters as follows\n");
		printf("%s -c <catagory> -h <host name> -p <port number> -d <input directory> -o <output directory>\n", argv[0]);
		printf("NOTE: -d <input directory> & -o <output directory> are optional and can be given in either order\n");
		return -1;
	}
	//initialize both input and output dir to current directory
	strcpy(input_dir_name,".");
	strcpy(output_dir_name,".");
	//traverse commandline to reinitialize parameters
	for( i = 1; i < argc; i += 2 ){
		if(strcmp(argv[i],"-c") == 0){
			//if duplicate -c <category> input given, print error
			//otherwise initialize
			if(category != NULL){
				printf("incorrect input, please input only one category to sort by.\n");
				return -1;
			}
			category = malloc( (strlen(argv[i+1])+1)*sizeof(char) );	
			strcpy(category, argv[i+1]);
			continue;
		}	
		if(strcmp(argv[i],"-d") == 0){
			//if duplicate input directory print error
			//otherwise initialize 	
			if(strcmp(input_dir_name,".") != 0){
				printf("incorrect input, please input only one starting directory.\n");
				return -1;
			}
			input_dir_name = realloc(input_dir_name, (strlen(argv[i+1])+1)*sizeof(char) );
			strcpy(input_dir_name, argv[i+1]);
			continue;

		}	
		//if duplicate output directory	print error
		//otherwise initialize
		if(strcmp(argv[i],"-o") == 0){
			if(strcmp(output_dir_name,".") != 0){
				printf("incorrect input, please input only one output directory.\n");
				return -1;
			}
			output_dir_name = realloc(output_dir_name, (strlen(argv[i+1])+1)*sizeof(char) );
			strcpy(output_dir_name, argv[i+1]);
			continue;

		}
		if(strcmp(argv[i],"-h") == 0){
			//if duplicate -h <host_name> input given, print error
			//otherwise initialize
			if(host_name != NULL){
				printf("incorrect input, please input only one host name.\n");
				return -1;
			}
			host_name = malloc( (strlen(argv[i+1])+1)*sizeof(char) );	
			strcpy(host_name, argv[i+1]);
			continue;
		}	
		if(strcmp(argv[i],"-p") == 0){
			//if duplicate -p <port_number> input given, print error
			//otherwise initialize
			if(port_number != NULL){
				printf("incorrect input, please input only one port number.\n");
				return -1;
			}
			port_number = malloc( (strlen(argv[i+1])+1)*sizeof(char) );	
			strcpy(port_number, argv[i+1]);
			continue;
		}	
	}//end for-loop (initialized inputdir, outputdir, category, hostname and port number)
	
	//Check validity of arguments---------------------------------------------------------
	if( category == NULL ){
		printf("incorrect input, -c <category> parameters required.\n");
		return -1;
	}//end of cmd line arg check
	
	//check if category exists
	typeFlag = getTypeFlag(category);
	if (typeFlag == '0'){
		fprintf(stderr, "ERROR: category [%s] does not exist\n", category);
        return -1;
	}
	//check if output_dir_name refers to a real directory
	if ( !(dir = opendir(output_dir_name)) ){
		fprintf(stderr, "ERROR: directory [%s] not found\n", output_dir_name);
        return -1;
	}
	closedir(dir);
	//check if input_dir_name refers to a real directory
    if ( !(dir = opendir(input_dir_name)) ){
		fprintf(stderr, "ERROR: directory [%s] not found\n", input_dir_name);
        return -1;
	}
	
	closedir(dir);
	//end of validity check-----------------------------------------------------------------
	
	//prepare arguments for enter_directory function----------------------------------------
    enter_dir_args = malloc(sizeof(Thread_Args) );
    if(enter_dir_args == NULL){
				fprintf(stderr, "enter_dir_args (in main) malloc failed\n");
				exit(0);
	}
    
    enter_dir_args->path = malloc( (sizeof(char)*strlen(input_dir_name)) + 1 );
    if(enter_dir_args->path == NULL){
				fprintf(stderr, "enter_dir_args->path (in main) malloc failed\n");
				exit(0);
	}
    enter_dir_args->entry_name = malloc(sizeof(char) * 3 );
    if(enter_dir_args->entry_name == NULL){
				fprintf(stderr, "enter_dir_args->entry_name (in main) malloc failed\n");
				exit(0);
	}
    strcpy(enter_dir_args->path, input_dir_name);
    strcpy(enter_dir_args->entry_name, "//");
    free(input_dir_name);
    //finished preparing arguments----------------------------------------------------------
    
    //set sort_request. Each thread will send this to server
    //+5 to account for header (x?xx) and null byte
    sort_request = malloc( strlen("Sort_Request,") + strlen(category) + 5);
    sprintf(sort_request, "Sort_Request,%s", category);
    sort_request = encode_wholerow(sort_request);
    
    outFileName = malloc ( (64 + strlen(output_dir_name))*sizeof(char));
    strcpy(outFileName, output_dir_name);
    strcat(outFileName, "/Allfiles-sorted-");
    strcat(outFileName, category);
    strcat(outFileName, ".csv");
    
    //printf("enter->path[%s]\n",enter_dir_args->path);
	//printf("entry_name[%s]\n",enter_dir_args->entry_name);
    //printf("outFileName[%s]\n", outFileName);
    //head = forceMerge(head, typeFlag);
	/*
	for (i=0;i<head->length;i++){
		//fprintf(outFilePtr, "\n[%s]\n", head->data[i]->key);
		fprintf(outFilePtr, "%s", head->data[i]->wholerow);
	}
	*/
	port_num = atoi(port_number);
	//free(port_number);
	
	//printf("port_num: %d\n", port_num);
	//create socket
	
	
	
	
	// initalize  server_address
/*
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port_num);
	server_address.sin_addr.s_addr = inet_addr(host_name); 		
*/

	
	
	int s;
	
	memset(&hints, 0, sizeof(struct addrinfo));
	 hints.ai_family 	= AF_INET;
	 hints.ai_socktype 	= SOCK_STREAM;
	 hints.ai_protocol 	= 0;
	 hints.ai_flags 	= 0;
	 
	
	s = getaddrinfo(host_name, port_number, &hints, &result);
	if ( s != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	
	//get addrinfo returns a list of address structures
	//so here we go through list and stop once we establish
	//a connection

	//freeaddrinfo(result); //no longer needed
/*
	// connect
	int connection = connect(client_socket,(struct sockaddr*)&server_address,sizeof(server_address));
	if (connection == -1){
		printf("failed to connect to sever :(\n");
		return -1;
	}
*/
	// send category over. DONT USE THIS
	//send(client_socket, category, sizeof(category), 0);
	
	enter_dir_args->socketfd = client_socket;
	enter_directory(enter_dir_args); 
	
	//get addrinfo returns a list of address structures
	//so here we go through list and stop once we establish
	//a connection
	for (rp = result; rp != NULL; rp = rp->ai_next){
		client_socket = socket(rp->ai_family, rp->ai_socktype,
							   rp->ai_protocol);
		if (client_socket == -1)
			continue;
		
		if (connect(client_socket, rp->ai_addr, rp->ai_addrlen) != -1)
			break;		//success
		
		close(client_socket);
		
	}
	if (rp == NULL){
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	
	//	send dump request,
	//	server will send filesize of dump
	//	then they will send the dump
	//	dump is loaded into megaString
	//	megastring is printed into outFile
	// 	done
	outFilePtr = fopen(outFileName, "w");
	fprintf(outFilePtr, "color,director_name,num_critic_for_reviews,duration,director_facebook_likes,actor_3_facebook_likes,actor_2_name,actor_1_facebook_likes,gross,genres,actor_1_name,movie_title,num_voted_users,cast_total_facebook_likes,actor_3_name,facenumber_in_poster,plot_keywords,movie_imdb_link,num_user_for_reviews,language,country,content_rating,budget,title_year,actor_2_facebook_likes,imdb_score,aspect_ratio,movie_facebook_likes\n");
	
	int bytesSent = write(client_socket, "2?12Dump_Request", 16);
	//printf("sent %d for dump\n", bytesSent);
	char* current_Data = NULL;
	int numRead = 1;
	char buf[500];
	int sizeHeader;
	int wholeRowSize;
	int offset = 0;
	while (numRead>0){
		numRead = read(client_socket, buf+offset, 500-offset) + offset;
		//printf("numread = %d\n",numRead);
		//printf("buf first 10: [%.20s]\n", buf);
		if(numRead == 0 && offset == 0){
			break;
		}
		offset = 0;
		int i = 0;
		char header[2];
		while (i<2){	
			header[i] = buf[i];
			//printf("header[%d] = %c\n", i, header[i]);
			if(buf[i] == '?'){
				header[i]= '\0';
				i++;
				break;
			}
			i++;
		}
		
		
		//printf("header %s\n", header);
		sizeHeader = 0;
		sizeHeader = atoi(header);
		if(sizeHeader == 0){
			printf("End of file\n");
			break;
		}
		int j = 0;
		char numData[sizeHeader +1];
		while (j<sizeHeader){
			//printf("j<sizeHeader\n");
			numData[j]= buf[i];
			j++;
			i++;
		}
		
		numData[j]= '\0';
		//printf("numData = %s\n", numData);
		wholeRowSize = 0;
		wholeRowSize = atoi(numData);
		current_Data = realloc(current_Data, wholeRowSize*sizeof(char)+1);
		if (current_Data == NULL){
			fprintf(stderr, "current_Data realloc failed\n");
		}
		memset(current_Data, 0, sizeof(current_Data));
		
		
		if(wholeRowSize>=numRead-i){
			//printf("wholeRowSize>=numRead-i\n");
			memcpy(current_Data, buf+i, numRead-i);
			while(wholeRowSize>numRead-i){
				numRead += read(client_socket, current_Data+(numRead-i), wholeRowSize-numRead);
				printf("while wholerowsize > numread-i\nnumread = %d\ncurr_data %s\n",
				numRead, current_Data);
			}
			current_Data[wholeRowSize] = '\0';
			//printf("writing [%s] to file\n", current_Data);
			fprintf(outFilePtr,"%s\n",current_Data);
			offset = 0;
		}
		
		if (numRead-i>wholeRowSize){
			//printf("numRead-i>wholeRowSize\n");
			memcpy(current_Data,buf+i,wholeRowSize); 
			current_Data[wholeRowSize] = '\0';
			//printf("writing [%s] to file\n", current_Data);
			fprintf(outFilePtr,"%s\n",current_Data);
			char temp[500];
			memcpy(temp, buf+i+wholeRowSize,500-(wholeRowSize+i));
			//printf("temp: [%s]\n", temp);
			fflush(stdout);
			memset(buf, 0, sizeof(current_Data));
			memcpy(buf,temp,500);
			offset = 500-(i+wholeRowSize); //*** size of good chunck
		}
	}
	
	
	
	fclose(outFilePtr);
	close(client_socket);
	free(host_name);
	free(input_dir_name);
	free(output_dir_name);
	free(category);
	return 0;
}
