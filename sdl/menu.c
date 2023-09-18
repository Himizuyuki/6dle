#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_video.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

int main(){

    // Init SDL
    if(SDL_Init(SDL_INIT_VIDEO) != 0)
        errx(EXIT_FAILURE, "%p", SDL_GetError);


    // Initialize SDL_Image
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Create a window
    SDL_Window* window = SDL_CreateWindow(
        "6DLE",
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        640, 
        400, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    ); 

    // Create a renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );  

    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
    
        if(event.type == SDL_QUIT)
            break;
    }


    // Clean up
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
