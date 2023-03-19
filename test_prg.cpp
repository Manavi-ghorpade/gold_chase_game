//version 1.1
#include "goldchase.h"
#include "Map.h"
#include "Screen.h"
#include <semaphore.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <memory>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <mqueue.h>
#include <sstream>
using namespace std;
struct Gold_Mine         //Defining a structure 
{
  unsigned short rows;
  unsigned short cols;
  pid_t pid[5];
  unsigned char map[0];
};

string mq_name[5] = {"/pcq0","/pcq1","/pcq2","/pcq3","/pcq4"};
unsigned char player_n[5]={G_PLR0,G_PLR1,G_PLR2,G_PLR3,G_PLR4};
mqd_t readqueue_fd;
bool win = false;
Gold_Mine* gmp;
shared_ptr<Map> pointer_to_rendering_map;            //shared pointer(refered from processnotes.cpp
int player_number = 0;
int current_byte = 0; 
unsigned char current_player;
sem_t *mysemaphore;  

void map_handling(int sigNum)
{
  if(pointer_to_rendering_map)
    pointer_to_rendering_map->drawMap();
}

void read_queue(int sigNum)
{
  if(pointer_to_rendering_map == NULL)
    return;
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);
  int e;
  char msg1[121];
  memset(msg1, 0, 121);
  while((e=mq_receive(readqueue_fd, msg1, 120, NULL))!=-1)
  {
    string message(msg1);
    pointer_to_rendering_map->postNotice(message.c_str());
    memset(msg1, 0, 121);
  }

  if(errno!=EAGAIN)
  {
    perror("mq_receive error");
    exit(1);
  }
}

void write_queue(unsigned char player, int player_num2)
{
  int i;
  string a, str, msg;
  unsigned int players, player_num;
  int m=0;
  for(int i=0;i<5;i++)
  {
   if(gmp->pid[i]!=0)
   {
     m|=player_n[i];
   }
  } 
  m &= ~player_n[player_num2-1];
  player_num = pointer_to_rendering_map->getPlayer(m);
  if(player_num ==0)
    return;
  a= pointer_to_rendering_map->getMessage();
  str = to_string(player_num2);
  msg = "Player " + str + " says: ";
  msg.append(a);

  switch(player_num)
  {
    case G_PLR0: i = 0;
           break;
    case G_PLR1: i = 1;
           break;
    case G_PLR2: i = 2;
           break;
    case G_PLR3: i = 3;
           break;
    case G_PLR4: i = 4;
           break;
  }

  mqd_t writequeue_fd;
  if((writequeue_fd =  mq_open(mq_name[i].c_str(), O_WRONLY|O_NONBLOCK)) == -1)
  {
    perror("mq open error at write mq");
    exit(1);
  }
  char message_text[121];
  memset(message_text , 0 ,121);
  strncpy(message_text, msg.c_str(), 120);

  if(mq_send(writequeue_fd, message_text, strlen(message_text), 0) == -1)
  {
    perror("mq_send error");
    exit(1);
  }
  mq_close(writequeue_fd);
}

void broadcasttoMqueue(int player_num2)
{
  int c=0;
  for(int i=0;i<5;i++)
  {
    if(gmp->pid[i]!=0)
    {
      c++;
    }
  }

  string temp,ss,message;
  if(c==1)
  {
    pointer_to_rendering_map->postNotice("No players");
    return;
  }
  if(win == false)
  {
    temp = pointer_to_rendering_map->getMessage();
    ss = to_string(player_num2);
    message = "Player " + ss + " says: ";
    message.append(temp);
  }
  else if(win == true)
  {
    ss = to_string(player_num2);
    message = "Player " + ss + " has already won ";
  }
  mqd_t writequeue_fd;

  char message_text[121];

  for(int i = 0; i < 5; i++)
  {
    if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
    {
      if((writequeue_fd=mq_open(mq_name[i].c_str(), O_WRONLY|O_NONBLOCK))==-1)
      {
        perror("mq_open........2");
        exit(1);
      }
      memset(message_text, 0, 121);
      strncpy(message_text, message.c_str(), 120);
      if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
      {
        perror("mq_send error");
        exit(1);
      }
      mq_close(writequeue_fd);
    }
  }
  return;
}

void clean(int sigNum)
{
  gmp->map[current_byte] &= ~current_player;
  gmp->pid[player_number-1]=0;

  for(int i = 0; i<5; i++)
  {
    if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
      kill(gmp->pid[i], SIGUSR1);
  }

  mq_close(readqueue_fd);
  mq_unlink(mq_name[player_number-1].c_str());
  if(gmp->pid[0] == 0 && gmp->pid[1] == 0 && gmp->pid[2] == 0 && gmp->pid[3] == 0 && gmp->pid[4] == 0)  // if no players
  {
    shm_unlink("/goldmemory");
    sem_close(mysemaphore);
    sem_unlink("/goldchase_semaphore");
  }
  exit(1);
}

int main(int argc,char* argv[])   
{
  srand(time(NULL));              //for creating random no
  ifstream mapfile;                //fstream obj to open file
  string line;
  int shared_mem_fd;
  int syscall_result;
  int num_gold=0;
  unsigned short rows=0,cols=0;
  string mq="";

  struct sigaction exit_handler3;
  exit_handler3.sa_handler=clean;
  sigemptyset(&exit_handler3.sa_mask);
  exit_handler3.sa_flags=0;
  sigaction(SIGINT, &exit_handler3, NULL);
  sigaction(SIGTERM, &exit_handler3, NULL);
  sigaction(SIGHUP, &exit_handler3, NULL);

  struct sigaction exit_handler;
  exit_handler.sa_handler=map_handling;
  sigemptyset(&exit_handler.sa_mask);
  exit_handler.sa_flags=0;
  sigaction(SIGUSR1, &exit_handler, nullptr);
 
  exit_handler.sa_handler=read_queue;
  sigaction(SIGUSR2, &exit_handler, nullptr);

  struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=120;

  
  
  if (argc==2)                                          //if the first player try to open it with map file then argc=2
  {
    mysemaphore=sem_open("/goldchase_semaphore",O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR,1);          //creating a semaphore for multiplayer
    if(mysemaphore==SEM_FAILED)                          //if failed to open/create semaphore
    {
      if(errno==EEXIST)
           std::cerr << "The game is already running. Do not provide a map file.\n";
      else //any other kind of semaphore open failure
           perror("Create & open semaphore");
      exit(1);
    }
    syscall_result=sem_wait(mysemaphore);                 //grab semaphore
    if(syscall_result==-1)
    {
      perror("failed sem_wait");
      exit(1);
    }      
    //just grabbed semaphore
    mapfile.open(argv[1]);                              //opening the file given by user
    if(!mapfile)
    {
      perror ("Couldn't open file!\n");  //error opening file
      sem_close(mysemaphore);//close and remove semaphore
      sem_unlink("/goldchase_semaphore");
      exit(1);                  
    }
    if(mapfile.is_open())                             //if map file is open
    {
      getline(mapfile, line);                         //read first line of file 
      num_gold = atoi(line.c_str());                  //its a gold number string .. so convert it to integer
      while(getline(mapfile,line))                    //reading the reamaining lines from the map file
      {
        ++rows;   //counting rows of map
        cols=line.length()>cols? line.length():cols; //counting columes of map
      }
    }
    mapfile.close();   //close file
    shared_mem_fd=shm_open("/goldmemory",O_CREAT|O_EXCL|O_RDWR ,S_IRUSR|S_IWUSR);  //creating the shared memeory
    if(shared_mem_fd==-1)   //if error creating
    {
      perror("Error in shm_open");
      sem_close(mysemaphore);//close and remove semaphore
      sem_unlink("/goldchase_semaphore");
      exit(1);
    }
    syscall_result=ftruncate(shared_mem_fd,sizeof(Gold_Mine)+rows*cols);   //dynamically allocating size to the shared memeory
    if(syscall_result==-1)    //if error
    {
      perror("Error ftruncate");
      close(shared_mem_fd);
      shm_unlink("/goldmemory");
      sem_close(mysemaphore);//close and remove semaphore
      sem_unlink("/goldchase_semaphore");
      exit(1);
    }
    gmp=(Gold_Mine*)mmap(nullptr,sizeof(Gold_Mine)+rows*cols,PROT_READ|PROT_WRITE,MAP_SHARED,shared_mem_fd,0);  //map pages of memory
    if(gmp==MAP_FAILED)   //error handling
    {
      close(shared_mem_fd);
      shm_unlink("/goldmemory");
      sem_close(mysemaphore);//close and remove semaphore
      sem_unlink("/goldchase_semaphore");
      perror("MMAP FAILED ");
      exit(1);
    }
    //now initialize this memory with the number of rows in the map and columes;
    gmp->rows=rows;
    gmp->cols=cols;
    mapfile.open(argv[1]);  //opening map
    if(!mapfile.is_open())
    {
      cerr << "Couldn't open file!\n";  //error opening file
      sem_close(mysemaphore);//close and remove semaphore
      sem_unlink("/goldchase_semaphore");
      exit(1);                  
    }
    getline(mapfile, line);
    while(getline(mapfile,line))
    {
      for(int i=0; i<line.length(); ++i)   //reading map
      {
        if(line[i]=='*')
        {
          gmp->map[current_byte]=G_WALL;   //assigning * as G_WALL
          current_byte++;
        }
        else if(line[i]==' ')
        {
          gmp->map[current_byte]=0;        //assigning " " as 0
          current_byte++;
        }
        else
        {
          perror("Unknown characters in map file");
          close(shared_mem_fd);
          shm_unlink("/goldmemory");
          sem_close(mysemaphore);
          sem_unlink("/goldchase_semaphore");
          exit(1);
        }
      }
    }
    mapfile.close();   //close map
    if(num_gold!=0)
    {
    for(int i=0;i<num_gold-1;i++)        ///checking random number for the blank space i.e 0 to assing the fool gold
    { 
      current_byte=rand()%(rows*cols);
      
      while(gmp->map[current_byte]!=0)
      {
        current_byte=rand()%(rows*cols);
      }
      gmp->map[current_byte] |= G_FOOL;
    }

    current_byte=rand()%(rows*cols);
    while(gmp->map[current_byte]!=0)     ///checking random number for the blank space i.e 0 to assing the real gold
    {
        current_byte=rand()%(rows*cols);
    }
    gmp->map[current_byte] |= G_GOLD;
    }
    for(int i = 0; i < 5; i++)
    {
    gmp->pid[i] = 0;
    }

    gmp->pid[0] = getpid();
    current_player = G_PLR0;
    mq = mq_name[0];
    player_number = 1;
    current_byte=rand()%(rows*cols);
    while(gmp->map[current_byte]!=0)     ///checking random number for the blank space i.e 0 to assing the current player
    {
      current_byte=rand()%(rows*cols);
    }
    current_player=G_PLR0;
    gmp->map[current_byte] |= current_player;   //turn on current player bit in map at current byte
    syscall_result=sem_post(mysemaphore);               //post/release semaphore

    for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }

    if(syscall_result==-1){perror("failed sem_post");exit(1);}
    try{
    pointer_to_rendering_map=make_shared<Map>(gmp->map, gmp->rows, gmp->cols); }         //pointer to map
    catch(const std::exception& e)
    {
      cerr << e.what() << '\n';
      close(shared_mem_fd);
      shm_unlink("/goldmemory");
      sem_close(mysemaphore);
      sem_unlink("/goldchase_semaphore");
      exit(1);
    }
    if((readqueue_fd=mq_open(mq_name[0].c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
            S_IRUSR|S_IWUSR, &mq_attributes))==-1)
    {
      perror("mq_open.......1");
      exit(1);
    }
    //set up message queue to receive signal whenever message comes in
    struct sigevent mq_notification_event;
    mq_notification_event.sigev_notify=SIGEV_SIGNAL;
    mq_notification_event.sigev_signo=SIGUSR2;
    mq_notify(readqueue_fd, &mq_notification_event);
    pointer_to_rendering_map->postNotice("WELCOME TO GOLDCHASE");     //post to the map
  }

  else
  {
    mysemaphore=sem_open("/goldchase_semaphore", O_RDWR,S_IRUSR|S_IWUSR);   //open semaphore for subsequent players
    if(mysemaphore==SEM_FAILED)   //if error
    {
      if(errno==ENOENT)
        cerr << "Nobody is playing! Provide a map file to start the game.\n";
      else //any other kind of semaphore open failure
        perror("create and Open semaphore");
      exit(1);
    }
    syscall_result=sem_wait(mysemaphore);           //grab semaphore  for subsequent player
    if(syscall_result==-1)
    {
      perror("failed sem_wait");
      exit(1);
    } 
    shared_mem_fd=shm_open("/goldmemory", O_RDWR,S_IRUSR|S_IWUSR);   //open shared memeory for subsequent player
    if(shared_mem_fd==-1)
    {
      perror("Error in shm_open");
      sem_close(mysemaphore);//close semaphore
      exit(1);
    } 
    unsigned short rows;     
    unsigned short cols;
    int returned_value;
    returned_value=read(shared_mem_fd, &rows, sizeof(rows));   //read the rows from the shared memeory map
    if(returned_value==-1) { perror("Reading rows"); exit(1); }
    returned_value=read(shared_mem_fd, &cols, sizeof(cols));   //read the colums from the shared memory map
    if(returned_value==-1) { perror("Reading columns"); exit(1); }
    gmp=(Gold_Mine*)mmap(nullptr,sizeof(Gold_Mine)+rows*cols,PROT_READ|PROT_WRITE,MAP_SHARED,shared_mem_fd,0); //map pages of memeory
    if(gmp==MAP_FAILED)   //error handling
    {
      close(shared_mem_fd);
      sem_close(mysemaphore);//close semaphore
      perror("MMAP FAILED ");
      exit(1);
    }
    if((gmp->pid[0]==0))      //iterate through gmp->players looking for avialble spot
    {
      gmp->pid[0] = getpid();
      current_player = G_PLR0;
      mq = mq_name[0];
      player_number = 1;
    }
    else if((gmp->pid[1]==0))
    {
      gmp->pid[1] = getpid();
      current_player = G_PLR1;
      mq = mq_name[1];
      player_number = 2;
    }
    else if((gmp->pid[2]==0))
    {
      gmp->pid[2] = getpid();
      current_player = G_PLR2;
      mq = mq_name[2];
      player_number = 3;
    }
    else if((gmp->pid[3]==0))
    {
      gmp->pid[3] = getpid();
      current_player = G_PLR3;
      mq = mq_name[3];
      player_number = 4;
    }
    else if((gmp->pid[4]==0))
    {
      gmp->pid[4] = getpid();
      current_player = G_PLR4;
      mq = mq_name[4];
      player_number = 5;
    }
    else                                                                //close up shop & error out if all 5 player bits are active,                     
    {
      perror("5 players are playing..please try later!");
      close(shared_mem_fd);
      sem_close(mysemaphore);
      exit(1);
    }
    current_byte=rand()%(rows*cols);   
    while(gmp->map[current_byte]!=0)    //take random no. until find the blank space or 0
    {
      current_byte=rand()%(rows*cols);
    }
    gmp->map[current_byte] |= current_player;   //assigne the current player to blank space
    syscall_result=sem_post(mysemaphore);  //post semaphore
    for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }
    if(syscall_result==-1){perror("failed sem_post");return 2;}
    try{
    pointer_to_rendering_map=make_shared<Map>(gmp->map, gmp->rows, gmp->cols);}      //pointer to map
    catch(const std::exception& e)
    {
      cerr << e.what() << '\n';
      close(shared_mem_fd);
      shm_unlink("/goldmemory");
      sem_close(mysemaphore);
      sem_unlink("/goldchase_semaphore");
      exit(1);
    }
    for(int i = 0; i<5; i++)
    {
      if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
        kill(gmp->pid[i], SIGUSR1);
    }

    if((readqueue_fd=mq_open(mq.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,S_IRUSR|S_IWUSR, &mq_attributes))==-1)
    {
      perror("mq_open..............3");
      exit(1);
    }
    //set up message queue to receive signal whenever message comes in
    struct sigevent mq_notification_event;
    mq_notification_event.sigev_notify=SIGEV_SIGNAL;
    mq_notification_event.sigev_signo=SIGUSR2;
    mq_notify(readqueue_fd, &mq_notification_event);

    for(int i = 0; i<5; i++)
    {
      if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
        kill(gmp->pid[i], SIGUSR1);
    }
    pointer_to_rendering_map->postNotice("Welcome to goldChase");  //post to map
  }

  int Gold=0;
  char key;
  while((key=pointer_to_rendering_map->getKey() )!='Q')   //while loop
  {
      if(key =='h')   //left
      { 
        if(((current_byte%gmp->cols) != 0)) 
        {  
          if(!((gmp->map[current_byte-1]) & G_WALL))
          {
            gmp->map[current_byte] &= ~current_player;    //turn off bit in map
            current_byte--;                               //change current byte //move left
            gmp->map[current_byte] |= current_player;     //assigne player to changed byte in map
            pointer_to_rendering_map->drawMap();
            for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }
          }
        }
        else if(Gold)     //found gold won
        {
          gmp->map[current_byte] &= ~current_player;
          pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)
          {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
          }
          win = true;
          broadcasttoMqueue(player_number);
          pointer_to_rendering_map->postNotice("You WON!");
          break;
        }
      }//end left

    else if(key =='j')  //down
      { 
        if(((current_byte+gmp->cols) < (gmp->rows*gmp->cols))) 
        {  
          if(!((gmp->map[current_byte+gmp->cols]) & G_WALL))
          {
            gmp->map[current_byte] &= ~current_player;  //turn of player bit in map
            current_byte+=gmp->cols;                    //move down
            gmp->map[current_byte] |= current_player;   //turn on bit
            pointer_to_rendering_map->drawMap();
            for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }
          }
        }
        else if(Gold)     //found gold won
        {
          gmp->map[current_byte] &= ~current_player;
          pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }
          win = true;
          broadcasttoMqueue(player_number);
          pointer_to_rendering_map->postNotice("You WON!");
          break;
        }
      } //end down

    else if(key =='l') //right
      { 
        if(((current_byte+1)%gmp->cols) != 0) 
        {  
          if(!((gmp->map[current_byte+1]) & G_WALL))
          {
            gmp->map[current_byte] &= ~current_player; //turn off bit
            current_byte++;                            //move right
            gmp->map[current_byte] |= current_player;  //turn on bit
            pointer_to_rendering_map->drawMap();    //draw map
            for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }
          }
        }
        else if(Gold && (((current_byte+1)%gmp->cols)==0))    //found real gold //won
        {
          gmp->map[current_byte] &= ~current_player;
          pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)
          {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
          }
          win = true;
          broadcasttoMqueue(player_number);
          pointer_to_rendering_map->postNotice("You WON!");
          break;
        }
      } //end right

    else if(key=='k') //up
      { 
        if(((current_byte-gmp->cols) >= 0)) 
        {  
          if(!((gmp->map[current_byte-gmp->cols]) & G_WALL))
          {
            gmp->map[current_byte] &= ~current_player; //turn off bit
            current_byte-=gmp->cols;                    //move up
            gmp->map[current_byte] |= current_player;  //turn on bit
            pointer_to_rendering_map->drawMap();  //draw map
            for(int i = 0; i<5; i++)
            {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
            }
          }
        }
        else if(Gold)   //winner //found gold
        {
          gmp->map[current_byte] &= ~current_player;
          pointer_to_rendering_map->drawMap();
          for(int i = 0; i<5; i++)
          {
            if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
              kill(gmp->pid[i], SIGUSR1);
          }
          win = true;
          broadcasttoMqueue(player_number);
          pointer_to_rendering_map->postNotice("You WON!");
          break;
        }
      } //end up
      else if(key == 'm') //m
      {
        write_queue(current_player, player_number); 
      }
      else if(key == 'b') //b
      {
        broadcasttoMqueue(player_number);
      }
      if((gmp->map[current_byte]&G_FOOL))   //fools gold
      {
        gmp->map[current_byte] &= ~G_FOOL;
        pointer_to_rendering_map->postNotice("OOPS... Fool's Gold.");
      }
      if((gmp->map[current_byte]&G_GOLD))  //real gold
      {
        Gold ++;  //increment 
        gmp->map[current_byte] &= ~G_GOLD;  //turn off bit
        pointer_to_rendering_map->postNotice("You found the gold! congratulations !!!!");  //post on map
      }
  }//end while

  syscall_result=sem_wait(mysemaphore);    //semaphore wait
  if(syscall_result==-1)
  {
    perror("failed sem_wait");
    exit(1);
  }
  gmp->pid[player_number-1]=0;
  gmp->map[current_byte] &= ~current_player; ///turn off bit of current player on map
  sem_post(mysemaphore);  //semaphore post
  pointer_to_rendering_map->drawMap();
  for(int i = 0; i<5; i++)
  {
    if(gmp->pid[i] != 0 && gmp->pid[i] != getpid())
      kill(gmp->pid[i], SIGUSR1);
  }
  if(gmp->pid[0] == 0 && gmp->pid[1] == 0 && gmp->pid[2] == 0 && gmp->pid[3] == 0 && gmp->pid[4] == 0)  // if no players
  {
    close(shared_mem_fd);  //close shared memory
    shm_unlink("/goldmemory");  //unlink shared memory
    sem_close(mysemaphore);  //close semaphore
    sem_unlink("/goldchase_semaphore");     //unlink semaphore
  }
  mq_close(readqueue_fd);
  mq_unlink(mq_name[player_number-1].c_str()); 
  return 0;
}