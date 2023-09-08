#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_timer.h>
#include <unistd.h>
#include <time.h>

#define first_nible (ins & 0xf000) >> 12
#define second_nible (ins & 0x0f00) >> 8
#define third_nible (ins & 0x00f0) >> 4
#define fourth_nible (ins & 0x000f)

typedef enum{
    RUNNING,
    NOT_RUNNING
} states;

typedef struct{
    __uint8_t RAM[4096]; //Stores data regarding the program
    __uint8_t display[64 * 32]; //Stores the value of pixels that will be displayed
    __uint16_t PC; //Points at current instruction in memory(RAM)
    __uint16_t I; //Points at locations in memory(RAM)
    __uint16_t stack[24]; //Stores 16-bit addresses which is used to call subroutines/functions and return from them  
    __uint8_t sp; //Stores the index value which pointes to the top  of the stack 
    __uint8_t registers[16]; //General-purpose variable registers 
    __uint8_t delay_timer; //Delay timer which is decremented at a rate of 60 Hz until it reaches 0
    __uint8_t sound_timer; //Sound timer which functions like the delay timer, but which also gives off a beeping sound as long as it’s not 0
    __uint8_t keys[16]; //Checks if a key is pressed by turning the coresponding index in keys to true
    states state; //The state of the emulator Running/Not-Running
} chip8;



void initialize_chip8(chip8 *chip8_object_ptr, FILE *rom){
    
    //The font represents the hexidacimal number 0-F
    char fonts[] = {
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

    //Load fonts into chip8 memory
    memcpy(chip8_object_ptr->RAM, fonts, sizeof(fonts));  

        //Get size of ROM in bytes
    fseek(rom, 0, SEEK_END);
    long size = ftell(rom);
    rewind(rom);

    //Where the ROM should be loaded at RAM
    __uint16_t start = 0x200;
    
    //Load ROM data into chip8 memory
    fread(chip8_object_ptr->RAM + 0x200, size, 1, rom);
    
    //Set program counter to the start of the program
    chip8_object_ptr->PC = start;

    //There is no stack yet
    chip8_object_ptr->sp = -1;

    //Set the state of the emulator to RUNNING
    chip8_object_ptr->state = RUNNING;

    //Clear diplay 0 = black
    for(__uint16_t i=0; i < sizeof chip8_object_ptr->display; i++){
        chip8_object_ptr->display[i] = 0;
    }
    
    //Set keys array elements to 0
    //0 = No key presses 
    for(__uint8_t i = 0; i<16; i++){
        chip8_object_ptr->keys[i] = 0;
    }
}

void audio_callback(void *userdata, __uint8_t *stream, int len){
    userdata = 0;

    int16_t *audio_data = (int16_t *)stream;
    static uint32_t running_sample_index = 0;
    const int32_t square_wave_period = 44100 / 440;
    const int32_t half_square_wave_period = square_wave_period / 2;

    // We are filling out 2 bytes at a time (int16_t), len is in bytes,
    //   so divide by 2
    // If the current chunk of audio for the square wave is the crest of the wave, 
    //   this will add the volume, otherwise it is the trough of the wave, and will add
    //   "negative" volume
    for (int i = 0; i < len / 2; i++)
        audio_data[i] = ((running_sample_index++ / half_square_wave_period) % 2) ? 
                        3000 : 
                        -3000;
}

void initialize_sdl(SDL_Window **screen, SDL_Renderer **renderer, SDL_AudioDeviceID *dev, SDL_AudioSpec *want, SDL_AudioSpec *have){
    // returns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
    }

    *screen = SDL_CreateWindow("Chip8",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    64 * 20, 32 * 20, SDL_WINDOW_RESIZABLE);

    if(!(*screen)) {
        printf("Could not create screen\n");
        exit(1);
    }

    *renderer = SDL_CreateRenderer(*screen, -1, 0);
    if(!(*renderer)){
        printf("Error creating renderer\n");
        exit(1);
    }    


    want->freq = 44100;
    want->format = AUDIO_S16LSB;
    want->channels = 1;
    want->samples = 512;        /* A good value for games */
    want->callback = audio_callback;

    *dev = SDL_OpenAudioDevice(NULL, 0, want, have, 0);

    /* Open the audio device and start playing sound! */
    if (SDL_OpenAudio(want, NULL) < 0) {
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        exit(1);
    }
}

void destroy_sdl(SDL_Window *screen, SDL_AudioDeviceID *dev){
    SDL_CloseAudioDevice(*dev);
    SDL_DestroyWindow(screen);
    SDL_Quit();
}

void user_input(chip8 *chip8_object_ptr){
    SDL_Event event;
    
    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT:
                chip8_object_ptr->state = NOT_RUNNING;
                break;
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym){
                    case SDLK_1:
                        chip8_object_ptr->keys[0x1] = 1;
                        break;
                    case SDLK_2:
                         chip8_object_ptr->keys[0x2] = 1;
                        break;
                    case SDLK_3:
                        chip8_object_ptr->keys[0x3] = 1;
                        break;
                    case SDLK_4:
                        chip8_object_ptr->keys[0xC] = 1;
                        break;

                    case SDLK_q:
                        chip8_object_ptr->keys[0x4] = 1;
                        break;
                    case SDLK_w:
                         chip8_object_ptr->keys[0x5] = 1;
                        break;
                    case SDLK_e:
                         chip8_object_ptr->keys[0x6] = 1;
                        break;
                    case SDLK_r:
                         chip8_object_ptr->keys[0xD] = 1;
                        break;
                
                    case SDLK_a:
                        chip8_object_ptr->keys[0x7] = 1;
                        break;
                    case SDLK_s:
                         chip8_object_ptr->keys[0x8] = 1;
                        break;
                    case SDLK_d:
                         chip8_object_ptr->keys[0x9] = 1;
                        break;
                    case SDLK_f:
                        chip8_object_ptr->keys[0xE] = 1;
                        break;
                
                    case SDLK_z:
                        chip8_object_ptr->keys[0xA] = 1;
                        break;
                    case SDLK_x:
                         chip8_object_ptr->keys[0x0] = 1;
                        break;
                    case SDLK_c:
                         chip8_object_ptr->keys[0xB] = 1;
                        break;
                    case SDLK_v:
                        chip8_object_ptr->keys[0xF] = 1;
                        break;
                }
                break;
            case SDL_KEYUP:
                switch(event.key.keysym.sym){
                    case SDLK_1:
                        chip8_object_ptr->keys[0x1] = 0;
                        break;
                    case SDLK_2:
                        chip8_object_ptr->keys[0x2] = 0;
                        break;
                    case SDLK_3:
                        chip8_object_ptr->keys[0x3] = 0;
                        break;
                    case SDLK_4:
                        chip8_object_ptr->keys[0xC] = 0;
                        break;

                    case SDLK_q:
                        chip8_object_ptr->keys[0x4] = 0;
                        break;
                    case SDLK_w:
                        chip8_object_ptr->keys[0x5] = 0;
                        break;
                    case SDLK_e:
                        chip8_object_ptr->keys[0x6] = 0;
                        break;
                    case SDLK_r:
                        chip8_object_ptr->keys[0xD] = 0;
                        break;
                
                    case SDLK_a:
                        chip8_object_ptr->keys[0x7] = 0;
                        break;
                    case SDLK_s:
                        chip8_object_ptr->keys[0x8] = 0;
                        break;
                    case SDLK_d:
                        chip8_object_ptr->keys[0x9] = 0;
                        break;
                    case SDLK_f:
                        chip8_object_ptr->keys[0xE] = 0;
                        break;
                
                    case SDLK_z:
                        chip8_object_ptr->keys[0xA] = 0;
                        break;
                    case SDLK_x:
                        chip8_object_ptr->keys[0x0] = 0;
                        break;
                    case SDLK_c:
                        chip8_object_ptr->keys[0xB] = 0;
                        break;
                    case SDLK_v:
                        chip8_object_ptr->keys[0xF] = 0;
                        break;
                }
                break;
            default:
                break;
        }
    }
}

void set_register_value(chip8 *chip8_object_ptr, __uint16_t ins){
    //Calculate register number from opcode
    __uint8_t reg_num = second_nible;
            
    //Calculate value from opcode
    __uint8_t value = (ins & 0x00ff);

    //Assign value to register
    chip8_object_ptr->registers[reg_num] = value; 
}

void add_register_value(chip8 *chip8_object_ptr, __uint16_t ins){
    //Calculate register number from opcode
    __uint8_t reg_num = second_nible;
            
    //Calculate value from opcode
    __uint8_t value = (ins & 0x00ff);
    
    //Add value to register
    chip8_object_ptr->registers[reg_num] += value; 
}

void clear_screen(chip8 *chip8_object_ptr){
    memset(chip8_object_ptr->display, 0x0, 2048);
}

void set_pc(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->PC = (ins & 0x0fff);
}

void set_i(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->I = (ins & 0x0fff);
}

void display_fun(chip8 *chip8_object_ptr, __uint16_t ins){
            // 0xDXYN: Draw N-height sprite at coords X,Y; Read from memory location I;
            //   Screen pixels are XOR'd with sprite bits, 
            //   VF (Carry flag) is set if any screen pixels are set off; This is useful
            //   for collision detection or other reasons.
            uint8_t X = chip8_object_ptr->registers[second_nible] % 64;
            uint8_t Y = chip8_object_ptr->registers[third_nible] % 32;
            uint8_t n = fourth_nible;
            const uint8_t orig_X = X; // Original X value

            chip8_object_ptr->registers[0xF] = 0;  // Initialize carry flag to 0

            // Loop over all N rows of the sprite
            for (uint8_t i = 0; i < n; i++) {
                // Get next byte/row of sprite data
                const uint8_t sprite_data = chip8_object_ptr->RAM[chip8_object_ptr->I + i];
                X = orig_X;   // Reset X for next row to draw

                for (int8_t j = 7; j >= 0; j--) {
                    // If sprite pixel/bit is on and display pixel is on, set carry flag
                    if ((sprite_data & (1 << j)) && (chip8_object_ptr->display[Y * 64 + X])) {
                        chip8_object_ptr->registers[0xF] = 1;  
                    }

                    // XOR display pixel with sprite pixel/bit to set it on or off
                    (chip8_object_ptr->display[Y * 64 + X]) ^= (sprite_data & (1 << j));

                    // Stop drawing this row if hit right edge of screen
                    if (++X >= 64) break;
                }

                // Stop drawing entire sprite if hit bottom edge of screen
                if (++Y >= 32) break;
            }
}

void draw(SDL_Renderer *renderer, chip8 *chip8_object_ptr){
    SDL_Rect rect = {.x = 0, .y = 0, .w = 20, .h = 20};
    
    uint32_t fg_color = 0xFFFFFFFF;
    uint32_t bg_color = 0x000000FF;

    __uint8_t fg_r = (fg_color >> 24) & 0xFF;
    __uint8_t fg_g = (fg_color >> 16) & 0xFF;
    __uint8_t fg_b = (fg_color >> 8) & 0xFF;
    __uint8_t fg_a = (fg_color >> 0) & 0xFF; 

    __uint8_t bg_r = (bg_color >> 24) & 0xFF;
    __uint8_t bg_g = (bg_color >> 16) & 0xFF;
    __uint8_t bg_b = (bg_color >> 8) & 0xFF;
    __uint8_t bg_a = (bg_color >> 0) & 0xFF;

    for(__uint32_t i = 0; i < 2048; i++){
        rect.x = (i % 64) * 20;
        rect.y = (i / 64) * 20;

       if(chip8_object_ptr->display[i]){
            SDL_SetRenderDrawColor(renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(renderer, &rect);
        }else{
            SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(renderer, &rect);  
        }
    
     
    }
    SDL_RenderPresent(renderer);
}

void call_subroutine(chip8 *chip8_object_ptr, __uint16_t ins){
    //Update stack pointer
    chip8_object_ptr->sp++;
    
    //Store the next instruction address on the stack
    chip8_object_ptr->stack[chip8_object_ptr->sp] = chip8_object_ptr->PC + 2;
    
    //Go to subroutine
    chip8_object_ptr->PC = (ins & 0x0fff);
}

void return_from_subroutine(chip8 *chip8_object_ptr){
    //Assign PC the value on the top of the stack
    chip8_object_ptr->PC = chip8_object_ptr->stack[chip8_object_ptr->sp];
    
    //Update stack pointer
    chip8_object_ptr->sp--;
}

void skip_constant_equal(chip8 *chip8_object_ptr, __uint16_t ins){
    if(chip8_object_ptr->registers[second_nible] == (ins & 0x00ff)){
        chip8_object_ptr->PC+=2;
    }
}

void skip_not_constant_equal(chip8 *chip8_object_ptr, __uint16_t ins){
    if(chip8_object_ptr->registers[second_nible] != (ins & 0x00ff)){
        chip8_object_ptr->PC+=2;
    }
}

void skip_register_equal(chip8 *chip8_object_ptr, __uint16_t ins){
    if(chip8_object_ptr->registers[second_nible] == chip8_object_ptr->registers[third_nible]){
        chip8_object_ptr->PC+=2;
    }
}

void skip_register_not_equal(chip8 *chip8_object_ptr, __uint16_t ins){
    if(chip8_object_ptr->registers[second_nible] != chip8_object_ptr->registers[third_nible]){
        chip8_object_ptr->PC+=2;
    }
}

void jump_with_offset(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->PC = ((ins & 0x0fff) + chip8_object_ptr->registers[0x0]);
}

void random(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint8_t nn = ins & 0x00ff;
    chip8_object_ptr->registers[second_nible] = (rand() % 256) & nn;
}

void set_vx_vy(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->registers[second_nible] = chip8_object_ptr->registers[third_nible];
}

void binary_or(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->registers[second_nible] |= chip8_object_ptr->registers[third_nible];
}

void binary_and(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->registers[second_nible] &= chip8_object_ptr->registers[third_nible];
}

void binary_xor(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->registers[second_nible] ^= chip8_object_ptr->registers[third_nible];
}

void subtract_vx_vy(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint8_t carry = chip8_object_ptr->registers[second_nible] > chip8_object_ptr->registers[third_nible];
    
    chip8_object_ptr->registers[second_nible] -= chip8_object_ptr->registers[third_nible];
    
    chip8_object_ptr->registers[0xf] = carry;
}

void subtract_vy_vx(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint8_t carry = chip8_object_ptr->registers[second_nible] < chip8_object_ptr->registers[third_nible];
    
    chip8_object_ptr->registers[second_nible] = chip8_object_ptr->registers[third_nible] - chip8_object_ptr->registers[second_nible];

    chip8_object_ptr->registers[0xf] = carry;
}

void add(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint16_t carry = (uint16_t)(chip8_object_ptr->registers[second_nible] + chip8_object_ptr->registers[third_nible] > 255);
    
    chip8_object_ptr->registers[second_nible] += chip8_object_ptr->registers[third_nible];

    chip8_object_ptr->registers[0xf] = carry;
} 

void shift_right(chip8 *chip8_object_ptr, __uint16_t ins){
    //chip8_object_ptr->registers[second_nible] = chip8_object_ptr->registers[third_nible]; 
    __uint8_t carry = chip8_object_ptr->registers[third_nible] & 0x01;

    chip8_object_ptr->registers[second_nible] = chip8_object_ptr->registers[third_nible] >> 1;

    chip8_object_ptr->registers[0xf] = carry;
}

void shift_left(chip8 *chip8_object_ptr, __uint16_t ins){
    //chip8_object_ptr->registers[second_nible] = chip8_object_ptr->registers[third_nible]; 
    __uint8_t carry = (chip8_object_ptr->registers[third_nible] & 0x80) >> 7;
    
    chip8_object_ptr->registers[second_nible] = chip8_object_ptr->registers[third_nible] << 1;
        
    chip8_object_ptr->registers[0xf] = carry;
}

void store_memory(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint8_t n = second_nible;
    
    for(int i=0; i<=n; i++){
        chip8_object_ptr->RAM[chip8_object_ptr->I + i] = chip8_object_ptr->registers[i];
    }
}

void load_memory(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint8_t n = second_nible;

    for(int i=0; i<=n; i++){
        chip8_object_ptr->registers[i] = chip8_object_ptr->RAM[chip8_object_ptr->I + i];
    }
}

void add_to_index(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->I += chip8_object_ptr->registers[second_nible];
}

void decimal_conversion(chip8 *chip8_object_ptr, __uint16_t ins){
    __uint8_t n = chip8_object_ptr->registers[second_nible];
    __uint8_t values[] = {n / 100, (n % 100) / 10, (n % 100) % 10}; 
    for(int i=0; i<3; i++){
        chip8_object_ptr->RAM[chip8_object_ptr->I + i] = values[i];
    }
}

void font_char(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->I = (chip8_object_ptr->registers[second_nible]) * 5;
}

void set_vx_delaytimer(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->registers[second_nible] = chip8_object_ptr->delay_timer;
}

void set_delaytimer_vx(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->delay_timer = chip8_object_ptr->registers[second_nible];
}

void set_soundtimer_vx(chip8 *chip8_object_ptr, __uint16_t ins){
    chip8_object_ptr->sound_timer = chip8_object_ptr->registers[second_nible];
}

void get_key(chip8 *chip8_object_ptr, __uint16_t ins){
    static __uint8_t key_pressed = 0;
    static __uint8_t key;
    
    __uint8_t i = 0;
    
    while((i < 16) && (!key_pressed)){
        if(chip8_object_ptr->keys[i]){
            key = i;
            key_pressed = 1;
            break;
        }
        i++;
    }

    if(!key_pressed){
        chip8_object_ptr->PC-=2;
    }else{
        if(chip8_object_ptr->keys[key]){
            chip8_object_ptr->PC-=2;
        }else{
            chip8_object_ptr->registers[second_nible] = key;
            key = -1;
            key_pressed = 0;
        }
    }
}

void skip_if_key(chip8 *chip8_object_ptr, __uint16_t ins){
    if(chip8_object_ptr->keys[chip8_object_ptr->registers[second_nible]]){
        chip8_object_ptr->PC+=2;
    }
}

void skip_if_not_key(chip8 *chip8_object_ptr, __uint16_t ins){
    if(!chip8_object_ptr->keys[chip8_object_ptr->registers[second_nible]]){
        chip8_object_ptr->PC+=2;
    }
}

void decrement_delay_timer(chip8 *chip8_obj_ptr){
    if(chip8_obj_ptr->delay_timer > 0){
        chip8_obj_ptr->delay_timer--;
    }
}

void decrement_sound_timer(chip8 *chip8_obj_ptr, SDL_AudioDeviceID *dev){
    if(chip8_obj_ptr->sound_timer > 0){
        SDL_PauseAudioDevice(*dev, 0);
        chip8_obj_ptr->sound_timer--;
    }else{
        SDL_PauseAudioDevice(*dev, 1);
    }
}

void debug(chip8 *chip8_obj_ptr, __uint16_t ins){
    
    printf("%hx  ", chip8_obj_ptr->PC);
    
    switch (first_nible)
    {
    case 0x0:
        if(ins & 0x00e0){
            // 0x00E0: Clear the screen
            printf("Clear screen\n");
        }else if(ins & 0x00ee){
            // 0x00E0: Return from subroutine
            printf("Return from subroutine to address 0x%04X\n",
            chip8_obj_ptr->stack[chip8_obj_ptr->sp]);
        }else{
            printf("Uniplimented opcode\n");
        }
        break;
    case 0x1:
        // 0x1NNN: Jump to address NNN
        printf("Jump to address NNN (0x%04X)\n",
            (ins & 0x0fff));   
        break;
    case 0x2:
        // 0x2NNN: Call subroutine at NNN
        printf("Call subroutine at NNN (0x%04X)\n",
            (ins & 0x0fff));
        break;
    case 0x3:
        // 0x3XNN: Check if VX == NN, if so, skip the next instruction
        printf("Check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true\n",
                   second_nible, chip8_obj_ptr->registers[second_nible], ins & 0x00ff);
        break;
    case 0x4:
        // 0x4XNN: Check if VX != NN, if so, skip the next instruction
        printf("Check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true\n",
                   second_nible, chip8_obj_ptr->registers[second_nible], ins & 0x00ff);
        break;
    case 0x5:
        // 0x5XY0: Check if VX == VY, if so, skip the next instruction
        printf("Check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true\n",
            second_nible, chip8_obj_ptr->registers[second_nible], 
            third_nible, chip8_obj_ptr->registers[third_nible]);
        break;
    case 0x06:
        // 0x6XNN: Set register VX to NN
        printf("Set register V%X = NN (0x%02X)\n",
            second_nible, ins & 0x00ff);
        break;
    case 0x07:
        // 0x7XNN: Set register VX += NN
        printf("Set register V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n",
            second_nible, chip8_obj_ptr->registers[second_nible], ins & 0x00ff,
            chip8_obj_ptr->registers[second_nible] + (ins & 0x00ff));
        break;
    case 0x8:
        switch (fourth_nible)
        {
        
        case 0:
            // 0x8XY0: Set register VX = VY
            printf("Set register V%X = V%X (0x%02X)\n",
                second_nible, third_nible, chip8_obj_ptr->registers[third_nible]);
            break;
        case 1:
            // 0x8XY1: Set register VX |= VY
            printf("Set register V%X (0x%02X) |= V%X (0x%02X); Result: 0x%02X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                third_nible, chip8_obj_ptr->registers[third_nible],
                chip8_obj_ptr->registers[second_nible] | chip8_obj_ptr->registers[third_nible]);
            break;
        case 2:
            // 0x8XY2: Set register VX &= VY
            printf("Set register V%X (0x%02X) &= V%X (0x%02X); Result: 0x%02X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                third_nible, chip8_obj_ptr->registers[third_nible],
                chip8_obj_ptr->registers[second_nible] & chip8_obj_ptr->registers[third_nible]);
            break;
        case 3:
            // 0x8XY3: Set register VX ^= VY
            printf("Set register V%X (0x%02X) ^= V%X (0x%02X); Result: 0x%02X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                third_nible, chip8_obj_ptr->registers[third_nible],
                chip8_obj_ptr->registers[second_nible] ^ chip8_obj_ptr->registers[third_nible]);
             break;
        case 4:
            // 0x8XY4: Set register VX += VY, set VF to 1 if carry
            printf("Set register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                third_nible, chip8_obj_ptr->registers[third_nible],
                chip8_obj_ptr->registers[second_nible] + chip8_obj_ptr->registers[third_nible],
                ((uint16_t)chip8_obj_ptr->registers[second_nible] + chip8_obj_ptr->registers[third_nible]) > 255);
            break;
        case 5:
            // 0x8XY5: Set register VX -= VY, set VF to 1 if there is not a borrow (result is positive/0)
            printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                third_nible, chip8_obj_ptr->registers[third_nible],
                chip8_obj_ptr->registers[second_nible] - chip8_obj_ptr->registers[third_nible],
                (chip8_obj_ptr->registers[second_nible] <= chip8_obj_ptr->registers[third_nible]));
            break;
        case 6:
            // 0x8XY6: Set register VX >>= 1, store shifted off bit in VF
            printf("Set register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                chip8_obj_ptr->registers[second_nible] & 1,
                chip8_obj_ptr->registers[second_nible] >> 1);
            break;
        case 7:
            // 0x8XY7: Set register VX = VY - VX, set VF to 1 if there is not a borrow (result is positive/0)
            printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                second_nible, third_nible, chip8_obj_ptr->registers[third_nible],
                second_nible, chip8_obj_ptr->registers[second_nible],
                chip8_obj_ptr->registers[third_nible] - chip8_obj_ptr->registers[second_nible],
                (chip8_obj_ptr->registers[second_nible] <= chip8_obj_ptr->registers[third_nible]));
            break;
        case 0xE:
            // 0x8XYE: Set register VX <<= 1, store shifted off bit in VF
            printf("Set register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                second_nible, chip8_obj_ptr->registers[second_nible],
                (chip8_obj_ptr->registers[second_nible] >> 7) & 0x1,
                chip8_obj_ptr->registers[second_nible] << 1);
            break;
        }
    break;
    case 0x9:
        // 0x9XY0: Check if VX != VY; Skip next instruction if so
        printf("Check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true\n",
            second_nible, chip8_obj_ptr->registers[second_nible], 
            third_nible, chip8_obj_ptr->registers[third_nible]);
        break;
    case 0xa:
        // 0xANNN: Set index register I to NNN
        printf("Set I to NNN (0x%04X)\n",
            ins & 0x0fff);
        break;
    case 0xb:
        // 0xBNNN: Jump to V0 + NNN
        printf("Set PC to V0 (0x%02X) + NNN (0x%04X); Result PC = 0x%04X\n",
            chip8_obj_ptr->registers[0x0], ins & 0x0fff, chip8_obj_ptr->registers[0x0] + (ins & 0x0fff));
        break;
    case 0xc:
        // 0xCXNN: Sets register VX = rand() % 256 & NN (bitwise AND)
        printf("Set V%X = rand() %% 256 & NN (0x%02X)\n",
            second_nible, ins & 0x00ff);
        break;
    case 0xd:
        // 0xDXYN: Draw N-height sprite at coords X,Y; Read from memory location I;
        //   Screen pixels are XOR'd with sprite bits, 
        //   VF (Carry flag) is set if any screen pixels are set off; This is useful
        //   for collision detection or other reasons.
        printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) "
            "from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n",
            fourth_nible, second_nible, chip8_obj_ptr->registers[second_nible], third_nible,
            chip8_obj_ptr->registers[third_nible], chip8_obj_ptr->I);
        break;
    default:
        break;
    }
}

void execute_instruction(chip8 *chip8_object_ptr){
    __uint8_t opcode1 = chip8_object_ptr->RAM[chip8_object_ptr->PC];
    __uint8_t opcode2 = chip8_object_ptr->RAM[chip8_object_ptr->PC + 1];
    
    __uint16_t ins = ((__uint16_t)opcode1 << 8 ) | opcode2;
    /*
    first_nible is the first 4 bits of the ins var
    which decide the category of the opcode
    */
    #ifdef DEBUG
    debug(chip8_object_ptr, ins);
    #endif

    switch(first_nible){
        case 0x0:
            if(second_nible == 0x0 && third_nible == 0xe && fourth_nible == 0x0){
                clear_screen(chip8_object_ptr);
            }else if(second_nible == 0x0 && third_nible == 0xe && fourth_nible == 0xe){
                return_from_subroutine(chip8_object_ptr);
                chip8_object_ptr->PC-=2;
            }else{
                printf("Unimplemented\n");
            }
            break;
        case 0x1:
            set_pc(chip8_object_ptr, ins);
            chip8_object_ptr->PC-=2;
            break;
        case 0x2:
            call_subroutine(chip8_object_ptr, ins);
            chip8_object_ptr->PC-=2;
            break;
        case 0x3:
            skip_constant_equal(chip8_object_ptr, ins);
            break;
        case 0x4:
            skip_not_constant_equal(chip8_object_ptr, ins);
            break;
        case 0x5:
            skip_register_equal(chip8_object_ptr, ins);
            break;
        case 0x6:
            set_register_value(chip8_object_ptr, ins); 
            break;
        case 0x7:
            add_register_value(chip8_object_ptr, ins);
            break;
        case 0x8:
            if(fourth_nible == 0x0){
                set_vx_vy(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x1){
                binary_or(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x2){
                binary_and(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x3){
                binary_xor(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x4){
                add(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x5){
                subtract_vx_vy(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x6){
                shift_right(chip8_object_ptr, ins);
            }else if(fourth_nible == 0x7){
                subtract_vy_vx(chip8_object_ptr, ins);
            }else if(fourth_nible == 0xe){
                shift_left(chip8_object_ptr, ins);
            }
            break;
        case 0x9:
            skip_register_not_equal(chip8_object_ptr, ins);
            break;
        case 0xA:
            set_i(chip8_object_ptr, ins);
            break;
        case 0xB:
            jump_with_offset(chip8_object_ptr, ins);
            chip8_object_ptr->PC-=2;
            break;
        case 0xC:
            random(chip8_object_ptr, ins);
            break;
        case 0xD:
            display_fun(chip8_object_ptr, ins);
            break;
        case 0xE:
            if((ins & 0x00ff) == 0x9e){
                skip_if_key(chip8_object_ptr, ins);
            }else{
                skip_if_not_key(chip8_object_ptr, ins);
            }
            break;
        case 0xF:
            if((ins & 0x00ff) == 0x55){
                store_memory(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x65){
                load_memory(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x1e){
                add_to_index(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x33){
                decimal_conversion(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x29){
                font_char(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x07){
                set_vx_delaytimer(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x15){
                set_delaytimer_vx(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x18){
                set_soundtimer_vx(chip8_object_ptr, ins);
            }else if((ins & 0x00ff) == 0x0A){
                get_key(chip8_object_ptr, ins);
            }
            break;
        default:
            printf("Unknown instuction ");
            printf("-> %hx\n", ins);
            break;
    }
    
    chip8_object_ptr->PC+=2;

}

int main(int argc, char **argv){
    
    chip8 chip8_object;
    chip8 *chip8_object_ptr = &chip8_object;

    SDL_Window *screen;
    SDL_Renderer *renderer;

    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    //Check if user provided ROM name
    if(argc != 2){
        printf("Usage: ./chip8 <rom name>\n");
        exit(1);
    }

    FILE *rom = fopen(argv[1], "r");

    //Check if everything went ok opening ROM
    if(rom == NULL){
        printf("Error opening ROM\n");
        exit(1);
    }

    initialize_chip8(chip8_object_ptr, rom);
       
    //SDL setup
    initialize_sdl(&screen, &renderer, &dev, &want, &have);
    
    srand(time(NULL));

    //Main loop
    while(!chip8_object_ptr->state){
        
        user_input(chip8_object_ptr);
        
        for(int i=0; i<700/60; i++)
            execute_instruction(chip8_object_ptr);
        
        SDL_Delay(16.6);
        
        decrement_delay_timer(chip8_object_ptr);
        decrement_sound_timer(chip8_object_ptr, &dev);

        draw(renderer, chip8_object_ptr);
    }

    //SDL Destroy
    destroy_sdl(screen, &dev);    
    
    return 0;
}