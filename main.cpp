
#include "util.h"
#include "incremental_rb.h"
#include "mem.h"
#include "profiler.h"

#include <random>
#include <set>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#define NUM_TEST_FRAMES_TO_SIMULATE 100
#define GAMESTATE_SIZE MEGABYTES_BYTES(128 + 24)
constexpr u32 NUM_RANDOM_WRITES_PER_FRAME = 100;
constexpr u32 NUM_FRAMES_TO_ROLLBACK = MAX_ROLLBACK_FRAMES;

struct Inputs {
    u8 left;
    u8 right;
    u8 up;
    u8 down;
    u8 a;
    u8 b;
    u8 x;
    u8 y;
};

char* gameState = nullptr;
char* gameStatePrerollback = nullptr;
char* currentRender = nullptr;
s32 startRollbackFrame = -1;
s32 frameToRollbackTo = -1;
Inputs* p1Inputs = nullptr;
Inputs* p2Inputs = nullptr;
std::set<void*> writtenPages;
Inputs* p1InputsBackup = nullptr;
Inputs* p2InputsBackup = nullptr;

u64 renderSpot = 5 + 8 * 16 + 1; // arbitrary byte index into game mem block
u64 renderSpot2 = renderSpot+8 * 16 + 1;
inline char* GetGameState() { return gameState; }
inline u64 GetGamestateSize() { return GAMESTATE_SIZE; }
inline s32* GetGameMemFrame() { return (s32*)gameState; } // first 4 bytes of game mem are the current frame

s32 simFrames = 1;
u8 oneRandomChar() {
    std::random_device rd; // seed for PRNG
    std::mt19937 mt_eng(rd()); // mersenne-twister engine initialised with seed
    const int range_min = 0; // min of random interval
    const int range_max = 255; // max of random interval
    // uniform distribution for generating random integers in given range
    std::uniform_int_distribution<> dist(range_min, range_max);
    return dist(mt_eng);
}
void GenerateRandomInputs(Inputs& inputs)
{
    inputs.left = oneRandomChar();
    inputs.right = oneRandomChar();
    inputs.up = oneRandomChar();
    inputs.down = oneRandomChar();
    inputs.a = oneRandomChar();
    inputs.b = oneRandomChar();
    inputs.x = oneRandomChar();
    inputs.y = oneRandomChar();
}
void SimulateRender(u32 numWrites)
{
    PROFILE_FUNCTION();
    
    char* gameMem = GetGameState();
    u64 pageSize = (u64)GetPageSize();
    if (!gameMem) return;
    auto start = std::chrono::high_resolution_clock::now();
    // do some random writes to random spots in mem
    auto 
    #ifdef ENABLE_LOGGING
    void* unalignedSpot = (void*)(gameMem + renderSpot);
    void* pageAligned = (void*)(((u64)unalignedSpot) & ~(pageSize-1));
    *(u32*)pageAligned = *GetGameMemFrame();
    writtenPages.insert(pageAligned);
    #endif
    memcpy((Inputs*)(gameMem + renderSpot), &p1Inputs[*GetGameMemFrame() - 1], sizeof(Inputs));
    memcpy((Inputs*)(gameMem + renderSpot2), &p2Inputs[*GetGameMemFrame() - 1], sizeof(Inputs));
    memcpy(currentRender, (Inputs*)(gameMem + renderSpot), sizeof(Inputs));
    memcpy(currentRender + sizeof(Inputs) + 1, (Inputs*)(gameMem + renderSpot2), sizeof(Inputs));
}
void PostSimulateFrame(u32 numWrites)
{
    PROFILE_FUNCTION();
    u64 spotToWrite = 5 + sizeof(Inputs) * 2 + 1; // arbitrary byte index into game mem block
    char* gameMem = GetGameState();
    u64 pageSize = (u64)GetPageSize();
    if (!gameMem) return;
    auto start = std::chrono::high_resolution_clock::now();
    // do some random writes to random spots in mem
    auto spotToWrite2 = spotToWrite+sizeof(Inputs) * 2 + 1;
    #ifdef ENABLE_LOGGING
    void* unalignedSpot = (void*)(gameMem + spotToWrite);
    void* pageAligned = (void*)(((u64)unalignedSpot) & ~(pageSize-1));
    *(u32*)pageAligned = *GetGameMemFrame();
    writtenPages.insert(pageAligned);
    #endif
    memcpy((Inputs*)(gameMem + spotToWrite), &p1Inputs[*GetGameMemFrame() - 1], sizeof(Inputs));
    memcpy((Inputs*)(gameMem + spotToWrite2), &p2Inputs[*GetGameMemFrame() - 1], sizeof(Inputs));
}
void GameSimulateFrame(s32& currentFrame, u32 numWrites)
{
    PROFILE_FUNCTION();
    u64 spotToWrite = 5 + currentFrame; // arbitrary byte index into game mem block
    char* gameMem = GetGameState();
    u64 pageSize = (u64)GetPageSize();
    if (!gameMem) return;
    // do some random writes to random spots in mem
    auto spotToWrite2 = spotToWrite+sizeof(Inputs)+1;
    memcpy((Inputs*)(gameMem + spotToWrite), &p1Inputs[currentFrame], sizeof(Inputs));
    memcpy((Inputs*)(gameMem + spotToWrite2), &p2Inputs[currentFrame], sizeof(Inputs));
}


bool randomBool() {
    std::random_device rd; // seed for PRNG
    std::mt19937 mt_eng(rd()); // mersenne-twister engine initialised with seed
    const int range_min = 0; // min of random interval
    const int range_max = 1; // max of random interval
    // uniform distribution for generating random integers in given range
    std::uniform_int_distribution<> dist(range_min, range_max);
    return dist(mt_eng);
}
s32 randomNumRollbackFrames() {
    std::random_device rd; // seed for PRNG
    std::mt19937 mt_eng(rd()); // mersenne-twister engine initialised with seed
    const int range_min = 3; // min of random interval
    const int range_max = MAX_ROLLBACK_FRAMES; // max of random interval
    // uniform distribution for generating random integers in given range
    std::uniform_int_distribution<> dist(range_min, range_max);
    return dist(mt_eng);
}
void Tick(s32& frame, bool isResim, bool isSim)
{
    PROFILER_FRAME_MARK();
    PROFILE_FUNCTION();
    #ifdef ENABLE_LOGGING
    printf("------------ FRAME %i ---------------\n", frame);
    #endif
    auto start = std::chrono::high_resolution_clock::now();
    // We cannot rollback to frame 0 atm due to frame 0 not having the changes from frame -1, so uhhh yeah idk this is probably bad tbh
    if(isSim)
    {
        memcpy(&p1Inputs[*GetGameMemFrame()], &p1InputsBackup[*GetGameMemFrame()], sizeof(Inputs));
        memcpy(&p2Inputs[*GetGameMemFrame()], &p2InputsBackup[*GetGameMemFrame()], sizeof(Inputs));
        // mess with our game memory. This is to simulate the game sim
        GameSimulateFrame(frame, NUM_RANDOM_WRITES_PER_FRAME);
    }
    #ifdef ENABLE_LOGGING
    printf("Advancing internal frame %u -> %u\n", frame, frame + 1);
    // first 4 bytes of the game memory will be our "internal" game frame
    #endif
    frame++;
    #ifdef ENABLE_LOGGING
    writtenPages.insert(GetGameMemFrame());
    #endif
    if(isSim)
    {
        PostSimulateFrame(NUM_RANDOM_WRITES_PER_FRAME);
    }
    OnFrameEnd(frame - 1, isResim);
    #ifdef ENABLE_LOGGING
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    auto timecount = duration.count();
    printf("GameSimulateFrame %llu ms [frame %i] [wrote %llu pages]\n", timecount, frame, writtenPages.size());
    writtenPages.clear();
    #endif
}
void printInputs(Inputs& inputs)
{
    printf("%i - %i - %i - %i - %i - %i - %i - %i\n", inputs.left, inputs.right, inputs.up, inputs.down, inputs.a, inputs.b, inputs.x, inputs.y);
}
int main()
{
    // virtualalloc always allocs on page boundaries (when base addr is null)
    gameState = (char*)VirtualAlloc(nullptr, GAMESTATE_SIZE, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    gameStatePrerollback = (char*)VirtualAlloc(nullptr, GAMESTATE_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    currentRender = (char*)VirtualAlloc(nullptr, sizeof(Inputs) * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    p1Inputs = new Inputs[NUM_TEST_FRAMES_TO_SIMULATE];
    p2Inputs = new Inputs[NUM_TEST_FRAMES_TO_SIMULATE];
    p1InputsBackup = new Inputs[NUM_TEST_FRAMES_TO_SIMULATE];
    p2InputsBackup = new Inputs[NUM_TEST_FRAMES_TO_SIMULATE];
    *GetGameMemFrame() = 0;
    IncrementalRBCallbacks cbs;
    cbs.getGamestateSize = GetGamestateSize;
    cbs.getGameState = GetGameState;
    cbs.getGameMemFrame = GetGameMemFrame;
    Init(cbs);
    writtenPages = {};
    while (*GetGameMemFrame() < NUM_TEST_FRAMES_TO_SIMULATE) 
    {
        int i = 0;
        simFrames = 1;
        printf("-- START SIMULATION FRAME %d --\n", *GetGameMemFrame());
        GenerateRandomInputs(p1Inputs[*GetGameMemFrame()]);
        GenerateRandomInputs(p2Inputs[*GetGameMemFrame()]);
        p1InputsBackup[*GetGameMemFrame()] = p1Inputs[*GetGameMemFrame()];
        p2InputsBackup[*GetGameMemFrame()] = p2Inputs[*GetGameMemFrame()];
        
        if (*GetGameMemFrame() > MAX_ROLLBACK_FRAMES + 1 && randomBool())
        {
            // rollback
            frameToRollbackTo = *GetGameMemFrame() - randomNumRollbackFrames();
            startRollbackFrame = *GetGameMemFrame();
            // if we're at frame 15, and rollback to frame 10,
            // we end up at the end of frame 9/beginning of frame 10.
            rbMemcpy(gameStatePrerollback, GetGameState(), GetGamestateSize());
            Rollback(startRollbackFrame, frameToRollbackTo);
            
            #ifdef ENABLE_LOGGING
            printf("Rolled back to frame %i\n", *GetGameMemFrame());
            #endif
            simFrames = startRollbackFrame - frameToRollbackTo;
            #ifdef ENABLE_LOGGING
            printf("Sim Frames: %i\n", simFrames);
            printf("Local Frame: %i\n", *GetGameMemFrame());
            printf("Rollback To Frame: %i\n", frameToRollbackTo);
            printf("Start Rollback Frame: %i\n", startRollbackFrame);
            #endif
        }
        while(i < simFrames)
        {
            Tick(*GetGameMemFrame(), simFrames > 1, true);
            i++;
        }
        SimulateRender(NUM_RANDOM_WRITES_PER_FRAME);
        if(simFrames > 1)
        {
            printf("-- THE BIG TEST --\n");
            printf("%d == %d?\n", startRollbackFrame, *GetGameMemFrame());
            assert(startRollbackFrame == *GetGameMemFrame());
            if(memcmp(gameStatePrerollback, GetGameState(), GAMESTATE_SIZE) != 0)
            {
                for (int i=0;i<GAMESTATE_SIZE;++i)
                {
                    if (gameStatePrerollback[i] != GetGameState()[i])
                    {
                        printf("DIFFERENCE AT INDEX %d: PREROLLBACK -- %d, CURRENT -- %d\n", i, gameStatePrerollback[i], GetGameState()[i]);
                    }
                }
                assert(false);
            }
            else 
            {
                printf("-- PASS FOR FRAME %d --\n", *GetGameMemFrame() - 1);
                assert(true);
            }
            Tick(*GetGameMemFrame(), false, true);
            SimulateRender(NUM_RANDOM_WRITES_PER_FRAME);
        }
        printf("-- END SIMULATION FRAME %d --\n", *GetGameMemFrame() - 1);
    }
    delete[] p1Inputs;
    delete[] p2Inputs;
    delete[] p1InputsBackup;
    delete[] p2InputsBackup;
    Shutdown();
    #ifdef _WIN32
    system("pause");
    #endif
    return 0;
}





