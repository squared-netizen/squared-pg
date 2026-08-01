#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>

#include <{{PROJECT_ID}}/application.hpp>
#include <squared/application/application.hpp>
#include <squared/application/event.hpp>
#include <squared/graphics/context.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>

namespace {

constexpr int kLogicalWidth = 960;
constexpr int kLogicalHeight = 540;

class PlatformLibraries final {
public:
    [[nodiscard]] bool initialize() noexcept
    {
        if (SDL_Init(
                SDL_INIT_VIDEO |
                SDL_INIT_AUDIO |
                SDL_INIT_GAMECONTROLLER
            ) != 0) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }
        sdl_ready_ = true;

        ttf_ready_ = TTF_Init() == 0;
        const int image_flags = IMG_INIT_PNG | IMG_INIT_JPG;
        image_ready_ =
            (IMG_Init(image_flags) & image_flags) == image_flags;
        const int mixer_flags = MIX_INIT_OGG | MIX_INIT_MP3;
        mixer_ready_ =
            (Mix_Init(mixer_flags) & mixer_flags) == mixer_flags;
        net_ready_ = SDLNet_Init() == 0;
        return true;
    }

    ~PlatformLibraries()
    {
        if (net_ready_) SDLNet_Quit();
        if (mixer_ready_) Mix_Quit();
        if (image_ready_) IMG_Quit();
        if (ttf_ready_) TTF_Quit();
        if (sdl_ready_) SDL_Quit();
    }

private:
    bool sdl_ready_{false};
    bool ttf_ready_{false};
    bool image_ready_{false};
    bool mixer_ready_{false};
    bool net_ready_{false};
};

std::optional<squared::application::Event> translate_event(
    const SDL_Event& event
) noexcept
{
    using Event = squared::application::Event;
    switch (event.type) {
    case SDL_QUIT:
        return Event{.type = Event::Type::QuitRequested};
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
        return Event{
            .type =
                event.type == SDL_FINGERDOWN
                    ? Event::Type::PointerDown
                    : (
                        event.type == SDL_FINGERMOTION
                            ? Event::Type::PointerMove
                            : Event::Type::PointerUp
                    ),
            .pointer_id =
                static_cast<std::int64_t>(event.tfinger.fingerId),
            .x = event.tfinger.x * kLogicalWidth,
            .y = event.tfinger.y * kLogicalHeight
        };
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        if (event.button.which == SDL_TOUCH_MOUSEID) {
            return std::nullopt;
        }
        SDL_Window* window =
            SDL_GetWindowFromID(event.button.windowID);
        int width = 0;
        int height = 0;
        if (window) SDL_GetWindowSize(window, &width, &height);
        if (width <= 0 || height <= 0) return std::nullopt;
        return Event{
            .type =
                event.type == SDL_MOUSEBUTTONDOWN
                    ? Event::Type::PointerDown
                    : Event::Type::PointerUp,
            .pointer_id =
                static_cast<std::int64_t>(event.button.which),
            .x =
                static_cast<float>(event.button.x) *
                static_cast<float>(kLogicalWidth) /
                static_cast<float>(width),
            .y =
                static_cast<float>(event.button.y) *
                static_cast<float>(kLogicalHeight) /
                static_cast<float>(height)
        };
    }
    case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_AC_BACK) {
            return Event{.type = Event::Type::BackRequested};
        }
        return std::nullopt;
    case SDL_APP_DIDENTERBACKGROUND:
        return Event{.type = Event::Type::Pause};
    case SDL_APP_DIDENTERFOREGROUND:
        return Event{.type = Event::Type::Resume};
    case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            event.window.event == SDL_WINDOWEVENT_RESIZED) {
            return Event{
                .type = Event::Type::Resize,
                .width = event.window.data1,
                .height = event.window.data2
            };
        }
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

}  // namespace

int main(int, char**)
{
    PlatformLibraries libraries;
    if (!libraries.initialize()) return 1;

    squared::graphics::Context graphics;
    if (!graphics.create(
            "{{PROJECT_TITLE}}",
            kLogicalWidth,
            kLogicalHeight
        )) {
        SDL_Log("OpenGL ES graphics setup failed");
        return 1;
    }

    auto application = {{PROJECT_ID}}::create_application();
    if (!application || !application->create(graphics)) {
        SDL_Log("Developer application initialization failed");
        graphics.destroy();
        return 1;
    }

    application->surface_created(graphics);
    application->resize(
        graphics.pixel_width(),
        graphics.pixel_height()
    );

    bool running = true;
    bool paused = false;
    Uint64 previous_counter = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();

    while (running && !application->quit_requested()) {
        SDL_Event native_event{};
        while (SDL_PollEvent(&native_event)) {
            const auto event = translate_event(native_event);
            if (!event) continue;

            switch (event->type) {
            case squared::application::Event::Type::QuitRequested:
                running = false;
                break;
            case squared::application::Event::Type::Pause:
                if (!paused) {
                    paused = true;
                    application->pause();
                }
                break;
            case squared::application::Event::Type::Resume:
                if (paused) {
                    paused = false;
                    graphics.refresh_viewport();
                    application->resume();
                    application->resize(
                        graphics.pixel_width(),
                        graphics.pixel_height()
                    );
                    previous_counter = SDL_GetPerformanceCounter();
                }
                break;
            case squared::application::Event::Type::Resize:
                graphics.refresh_viewport();
                application->resize(
                    graphics.pixel_width(),
                    graphics.pixel_height()
                );
                break;
            default:
                break;
            }
            application->handle_event(*event);
        }

        if (paused) {
            SDL_Delay(50);
            continue;
        }

        const Uint64 current_counter = SDL_GetPerformanceCounter();
        const double raw_seconds =
            static_cast<double>(current_counter - previous_counter) /
            static_cast<double>(frequency);
        previous_counter = current_counter;
        const auto delta = std::chrono::duration_cast<
            std::chrono::nanoseconds
        >(
            std::chrono::duration<double>(
                std::min(raw_seconds, 0.1)
            )
        );

        application->update(delta);
        application->render(graphics);
        graphics.present();
    }

    application->surface_destroyed();
    application->dispose();
    application.reset();
    graphics.destroy();
    return 0;
}
