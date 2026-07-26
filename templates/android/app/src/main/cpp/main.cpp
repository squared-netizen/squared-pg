#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>

#include <{{PROJECT_ID}}/script_runtime.hpp>

#include <algorithm>

namespace {

constexpr int kLogicalWidth = 960;
constexpr int kLogicalHeight = 540;

}  // namespace

int main(int, char**)
{
    if (SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_AUDIO |
            SDL_INIT_GAMECONTROLLER
        ) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const bool ttf_ready = TTF_Init() == 0;
    const int image_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    const bool image_ready =
        (IMG_Init(image_flags) & image_flags) == image_flags;
    const int mixer_flags = MIX_INIT_OGG | MIX_INIT_MP3;
    const bool mixer_ready =
        (Mix_Init(mixer_flags) & mixer_flags) == mixer_flags;
    const bool net_ready = SDLNet_Init() == 0;

    SDL_Window* window = SDL_CreateWindow(
        "{{PROJECT_TITLE}}",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kLogicalWidth,
        kLogicalHeight,
        SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = window
        ? SDL_CreateRenderer(
              window,
              -1,
              SDL_RENDERER_ACCELERATED |
                  SDL_RENDERER_PRESENTVSYNC
          )
        : nullptr;

    if (!window || !renderer) {
        SDL_Log("Renderer setup failed: %s", SDL_GetError());
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        if (net_ready) SDLNet_Quit();
        if (mixer_ready) Mix_Quit();
        if (image_ready) IMG_Quit();
        if (ttf_ready) TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(renderer, kLogicalWidth, kLogicalHeight);

    {{PROJECT_ID}}::VisualState visual;
    {{PROJECT_ID}}::ScriptRuntime scripts(
        visual,
        kLogicalWidth,
        kLogicalHeight
    );
    if (!scripts.start("lua/bootstrap.lua")) {
        SDL_Log("Application scripting bootstrap failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (net_ready) SDLNet_Quit();
        if (mixer_ready) Mix_Quit();
        if (image_ready) IMG_Quit();
        if (ttf_ready) TTF_Quit();
        SDL_Quit();
        return 1;
    }

    bool running = true;
    bool backgrounded = false;
    Uint64 previous_counter = SDL_GetPerformanceCounter();

    while (running && !scripts.quit_requested()) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            scripts.handle_event(event);

            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                backgrounded = true;
                break;
            case SDL_APP_WILLENTERFOREGROUND:
            case SDL_APP_DIDENTERFOREGROUND:
                backgrounded = false;
                previous_counter = SDL_GetPerformanceCounter();
                break;
            default:
                break;
            }
        }

        if (backgrounded) {
            SDL_Delay(50);
            continue;
        }

        const Uint64 current_counter = SDL_GetPerformanceCounter();
        const double delta_seconds = std::min(
            static_cast<double>(
                current_counter - previous_counter
            ) /
                static_cast<double>(SDL_GetPerformanceFrequency()),
            0.1
        );
        previous_counter = current_counter;
        scripts.update(delta_seconds);

        SDL_SetRenderDrawColor(
            renderer,
            visual.background_red,
            visual.background_green,
            visual.background_blue,
            255
        );
        SDL_RenderClear(renderer);

        SDL_Rect tile{
            visual.tile_x,
            visual.tile_y,
            visual.tile_size,
            visual.tile_size
        };
        SDL_SetRenderDrawColor(
            renderer,
            visual.tile_red,
            visual.tile_green,
            visual.tile_blue,
            255
        );
        SDL_RenderFillRect(renderer, &tile);
        SDL_RenderPresent(renderer);
    }

    scripts.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (net_ready) SDLNet_Quit();
    if (mixer_ready) Mix_Quit();
    if (image_ready) IMG_Quit();
    if (ttf_ready) TTF_Quit();
    SDL_Quit();
    return 0;
}
