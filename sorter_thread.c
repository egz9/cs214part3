//SYSTEMS PROGRAMMING PROJECT 2

#include "sorter_thread.h"


//Thread_Node * head_thread;
pthread_mutex_t thread_count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t csv_node_lock = PTHREAD_MUTEX_INITIALIZER;
char * category;
int totalThreadCount;

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
			sortcsv_args->path =		 malloc(sizeof(char)*(strlen(dirpath) + 1));
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
			
			//check first line*******************************************************************
			char * csvpath = (char*)malloc( strlen( sortcsv_args->path) + strlen(sortcsv_args->entry_name) + 3); 
			if (csvpath == NULL){
				fprintf(stderr, "csvpath malloc failed\n");
				exit(0);
			}
			strcpy(csvpath, sortcsv_args->path);
			strcat(csvpath, "/");
			strcat(csvpath, sortcsv_args->entry_name);
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
			}
			
			//printf("spawing sort thread for [%s]\n", entry->d_name);
			pthread_create(&tid, NULL, (void*)sortcsv, (void*)sortcsv_args);
			headThread = addtid(tid, headThread);
		}
		//if entry is neither directory or a csv we skip it
		else{
			//printf("continuing\n");
			continue;
		}
	}
	
	//joins
	Thread_Node * temp = headThread;
	int threadCount = 0;
	temp = headThread;
	while(temp){
		threadCount++;
		printf("%d\n", temp->tid);
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
 
/* is_csv takes a string s1 and determines
 * if the last 4 characters are ".csv" indicating
 * whether or not the string represents a csv file
 * or not. It returns 1 if the string has the ".csv"
 * extension and 0 otherwise */
int is_csv(char * s1){
	char * ptr = strrchr(s1, '.');
	int i;
    if(!ptr){
		return 0;
	}
	else {
		ptr++;
		if ( (*ptr == 'c' || *ptr == 'C') &&
		   (*(ptr+1) == 's' ||*(ptr+1) == 'S' ) && 
		   (*(ptr+2) == 'v' || *(ptr+2) == 'V' )&& 
		   (*(ptr+3) == '\0') ){
			//printf("Success\n");
			return 1;
		}
		return 0;
	}
}

int main(int argc, char ** argv) {
    pthread_t tid;		//will hold tid temporaily of spawned threads
    int i = 0;			//iterator
	int areThreads = 1;	//used in while loop 
	totalThreadCount = 0;
	char typeFlag;
	char * input_dir_name = malloc(2*sizeof(char));//input_dir_name holds the name of the input directory
	char * output_dir_name = malloc(2*sizeof(char));;//output_dir_name holds the name of the output directory
	char * csvHeader = "color,director_name,num_critic_for_reviews,duration,director_facebook_likes,actor_3_facebook_likes,actor_2_name,actor_1_facebook_likes,gross,genres,actor_1_name,movie_title,num_voted_users,cast_total_facebook_likes,actor_3_name,facenumber_in_poster,plot_keywords,movie_imdb_link,num_user_for_reviews,language,country,content_rating,budget,title_year,actor_2_facebook_likes,imdb_score,aspect_ratio,movie_facebook_likes";
	char * outFileName;
	FILE * outFilePtr;
	DIR * dir;
	
	
	head = NULL;
	
	if (
		( argc == 3 && strcmp(argv[1], "-c") != 0 )
			|| ( argc != 3 &&
				argc != 5 &&
					argc != 7  ) ){
		printf("incorrect input, please enter paramaters as follows\n");
		printf("./sorter -c <catagory> -d <input directory> -o <output directory>\n");
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
		//argv[1] || argv[3] || argv[5}  are something other than -c , -d , or -o	
		printf("incorrect input, please enter paramaters as follows\n");
		printf("./sorter -c <catagory> -d <input directory> -o <output directory>\n");
		printf("NOTE: -d <input directory> & -o <output directory> are optional and can be given in either order\n");
		return -1;
	}//end for-loop (initialized inputdir, outputdir, and category)
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
	
    Thread_Args * enter_dir_args = malloc(sizeof(Thread_Args) );
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
    
    outFileName = malloc ( (64 + strlen(output_dir_name))*sizeof(char));
    strcpy(outFileName, output_dir_name);
    strcat(outFileName, "/Allfiles-sorted-");
    strcat(outFileName, category);
    strcat(outFileName, ".csv");
    
    //printf("enter->path[%s]\n",enter_dir_args->path);
	//printf("entry_name[%s]\n",enter_dir_args->entry_name);
    //printf("outFileName[%s]\n", outFileName);
         
    printf("initial PID: %d\n", getpid());
    printf("TIDs of all child threads:\n"); 
    enter_directory(enter_dir_args);
    
	printf("Total number of threads: %d\n", totalThreadCount);
    
    if(head == NULL){
		return;
	}
    
    head = forceMerge(head, typeFlag);
    outFilePtr = fopen(outFileName, "w");
	fprintf(outFilePtr, "%s\n", csvHeader);
	for (i=0;i<head->length;i++){
		//fprintf(outFilePtr, "\n[%s]\n", head->data[i]->key);
		fprintf(outFilePtr, "%s", head->data[i]->wholerow);
	}
}


