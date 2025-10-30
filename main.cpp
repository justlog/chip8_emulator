#if defined(_WIN32)
  #include <SDL.h>
#else
  #include <SDL2/SDL.h>
#endif

#include <iostream>
#include <unordered_map>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <math.h>
#include <assert.h>



typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef bool b8;
std::vector<char> LoadROM(const char* path){
    std::ifstream inputFile(path, std::ios::binary);
    if (!inputFile){
        std::cerr << "Unable to open file " << path << std::endl;
        exit(1);
    }
    inputFile.seekg(0, std::ios::end);
    std::streamsize fileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    if(inputFile.read(buffer.data(), fileSize)){
        return buffer;
    }
    else{
        std::cerr << "Error reading file " << path << std::endl;
        exit(1);
    }
}
//
// Screen dimensions
const u8 CHIP8_DISPLAY_WIDTH = 64;
const u8 CHIP8_DISPLAY_HEIGHT = 32;
const int SCREEN_WIDTH = 8*CHIP8_DISPLAY_WIDTH;
const int SCREEN_HEIGHT = 8*CHIP8_DISPLAY_HEIGHT;

#define MEM_ALLOC_ERR() std::cerr << "Could not allocate memory! Quitting..."; exit(1);
//
//NOTE 1KB? maybe we'll need more? who knows.
#define STACK_SIZE 1024 
struct Stack{
  u16 memory[STACK_SIZE];
  u32 counter = 0;
  void Push(u16 address){
    assert(counter < STACK_SIZE);
    memory[counter++] = address;
  }
  u16 Pop(){
    if(counter > 0){
      return memory[--counter];
    }
    return 0;
  }
};
struct Chip8Context {
	i8 *ram;
	b8 display[CHIP8_DISPLAY_HEIGHT][CHIP8_DISPLAY_WIDTH];
	u16 PC;
	u16 indexRegister;
  Stack stack;
	u8 delayTimer;
	u8 soundTimer;
	union {
		struct {
			u8 V0;
			u8 V1;
			u8 V2;
			u8 V3;
			u8 V4;
			u8 V5;
			u8 V6;
			u8 V7;
			u8 V8;
			u8 V9;
			u8 VA;
			u8 VB;
			u8 VC;
			u8 VD;
			u8 VE;
			u8 VF;
		};
		u8 registers[16];
	};
	union{
		struct{
			b8 btn0;
			b8 btn1;
			b8 btn2;
			b8 btn3;
			b8 btn4;
			b8 btn5;
			b8 btn6;
			b8 btn7;
			b8 btn8;
			b8 btn9;
			b8 btnA;
			b8 btnB;
			b8 btnC;
			b8 btnD;
			b8 btnE;
			b8 btnF;
		};
		b8 buttons[16];
	};
  u32 instructionPerformed = 0;
  b8 getKey = false;
  u8 getKeyPressed = 0xFF;
};

constexpr u32 BYTES_PER_FONT = 5;
constexpr u8 FONT[] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};
void ClearDisplay(Chip8Context* ctx){
  for(u32 i = 0; i < CHIP8_DISPLAY_HEIGHT; i++)
    for(u32 j = 0; j < CHIP8_DISPLAY_WIDTH; j++)
      ctx->display[i][j] = false;
}
void InitChip8Context(Chip8Context* ctx){
	//Allocate memory for ram
	ctx->ram = (i8*)malloc(4096);
	if(!ctx->ram) {
		MEM_ALLOC_ERR();
	}
  //Copy Font
  for(u32 i = 0; i < sizeof(FONT)/sizeof(FONT[0]); i++)
    ctx->ram[i] = FONT[i];
	ctx->PC = 0x200;
}

constexpr u16 OP_MASK = 15 << 12;
constexpr u16 X_MASK = 15 << 8;
constexpr u16 Y_MASK = 15 << 4;
constexpr u16 N_MASK = 15 << 0;
constexpr u16 NN_MASK = 255 << 0;
constexpr u16 NNN_MASK = 4095;

#define MASK_OP(inst) (((inst) & OP_MASK) >> 12)
#define MASK_X(inst) (((inst) & X_MASK) >> 8)
#define MASK_Y(inst) (((inst) & Y_MASK) >> 4)
#define MASK_N(inst) (((inst) & N_MASK) >> 0)
#define MASK_NN(inst) (((inst) & NN_MASK) >> 0)
#define MASK_NNN(inst) (u16)(((inst) & NNN_MASK) >> 0)


enum Operation {
	CLS = 0x00E0,
  RET = 0x00EE,
	JP = 0x1000,
  CALL = 0x2000,
  SE_IMM = 0x3000,
  SNE_IMM = 0x4000,
  SE_REG = 0x5000,
  SNE_REG = 0x9000,
	LDX_IMM = 0x6000,
  LDX_REG = 0x8000,
  ORX_REG = 0x8001,
  ANDX_REG = 0x8002,
  XORX_REG = 0x8003,
	ADDX_REG = 0x8004,
  SUB_REG = 0x8005,
  SUBN_REG = 0x8007,
  /*NOTE: shifting are ambiguous instructions: 
  the CHIP-8 interpreter for the original COSMAC VIP, this instruction did the following: It put the value of VY into VX, and then shifted the value in VX 1 bit to the right (8XY6) or left (8XYE). VY was not affected, but the flag register VF would be set to the bit that was shifted out.
  However, starting with CHIP-48 and SUPER-CHIP in the early 1990s, these instructions were changed so that they shifted VX in place, and ignored the Y completely.
  
  This is one of the main differences between implementations that cause problems for programs. Since different games expect different behavior, you could consider making the behavior configurable by the user.*/
  SHR = 0x8006,
  SHL = 0x800E,
	ADDX_IMM = 0x7000,
	SETI = 0xA000,
  /*Ambiguous instruction!
  In the original COSMAC VIP interpreter, this instruction jumped to the address NNN plus the value in the register V0. This was mainly used for “jump tables”, to quickly be able to jump to different subroutines based on some input.
  Starting with CHIP-48 and SUPER-CHIP, it was (probably unintentionally) changed to work as BXNN: It will jump to the address XNN, plus the value in the register VX. So the instruction B220 will jump to address 220 plus the value in the register V2.
  The BNNN instruction was not widely used, so you might be able to just implement the first behavior (if you pick one, that’s definitely the one to go with). If you want to support a wide range of CHIP-8 programs, make this “quirk” configurable.*/
  JPOFFSET = 0xB000,//TODO: I can try to configure this later on, supposedly the instruction is not widely used.
  RND = 0xC000,
  //Skip if de/pressed
  SKP = 0xE09E,
  SKNP = 0xE0A1,
	DRAW = 0xD000,
  //Timers
  LDX_TIMER = 0xF007,
  LD_DT = 0xF015,//Load delay timer
  LD_ST = 0xF018,//Load sound timer
  ADDI_X = 0xF01E,//Unlike other arithmetic instructions, this did not affect VF on overflow on the original COSMAC VIP. However, it seems that some interpreters set VF to 1 if I “overflows” from 0FFF to above 1000 (outside the normal addressing range). This wasn’t the case on the original COSMAC VIP, at least, but apparently the CHIP-8 interpreter for Amiga behaved this way. At least one known game, Spacefight 2091!, relies on this behavior. I don’t know of any games that rely on this not happening, so perhaps it’s safe to do it like the Amiga interpreter did.
  LD_KEY = 0xF00A,
  LD_FONT = 0xF029,//Load I register with the address of font for character in X
  BCD = 0xF033,




  /*
  * Ambiguous instruction!
  *
  * These two instructions store registers to memory, or load them from memory, respectively.
  *
  * For FX55, the value of each variable register from V0 to VX inclusive (if X is 0, then only V0) will be stored in successive memory addresses, starting with the one that’s stored in I. V0 will be stored at the address in I, V1 will be stored in I + 1, and so on, until VX is stored in I + X.
*
* FX65 does the opposite; it takes the value stored at the memory addresses and loads them into the variable registers instead.
*
* The original CHIP-8 interpreter for the COSMAC VIP actually incremented the I register while it worked. Each time it stored or loaded one register, it incremented I. After the instruction was finished, I would end up being set to the new value I + X + 1.
*
* However, modern interpreters (starting with CHIP48 and SUPER-CHIP in the early 90s) used a temporary variable for indexing, so when the instruction was finished, I would still hold the same value as it did before.
*
  * If you only pick one behavior, go with the modern one that doesn’t actually change the value of I. This will let you run the common CHIP-8 games you find everywhere, and it’s also what the common test ROMs depend on (the other behavior will fail the tests). But if you want your emulator to run older games from the 1970s or 1980s, you should consider making a configurable option in your emulator to toggle between these behaviors.
*/
  ST_MEM = 0xF055,
  LD_MEM = 0xF065,
  SENTINEL_OP = 0xFFFF,
};

std::unordered_map<Operation, std::string> OperationToString = {
  {CLS,"CLS"},
  {RET,"RET"},
  {JP,"JP"},
  {CALL,"CALL"},
  {SE_IMM,"SE_IMM"},
  {SNE_IMM,"SNE_IMM"},
  {SE_REG,"SE_REG"},
  {SNE_REG,"SNE_REG"},
  {LDX_IMM,"LDX_IMM"},
  {LDX_REG,"LDX_REG"},
  {ORX_REG,"ORX_REG"},
  {ANDX_REG,"ANDX_REG"},
  {XORX_REG,"XORX_REG"},
  {ADDX_REG,"ADDX_REG"},
  {SUB_REG,"SUB_REG"},
  {SUBN_REG,"SUBN_REG"},
  {SHR,"SHR"},
  {SHL,"SHL"},
  {ADDX_IMM,"ADDX_IMM"},
  {SETI,"SETI"},
  {JPOFFSET,"JPOFFSET"},
  {RND,"RND"},
  {SKP,"SKP"},
  {SKNP,"SKNP"},
  {DRAW,"DRAW"},
  {LDX_TIMER,"LDX_TIMER"},
  {LD_DT,"LD_DT"},
  {LD_ST,"LD_ST"},
  {ADDI_X,"ADDI_X"},
  {LD_KEY,"LD_KEY"},
  {LD_FONT,"LD_FONT"},
  {BCD,"BCD"},
  {ST_MEM,"ST_MEM"},
  {LD_MEM,"LD_MEM"},
  {SENTINEL_OP,"SENTINEL_OP"},
};
Operation GetOperation(u16 inst)
{
  u16 maskedInst = inst & OP_MASK;
  Operation op = SENTINEL_OP;
  switch(maskedInst){
    case 0:
      switch(inst){
        case Operation::CLS:
          op = Operation::CLS;
          break;
        case Operation::RET:
          op = Operation::RET;
          break;
          break;
      }
    case 0xE000:
      switch(inst & 0xE0FF){//Need to mask off X to get the opcode 
        case Operation::SKP:
          op = Operation::SKP;
          break;
        case Operation::SKNP:
          op = Operation::SKNP;
          break;
      }
      break;
    case 0xF000:
      switch(inst & 0xF0FF){
        case Operation::LDX_TIMER:
          op = Operation::LDX_TIMER;
          break;
        case Operation::LD_DT:
          op = Operation::LD_DT;
          break;
        case Operation::LD_ST:
          op = Operation::LD_ST;
          break;
        case Operation::ADDI_X:
          op = Operation::ADDI_X;
          break;
        case Operation::LD_KEY:
          op = Operation::LD_KEY;
          break;
        case Operation::LD_FONT:
          op = Operation::LD_FONT;
          break;
        case Operation::BCD:
          op = Operation::BCD;
          break;
        case Operation::ST_MEM:
          op = Operation::ST_MEM;
          break;
        case Operation::LD_MEM:
          op = Operation::LD_MEM;
          break;
      }
      break;
    case 0x8000:
      switch(inst & 0x800f){
        case Operation::LDX_REG:
          op = Operation::LDX_REG;
          break;
        case Operation::ORX_REG:
          op = Operation::ORX_REG;
          break;
        case Operation::ANDX_REG:
          op = Operation::ANDX_REG;
          break;
        case Operation::XORX_REG:
          op = Operation::XORX_REG;
          break;
        case Operation::ADDX_REG:
          op = Operation::ADDX_REG;
          break;
        case Operation::SUB_REG:
          op = Operation::SUB_REG;
          break;
        case Operation::SUBN_REG:
          op = Operation::SUBN_REG;
          break;
        case Operation::SHR:
          op = Operation::SHR;
          break;
        case Operation::SHL:
          op = Operation::SHL;
          break;
      }
      break;
    case Operation::CALL:
      op = Operation::CALL;
      break;
    case Operation::SE_IMM:
      op = Operation::SE_IMM;
      break;
    case Operation::SNE_IMM:
      op = Operation::SNE_IMM;
      break;
    case Operation::SE_REG:
      op = Operation::SE_REG;
      break;
    case Operation::SNE_REG:
      op = Operation::SNE_REG;
      break;
    case Operation::JP:
      op = Operation::JP;
      break;
    case Operation::DRAW:
      op = Operation::DRAW;
      break;
    case Operation::LDX_IMM:
      op = Operation::LDX_IMM;
      break;
    case Operation::ADDX_IMM:
      op = Operation::ADDX_IMM;
      break;
    case Operation::SETI:
      op = Operation::SETI;
      break;
    case Operation::RND:
      op = Operation::RND;
      break;
    default:
      std::cout << "Could not preform instruction " << std::hex << inst << std::endl; 
      break;
  }
  if(op == Operation::SENTINEL_OP){
    std::cout << "Instruction not implemented: " << std::hex << inst << std::endl;
  }
  return op;
}
//Currently uses SDL's built-in integer scaling. Could implement it myself.
//Also currently draws each point to the screen separately, should use a texture instead. 
void DrawDisplay(SDL_Renderer* renderer, Chip8Context& ctx, u8 X, u8 Y, u8 N)
{
  // std::cout << "Draw display function" << std::endl;
  u8 x = ctx.registers[X] % CHIP8_DISPLAY_WIDTH;
  u8 y = ctx.registers[Y] % CHIP8_DISPLAY_HEIGHT;
  u8 countBytes = N;
  for(u8 row = 0; row < countBytes; row++){
    u8 pixelByte = ctx.ram[ctx.indexRegister+row];
    for(u8 pixel = 0; pixel < 8; pixel++){
      b8 color = pixelByte & (1 << pixel);
      u8 cellX = x+7-pixel;//Draw from most significant bit to least significant bit
      u8 cellY = y+row;
      if(cellX < CHIP8_DISPLAY_WIDTH && cellY < CHIP8_DISPLAY_HEIGHT){
        ctx.display[cellY][cellX] ^= color;
      }
    }
  }
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  //TODO: SLOW, Optimize to use a texture.
  for(u32 row = 0; row < CHIP8_DISPLAY_HEIGHT; ++row){
    for(u32 column = 0; column < CHIP8_DISPLAY_WIDTH; ++column){
      if(ctx.display[row][column]){
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      }
      else{
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      }
      SDL_RenderDrawPoint(renderer, column, row);
    }
  }
  SDL_RenderPresent(renderer);
}


#define SAMPLE_RATE 48000

typedef struct {
    double phase;
    double freq;
    double volume;
} AudioData;
void audio_callback(void* userdata, Uint8* stream, int len) {
    AudioData* data = (AudioData*)userdata;
    float* buffer = (float*)stream;
    int samples = len / sizeof(float);

    double phase = data->phase;
    double freq = data->freq;
    double volume = data->volume;
    double phase_increment = 2.0 * M_PI * freq / SAMPLE_RATE;

    for (int i = 0; i < samples; i++) {
        // Square wave "buzz" tone
        float value = (sin(phase) > 0 ? 1.0f : -1.0f) * volume;
        buffer[i] = value;
        phase += phase_increment;
        if (phase >= 2.0 * M_PI)
            phase -= 2.0 * M_PI;
    }

    data->phase = phase;
}




int main(int argc, char* argv[]) {
  if(argc < 2){
    std::cerr << "Need to supply CHIP8 emulator with a ROM." << std::endl;
    return 1;
  }
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return 1;
	} 
	SDL_Window* window = SDL_CreateWindow(
		"SDL2 Template",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		SDL_WINDOW_SHOWN
	);


  if (!window) {
    std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }

	// Create renderer
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}
  //NOTE: Using SDL built-in integer scaling. If I want to, I could try to implement this myself.
  SDL_RenderSetLogicalSize(renderer, CHIP8_DISPLAY_WIDTH, CHIP8_DISPLAY_HEIGHT);
  SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

	//Audio
	AudioData data = {0, 22.0, 0.2}; 
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = SAMPLE_RATE;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 512;
  want.callback = audio_callback;
  want.userdata = &data;
  if(SDL_OpenAudio(&want, &have) < 0){
    std::cerr << "Failed to open audio " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }







  Chip8Context ctx = {0};
  InitChip8Context(&ctx);
  auto buffer = LoadROM(argv[1]);
  for(auto byte : buffer){
    ctx.ram[ctx.PC++] = byte;
  }
  ctx.PC = 0x200;




  bool quit = false;
  SDL_Event e;

	
  u32 cycle = 1;
  b8 emulate = true;
  std::unordered_map<SDL_Scancode, u8> buttonMap = {
    {SDL_SCANCODE_1, 0x1},{SDL_SCANCODE_2, 0x2},{SDL_SCANCODE_3, 0x3},{SDL_SCANCODE_4, 0xC},
    {SDL_SCANCODE_Q, 0x4},{SDL_SCANCODE_W, 0x5}, {SDL_SCANCODE_E, 0x6},{SDL_SCANCODE_R, 0xD},
    {SDL_SCANCODE_A, 0x7},{SDL_SCANCODE_S, 0x8},{SDL_SCANCODE_D, 0x9},{SDL_SCANCODE_F, 0xE},
    {SDL_SCANCODE_Z, 0xA},{SDL_SCANCODE_X, 0x0},{SDL_SCANCODE_C, 0xB},{SDL_SCANCODE_V, 0xF},
  };
  u64 counterFrequency = SDL_GetPerformanceFrequency();
  u64 lastFrame = SDL_GetPerformanceCounter();
  f64 lastTimerTick = 0.0;
  constexpr f64 msPerTimerTick = (1.0/60.0) * 1000.0;
  constexpr u32 INST_PER_SECOND = 700;//TODO: https://tobiasvl.github.io/blog/write-a-chip-8-emulator/ claims the speed should be reconfigurable because of different processors it ran on. but it also says a "standard" speed of 700 instructions per second fits well enough for chip8 programs.
  f64 emulationTimer = 0.0;
  f64 msPerInst = (1.0/INST_PER_SECOND) * 1000;
	// Main loop
	while (!quit) {
    //Timer ticks
    //TODO: currently busy waiting, switch to non-busy?
    u64 currentFrame = SDL_GetPerformanceCounter();
    f64 dt = ((currentFrame - lastFrame) / (f64)counterFrequency) * 1000;//in ms
    lastTimerTick += dt;
    emulationTimer += dt;
    lastFrame = currentFrame;
    if(lastTimerTick >= msPerTimerTick){
      if(ctx.delayTimer > 0){
        ctx.delayTimer -= 1;
      }
      if(ctx.soundTimer > 0){
        ctx.soundTimer -= 1;
        if(ctx.soundTimer == 0){
          SDL_PauseAudio(1);
        }
      }
      lastTimerTick -= msPerTimerTick;
    }
    if(emulationTimer >= msPerInst){
      if(emulate){
        std::cout << "EMULATION TOO SLOW, ONLY " << ctx.instructionPerformed << " WERE PREFORMED" << std::endl;
      }
      emulationTimer -= msPerInst;
      emulate = true;
    }

		// Handle events
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT)
				quit = true;
      //TODO: Currently it seems input is being dropped, probably because the emulation and the input logic is mismatched (someone can <down><up> a key and the emulator wouldn't recognize it because we only see press down
      if(e.type == SDL_KEYDOWN){
        if(auto it = buttonMap.find(e.key.keysym.scancode); it != buttonMap.end()){
          std::cout << "chip8 key " << SDL_GetKeyName(SDL_GetKeyFromScancode(e.key.keysym.scancode)) << " pressed" << std::endl;
          ctx.buttons[it->second] = true;
        }
      }
			if (e.type == SDL_KEYUP){//TODO Controls
        if(auto it = buttonMap.find(e.key.keysym.scancode); it != buttonMap.end()){
          std::cout << "chip8 key " << SDL_GetKeyName(SDL_GetKeyFromScancode(e.key.keysym.scancode)) << " released" << std::endl;
          if(ctx.getKey){//TODO: Not sure that this is the right logic, might need to keep state for all the buttons (states for both pressed and released).
            ctx.getKeyPressed = it->second;//On the original COSMAC VIP, the key was only registered when it was pressed and then released.
          }
          ctx.buttons[it->second] = false;
        }
			}
		}
    if(emulate){
      u8 byte1 = ctx.ram[ctx.PC++];
      u8 byte2 = ctx.ram[ctx.PC++];
      u16 inst = (((u16)byte1) << 8) | ((u16)byte2);

      u8 X = (u8)MASK_X(inst);
      u8 Y = (u8)MASK_Y(inst);
      u8 N = (u8)MASK_N(inst);
      u8 NN = (u8)MASK_NN(inst);
      u16 NNN = (u16)MASK_NNN(inst);

      Operation op = GetOperation(inst);
      assert(op != SENTINEL_OP);
      // std::cout << "Preforming operation " << OperationToString.at(op) << std::endl;
      switch(op){
        case Operation::CLS:
          // Clear screen with black
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
          SDL_RenderClear(renderer);
          SDL_RenderPresent(renderer);
          ClearDisplay(&ctx);
          break;
        case Operation::RET:
          ctx.PC = ctx.stack.Pop();
          break;
        case Operation::SKP:
          if(ctx.registers[X] <= 0xF && ctx.buttons[ctx.registers[X]]){
            ctx.PC += 2;
          }
          break;
        case Operation::SKNP:
          if(ctx.registers[X] <= 0xF && !ctx.buttons[ctx.registers[X]]){
            ctx.PC += 2;
          }
          break;
        case Operation::LDX_TIMER:
          ctx.registers[X] = ctx.delayTimer;
          break;
        case Operation::LD_DT:
          ctx.delayTimer = ctx.registers[X];
          break;
        case Operation::LD_ST:
          ctx.soundTimer = ctx.registers[X];
          SDL_PauseAudio(0);
          break;
        case Operation::ADDI_X:
          //NOTE:  Unlike other arithmetic instructions, this did not affect VF on overflow on the original COSMAC VIP. However, it seems that some interpreters set VF to 1 if I “overflows” from 0FFF to above 1000 (outside the normal addressing range). This wasn’t the case on the original COSMAC VIP, at least, but apparently the CHIP-8 interpreter for Amiga behaved this way. At least one known game, Spacefight 2091!, relies on this behavior. I don’t know of any games that rely on this not happening, so perhaps it’s safe to do it like the Amiga interpreter did.
          /* NOTE: From Emulator Development discord regarding setting VF: We still have not come accross an emulator that shows that behavior and is from that time period of the only game that seems to profit from it,
           * and actually that game (Spacefight 2091) is just broken and would not be fixed by that "quirk", it would just fix a part of the issues, and a really fixed version was made instead.
           * So I see that "quirk" as a myth and it can actually break games and fixes none. All emulators implementing that behavior are from much later and just repeat the myth without knowing why.
           * Programs for any known variant do not expect their VF to be destroyed by Fx1E, so it is a bad idea.
          */
          // if(ctx.indexRegister + ctx.registers[X] > 0x1000){
          //   ctx.VF = 1;
          // }
          ctx.indexRegister += ctx.registers[X];
          break;
        case Operation::LD_KEY:
          if(ctx.getKey && ctx.getKeyPressed <= 0xF){
            ctx.registers[X] = ctx.getKeyPressed;
            ctx.getKeyPressed = 0xFF; //Set back to invalid value.
            ctx.getKey = false;
          }
          else{
            ctx.getKey = true;
            ctx.PC -= 2;
          }
          break;
        case Operation::LD_FONT:
          ctx.indexRegister = (ctx.registers[X] & 0xF)*BYTES_PER_FONT;
          break;
        case Operation::BCD:
          {
            u8 b = ctx.registers[X];
            i8 i = 2;
            while(i >= 0){
              ctx.ram[ctx.indexRegister + i--] = b%10;
              b /= 10;
            }
          }
          break;
        case Operation::ST_MEM:
          for(u8 i = 0; i <= X; i++){
            ctx.ram[ctx.indexRegister + i] = ctx.registers[i];
          }
          break;
        case Operation::LD_MEM:
          for(u8 i = 0; i <= X; i++){
            ctx.registers[i] = ctx.ram[ctx.indexRegister + i];
          }
          break;
        case Operation::CALL:
          ctx.stack.Push(ctx.PC);
          ctx.PC = NNN;
          break;
        case Operation::SE_IMM:
          if(ctx.registers[X] == NN){
            ctx.PC += 2;
          }
          break;
        case Operation::SNE_IMM:
          if(ctx.registers[X] != NN){
            ctx.PC += 2;
          }
          break;
        case Operation::SE_REG:
          if(ctx.registers[X] == ctx.registers[Y]){
            ctx.PC += 2;
          }
          break;
        case Operation::SNE_REG:
          if(ctx.registers[X] != ctx.registers[Y]){
            ctx.PC += 2;
          }
          break;
        case Operation::JP:
          ctx.PC = NNN;
          break;
        case Operation::DRAW:
          DrawDisplay(renderer, ctx, X, Y, N);
          break;
        //---- 0x8000 instructions, need to mask last nibble aswell
        case Operation::LDX_REG:
          ctx.registers[X] = ctx.registers[Y];
          break;
        case Operation::ORX_REG:
          ctx.registers[X] |= ctx.registers[Y];
          break;
        case Operation::ANDX_REG:
          ctx.registers[X] &= ctx.registers[Y];
          break;
        case Operation::XORX_REG:
          ctx.registers[X] ^= ctx.registers[Y];
          break;
        case Operation::ADDX_REG:
          {
            u16 sum = (u16)ctx.registers[X] + (u16)ctx.registers[Y];
            ctx.registers[X] = (u8)sum;
            ctx.VF = sum > 255 ? 1 : 0;
          }
          break;
        case Operation::SUB_REG:
          {
            const u8 notBorrow = ctx.registers[X] >= ctx.registers[Y] ? 1 : 0;//NOTE: For some reason >= fixes rom number 4 even though the spec says it should set VF only on x > y...
            const u8 diff = ctx.registers[X] - ctx.registers[Y];
            ctx.registers[X] = diff;
            ctx.VF = notBorrow;
          }
          break;
        case Operation::SUBN_REG:
          ctx.registers[X] = ctx.registers[Y] - ctx.registers[X];
          ctx.VF =  ctx.registers[X] < ctx.registers[Y] ? 1 : 0;
          break;
        //NOTE shifting are ambiguous instructions, might want to have configurable behaviour, see enum defintion.
        case Operation::SHR:
          {
            u8 borrow = (ctx.registers[X] & 0x1) ? 1 : 0;
            u8 result = ctx.registers[X] >> 1;
            ctx.registers[X] = result;
            ctx.VF = borrow;
          }
          break;
        case Operation::SHL:
          {
            u8 borrow = (ctx.registers[X] & 0x80) ? 1 : 0;
            u8 result = ctx.registers[X] << 1;
            ctx.registers[X] = result;
            ctx.VF = borrow;
          }
          break;
        case Operation::LDX_IMM:
          ctx.registers[X] = NN;
          break;
        case Operation::ADDX_IMM:
          ctx.registers[X] += NN;
          break;
        case Operation::SETI:
          ctx.indexRegister = NNN;
          break;
        case Operation::RND:
          {
            u8 rand = (u8)(std::rand() % 255);
            ctx.registers[X] = NN & rand;//TODO: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM points to the instruction AND of the chip8. but https://tobiasvl.github.io/blog/write-a-chip-8-emulator/ doesn't mention it. I wonder if the behaviour is supposed to match the regular instruction (i.e. set the VF register)
            break;
          }
        default:
          std::cout << "Could not preform instruction " << std::hex << inst << std::endl; 
          break;
      }
      ctx.instructionPerformed++;
      emulate = false;
    }
	}

	// Clean up
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

