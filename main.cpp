#include <SDL2/SDL.h>
#include <iostream>
#include <cstdint>



typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef bool b8;
//
// Screen dimensions
const int SCREEN_WIDTH = 64;
const int SCREEN_HEIGHT = 32;
const u8 CHIP8_DISPLAY_WIDTH = 64;
const u8 CHIP8_DISPLAY_HEIGHT = 32;

/*
	* Font to be used
	* 0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	* 0x20, 0x60, 0x20, 0x20, 0x70, // 1
	* 0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	* 0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	* 0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	* 0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	* 0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	* 0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	* 0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	* 0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	* 0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	* 0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	* 0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	* 0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	* 0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	* 0xF0, 0x80, 0xF0, 0x80, 0x80  // F
*/
struct Chip8Context {
	i8 *ram;
	b8 display[64][32];
	i16 PC;
	i16 indexRegister;
	i16 *stack; //TODO: What is the size?
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
	} GPRegisters;
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
		b8 controls[16];
	};
};

#define MEM_ALLOC_ERR() std::cerr << "Could not allocate memory! Quitting..."; exit(1);
//NOTE 1KB? maybe we'll need more? who knows.
#define STACK_SIZE 1024 
void InitChip8Context(Chip8Context* ctx){
	//Allocate memory for ram and stack
	ctx->ram = (i8*)malloc(4096);
	if(!ctx->ram) {
		MEM_ALLOC_ERR();
	}
	ctx->stack = (i16*)malloc(STACK_SIZE);
	if(!ctx->stack) {
		MEM_ALLOC_ERR();
	}
	ctx->PC = 200;
}

constexpr u16 OP_MASK = 15 << 12;
constexpr u16 X_MASK = 15 << 8;
constexpr u16 Y_MASK = 15 << 4;
constexpr u16 N_MASK = 15 << 0;
constexpr u16 NN_MASK = 255 << 0;
constexpr u16 NNN_MASK = 4095;


enum Operation {
	CLEAR_SCREEN = 0,
	JUMP = 1,
	SET_REGISTER_X = 6,
	ADD_VALUE_TO_X = 7,
	SET_INDEX_I = 0xA,
	DRAW = 0xD
};

int main(int argc, char* argv[]) {
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return 1;
	} // Create window
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

	Chip8Context ctx = {0};
	InitChip8Context(&ctx);
	//TODO: Load game into ram




	bool quit = false;
	SDL_Event e;

	
	// Main loop
	while (!quit) {
		// Handle events
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT)
				quit = true;
			if (e.type == SDL_KEYDOWN){//TODO Controls
			}
		}
		u8 byte1 = ctx.ram[ctx.PC++];
		u8 byte2 = ctx.ram[ctx.PC++];
		u16 inst = (((u16)byte1) << 8) & ((u16)byte2);

		u8 op = inst & OP_MASK;
		u8 X = inst & X_MASK;
		u8 Y = inst & Y_MASK;
		u8 N = inst & N_MASK;
		u8 NN = inst & NN_MASK;
		u8 NNN = inst & NNN_MASK;

		switch(op){
			case Operation::CLEAR_SCREEN:
                // Clear screen with black
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                break;
            case Operation::JUMP:
                ctx.PC = NNN;
                break;
            case Operation::DRAW:
                {
                    u8 x = ctx.GPRegisters.registers[X] % CHIP8_DISPLAY_WIDTH;
                    u8 y = ctx.GPRegisters.registers[Y] % CHIP8_DISPLAY_HEIGHT;
                }
                break;
            case Operation::SET_REGISTER_X:
                ctx.GPRegisters.registers[X] = NN;
                break;
            case Operation::ADD_VALUE_TO_X:
                ctx.GPRegisters.registers[X] += NN;
                break;
            case Operation::SET_INDEX_I:
                ctx.indexRegister = NNN;
                break;
		}
		// Clear screen with black
		// SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		// SDL_RenderClear(renderer);

		// Set draw color to red and draw a rectangle
		// SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
		// SDL_Rect rect = { SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2 - 50, 100, 100 };
		// SDL_RenderFillRect(renderer, &rect);

		// Update screen
		SDL_RenderPresent(renderer);
	}

	// Clean up
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

