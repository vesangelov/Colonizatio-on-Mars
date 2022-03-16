/*Veselin Angelov
 * EA7
 * Problem 3
 * rstgame.c
 * The purpose of the program is to simulate the colonization of Mars, where workers collect resources and build
 * bases. We have one main bases and the workers can make barracks and train warriors.*/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#define MAXRESOURCES  4096  //The number of total resources.
#define MAXWORKERS  5       //The number of the workers.
#define MAXBARRACKS  20     //Max number of built barracks.
#define MAXUNITS  100     //Max units that the player can collect.

enum worker_state {searching, transporting, unloading, building};   //The 4 state of operations,
// that the workers can do.
enum barrack_state {waiting, training}; //Two state of barracks that can they do.

size_t map_resources = MAXRESOURCES;
size_t base_station_resources = 0;
size_t players_total_resources = 0;
bool start_training = false;
bool start_building = false;
bool game_over = false;
size_t warriors_count = 0 ;  //Represent how many workers the player has in every moment;

pthread_mutex_t map_resources_lock; //mutex to lock and unlock the resources on the map.
pthread_mutex_t base_station_lock;  //mutex to lock and unlock the state of out main base.
pthread_mutex_t barrack_lock;       //mutex to lock and unlock when we train a warrior.
pthread_t input_thread;             //mutex to lock and unlock when we train a warrior.
pthread_cond_t barracks_command_cond; //Condition is true and the training begins.

struct Worker{
    /*This structure contains all needed parameters for every player's worker.
    The parameters of one worker are number, where:*/
    size_t number;      //Every worker has it's own ID
    enum worker_state currentState; /*Every worker has state, that shows what he is doing in current moment
    The worker can do four command: searching, transporing, unloading and building*/
    pthread_t thread;   //Create thread for every one worker
    size_t load;        //The variable contains the amount of resourses that the worker searched
    bool took_last_resource;    /*This variable shows when the current worker takes the last resources on
    the map. */
};

struct Barrack{
    /*This structure represent the parameters in every barrack that we can construct
    The variables are: */
    enum barrack_state currentState;    /*This shows the current state of barrack. It can be "waiting", when the
    barrack waits to do something and "training", when training a warrior. */
    pthread_t thread;   //Create a thread for every on barrack.
};

void barrack_create(struct Barrack* this);
/*Function barrack_create will create the barracks when the player has the needed resources and push the command
for build with the symbol 'b' from the console. */
struct Barrack my_barracks[MAXBARRACKS];    /*In game the player can create 20 barracks, because he has a limit
from the student, because he will not need from more building on this level. */
size_t barracksCount = 0;

void* worker_work(void* arg){
    /*This function represent the action of every worker and shows on the player what everyone do in the current
     * moment. In this function is shown how much time every operation is long and represent the conditions
     * for it's execution.*/
    struct Worker* this = arg;

    while(1){
        switch (this->currentState) {
            case searching:
                //This state is when the worker is searching for resources on the map.
                printf("Worker %zu is searching\n", this->number);
                sleep(4);   //Sleep 4 seconds
                pthread_mutex_lock(&map_resources_lock);    //Lock the resourses for current worker
                //and only he can take resouces.

                if(map_resources == 0){
                    /*If the resources on the map are finished, it's end of the game and
                    /unlock the resources mutex. */
                    printf("No more resuorses!\n");
                    pthread_mutex_unlock(&map_resources_lock);
                    return NULL;
                }
                /*If the resources on the map are less than 6 the worker will take the rest of them
                and collect them in the base resources. */
                this->load = map_resources >= 6 ? 6 : map_resources;
                map_resources -= this->load;

                if(map_resources == 0){
                    //This variable shows the worker take the last resources.
                    this->took_last_resource = true;
                }
                pthread_mutex_unlock(&map_resources_lock);
                this->currentState = transporting;
                break;
            case transporting:
                //This state is for when the worker transport the founded resources to the base station.
                printf("Worker %zu is transporting\n", this->number);
                sleep(3);   //Wait 3 seconds
                this->currentState = unloading;
                break;
            case unloading:
                /*In this case the worker store the resources in tha main base and
                add them in total base resources. */
                printf("Worker %zu is unloaded resources to Base station\n", this->number);
                usleep(500000); //Wait 0.5 seconds
                pthread_mutex_lock(&base_station_lock);
                base_station_resources += this->load;
                players_total_resources += this->load;

                pthread_mutex_lock(&barrack_lock);
                if(game_over){
                    pthread_mutex_unlock(&barrack_lock);
                    pthread_mutex_unlock(&base_station_lock);
                    return NULL;
                }
                pthread_mutex_unlock(&barrack_lock);

                if(start_building){
                    //Chech that it has command to start building barrack.
                    start_building = false;

                    if(base_station_resources >= 80){
                        base_station_resources -= 80;   //substract the resources for barrack
                        this->currentState = building;  //Set the state on building
                        pthread_mutex_unlock(&base_station_lock);
                        break;
                    }
                    else {
                        //Condition when the player hasn't enough resourses to build the barrack.
                        printf("Not enough resourses\n");
                    }
                }

                if(this->took_last_resource){
                    //Check if one of the workers was taken the last resourses on the map.
                    pthread_mutex_lock(&barrack_lock);
                    game_over = true;
                    printf("Last map resource has been unloaded to base station. Press any key to quit.\n");
                    pthread_cond_broadcast(&barracks_command_cond);
                    pthread_mutex_unlock(&barrack_lock);
                    pthread_mutex_unlock(&base_station_lock);
                    return NULL;
                }

                pthread_mutex_unlock(&base_station_lock);
                this->currentState = searching;
                break;
            case building:
                //Start to build barrack from the first free worker.
                printf("Worker %zu is constructing new building Barrack\n", this->number);
                sleep(30);  //Wait 30 seconds to construct barrack.
                pthread_mutex_lock(&barrack_lock);

                if(barracksCount >= MAXBARRACKS){
                    //Check if we are made the max counter of barracks.
                    printf("Max barracks limits reached!\n");
                }
                else {
                    barrack_create(my_barracks+barracksCount);
                    barracksCount++;
                }
                //Message that shows the worker is built the barrack and the number of the worker.
                printf("Worker %zu completed new building Barrack\n", this->number);
                pthread_mutex_unlock(&barrack_lock);
                this->currentState = searching;
                break;
        }
    }
}

void* barrack_work(void* arg){
    //This function represent the work of the barrack and tells when it has a warrior for training.
    struct Barrack* this = arg;

    while(1){
        switch (this->currentState) {
            case waiting:
                //This situation is when barrack wait command to start training.
                pthread_mutex_lock(&barrack_lock);
                struct timespec waiting_time;
                //Struct for waiting time from the console.
                waiting_time.tv_sec = 1;
                waiting_time.tv_nsec = 0;
                while(!start_training && !game_over){
                    pthread_cond_timedwait(&barracks_command_cond, &barrack_lock, &waiting_time);
                    if(game_over){
                        //Checks that if workers deliver all resources on the map and the game is
                        // over in this situation.
                        pthread_mutex_unlock(&barrack_lock);
                        return NULL;
                    }
                }
                //If the game continue and it has command to train, the barrack starts to training warrior.
                start_training = false;
                this->currentState = training;
                pthread_mutex_unlock(&barrack_lock);
                break;
            case training:
                //Case when the warrior start training. Start with check for enough resources for one training.
                pthread_mutex_lock(&base_station_lock);
                if(base_station_resources < 16){
                    printf("Not enough resourses\n");
                    pthread_mutex_unlock(&base_station_lock);
                    this->currentState = waiting;
                    break;
                }
                else {
                    //Check that the player's army reached the max units.
                    pthread_mutex_lock(&barrack_lock);
                    if(warriors_count >= MAXUNITS){
                        printf("You've reached the max limit of army\n");
                    }
                    else{
                        base_station_resources -= 16;
                        printf("Warrior is being trained.\n");
                        warriors_count++;
                        pthread_mutex_unlock(&barrack_lock);
                        sleep(4);   //Wait 4 seconds for training the warrior.
                        pthread_mutex_lock(&barrack_lock);
                        printf("Warrior is ready for duty\n");

                        if(warriors_count == 20){
                            //Che if the purpose of the program is complete.
                            printf("Congratulations. Mission complete! Press any key to quit.\n");
                            game_over = true;
                            pthread_cond_broadcast(&barracks_command_cond);
                            pthread_mutex_unlock(&barrack_lock);
                            pthread_mutex_unlock(&base_station_lock);
                            return NULL;
                        }
                    }
                    pthread_mutex_unlock(&barrack_lock);
                }
                pthread_mutex_unlock(&base_station_lock);
                this->currentState = waiting;
                break;
        }
    }

    return NULL;
}

void workerCreate(struct Worker* this, size_t number){
    /*This function is like a constructor for the struct from Workers and here it can put parameters for the
    different condition like worker number, current state and etc.*/
    this->number = number;
    this->currentState = searching;
    pthread_create(&this->thread, NULL, worker_work, this);
    this->load = 0;
    this->took_last_resource = false;
}

void barrack_create(struct Barrack* this){
    /*This function is like a constructor for the struct from Barracks and here it can put parameters for the
    different condition like how much warriors we have. */
    this->currentState = waiting;
    pthread_create(&this->thread, NULL, barrack_work, this);
}

void* input_work(void* arg){

    while(1){
        //Reading command from the player for current operation(building and training).
        char command = getchar();

        pthread_mutex_lock(&barrack_lock);
        if(game_over){
            pthread_mutex_unlock(&barrack_lock);
            goto quit;
        }
        pthread_mutex_unlock(&barrack_lock);

        switch (command) {
            case 'b':
                //Command for build barrack.
                pthread_mutex_lock(&base_station_lock);
                start_building = true;
                pthread_mutex_unlock(&base_station_lock);
                break;
            case 'w':
                //Command for trainin warrior.
                pthread_mutex_lock(&barrack_lock);

                if(barracksCount == 0){
                    printf("Operation not suppoerted\n");
                }
                else {
                    start_training = true;
                }

                pthread_cond_signal(&barracks_command_cond);
                pthread_mutex_unlock(&barrack_lock);
                break;
            case 'q':
                //Exit from the program.
                goto quit;
            case '\n':
                break;
            default:
                //Unknown command from the player.
                printf("Unknown command!\n");
                break;
        }
    }

    quit:
    return 0;
}

int main() {

    //Start time in the beginning of the program.
    time_t begin, end;
    time(&begin);

    //Initialization of two mutex.
    pthread_mutex_init(&map_resources_lock, NULL);
    pthread_mutex_init(&base_station_lock, NULL);
    //Initialisation the condition variable.
    pthread_cond_init(&barracks_command_cond, NULL);
    pthread_create(&input_thread, NULL, input_work, NULL);

    struct Worker workers[MAXWORKERS];

    for (int i = 0; i < MAXWORKERS; ++i) {
        //Create five workers.
        workerCreate(workers + i, i);
    }

    for (int i = 0; i < MAXWORKERS; ++i) {
        //Waiting to the threads complete.
        pthread_join(workers[i].thread, NULL);
    }

    for (int i = 0; i < barracksCount; ++i) {
        //Waiting to the threads complete.
        pthread_join(my_barracks[i].thread, NULL);
    }

    //Waiting to the input threads complete.
    pthread_join(input_thread, NULL);

    //Stop timing.
    time(&end);
    float elapsed = end - begin;

    size_t left_resources = MAXRESOURCES - players_total_resources;

    /*Print the resources on the map in the beginning, how much are left, how much the player is kept,
    the time of the program execusion from the start to the end of the game, how much buildings player's has in the end,
    how much resource player has left and how much units player has in the end of the game. */
    printf("Map resources in the begining : %d\n", MAXRESOURCES);
    printf("Map resources left : %zu\n", left_resources);
    printf("Player's total resources from the beginning: %zu\n", players_total_resources);
    printf("The program execution time is : %.2f seconds\n", elapsed);
    printf("Player's buildings are : %zu\n", barracksCount + 1);
    printf("Player's resources in the end of the game are: %zu\n", base_station_resources);
    printf("Player's units are : %zu\n", warriors_count + MAXWORKERS);

    pthread_mutex_destroy(&map_resources_lock);
    pthread_mutex_destroy(&base_station_lock);
    pthread_mutex_destroy(&barrack_lock);

    return 0;
}