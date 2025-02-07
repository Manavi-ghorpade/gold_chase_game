# Gold Chase (Multiplayer Console Game)

## Overview
Gold Chase is a multiplayer console-based game implemented in C++ using distributed programming concepts. The game supports 1-5 players who navigate a map in search of real gold while avoiding fool's gold. The first player to find the real gold and exit the map wins.

This project incorporates **shared memory, semaphores, signal handling, and message queues** to coordinate multiple processes. The game map is rendered using **ncurses and panels**, with synchronization mechanisms ensuring smooth gameplay across different players.

## Features
- **Multiplayer support** (1-5 players) using shared memory
- **Real-time game updates** via message queues
- **Synchronization using semaphores** to prevent simultaneous memory access
- **Signal handling** for process management and quitting
- **Interactive terminal interface** powered by ncurses

## Rules of the Game
- Players move using the following keys:
  - `h` - Left
  - `j` - Down
  - `k` - Up
  - `l` - Right
- Players must navigate the map to find the **real gold** while avoiding fool's gold.
- If a player finds the real gold and moves off the edge of the map, they win the game.
- Players cannot move through walls or beyond the map boundaries.
- Players can pass through each other.
- Press `Q` to quit the game at any time.

## Technical Details
### Map Representation
- The map is stored in a **shared memory segment** as an array of bytes.
- The first game process initializes the shared memory and loads the map from a text file.
- Subsequent processes join the game using the existing shared memory.
- The last process to exit **unlinks the shared memory segment**.
- Each byte in the shared memory represents **walls, gold, fool’s gold, and players** using bitwise flags.

### Synchronization & Interprocess Communication
- **Semaphores** ensure only one process writes to shared memory at a time.
- **Message queues** enable automatic screen updates and chat functionality between players.
- **Signal handling** manages process cleanup and game exit conditions.

### Libraries Used
- **ncurses & panel** (`-lncurses -lpanel`) for game rendering.
- **rt library** (`-lrt`) for shared memory management.
- **pthread library** (`-lpthread`) for semaphore-based synchronization.

## Deployment & Execution
### Compilation
To compile the project, use the provided **Makefile**:
```sh
make
```

### Running the Game
Start the first player (this initializes shared memory and semaphores):
```sh
./goldchase <map_file>
```
Additional players can join by executing the same command.

### Exiting the Game
- A player can quit by pressing `Q`.
- The last remaining player **unlinks** the shared memory segment automatically.

## Project Goals & Achievements
- **Completion Timeline:** 4 weeks
- **Deployment Time Optimization:** Reduced by 50%
- **Efficient process coordination** using interprocess communication techniques

## Future Enhancements
- Implement real-time notifications when a player wins.
- Introduce AI-controlled players for single-player mode.
- Improve the game UI for better user experience.

---
**Technologies Used:** C++, Shared Memory, Semaphores, Sockets, Message Queues, ncurses, Distributed Programming

