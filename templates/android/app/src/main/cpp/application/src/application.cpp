#include <SDL_image.h>
#include <SDL_ttf.h>

#include <{{PROJECT_ID}}/application.hpp>
#include <{{PROJECT_ID}}/script_runtime.hpp>
#include <squared/application/application.hpp>
#include <squared/data/json.hpp>
#include <squared/graphics/color.hpp>
#include <squared/graphics/context.hpp>
#include <squared/graphics2d/orthographic_camera.hpp>
#include <squared/graphics2d/sprite_batch.hpp>
#include <squared/graphics2d/texture.hpp>
#include <squared/graphics2d/texture_atlas.hpp>
#include <squared/graphics2d/texture_region.hpp>
#include <squared/messaging/message_dispatcher.hpp>
#include <squared/messaging/telegram_provider.hpp>
#include <squared/time/timepiece.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <memory>
#include <utility>
#include <vector>

namespace {

constexpr int kLogicalWidth = 960;
constexpr int kLogicalHeight = 540;
constexpr float kStatusX = 24.0F;
constexpr float kStatusY = 24.0F;
constexpr float kStatusSize = 96.0F;
constexpr float kTimeIndicatorX = 56.0F;
constexpr float kTimeIndicatorY = 136.0F;
constexpr float kTimeIndicatorSize = 32.0F;
constexpr auto kTimeIndicatorPeriod = std::chrono::seconds(1);
constexpr float kProviderIndicatorX = 96.0F;
constexpr float kProviderIndicatorY = 144.0F;
constexpr float kProviderIndicatorSize = 16.0F;
constexpr float kSnapshotIndicatorX = 120.0F;
constexpr float kSnapshotIndicatorY = 144.0F;
constexpr float kSnapshotIndicatorSize = 16.0F;
constexpr float kReportX = 24.0F;
constexpr float kReportY = 184.0F;
constexpr float kReportWidth = 912.0F;
constexpr float kReportHeight = 196.0F;
constexpr float kDiagnosticX = 144.0F;
constexpr float kDiagnosticY = 24.0F;
constexpr float kDiagnosticWidth = 792.0F;
constexpr float kDiagnosticHeight = 96.0F;
constexpr std::string_view kDiagnosticAsset =
    "diagnostics/json-ttf-status.json";
constexpr std::string_view kDiagnosticLog =
    "/sdcard/Download/{{PROJECT_ID}}-diagnostics.log";
constexpr std::string_view kFallbackDiagnosticFont =
    "fonts/DejaVuSansMono.ttf";

enum class DiagnosticStage {
    AssetRead,
    JsonParse,
    JsonSchema,
    JsonWrite,
    JsonRoundTrip,
    TtfInitialize,
    FontOpen,
    TextRender,
    Ready
};

struct DiagnosticConfig {
    std::string message;
    std::string font;
    int point_size{30};
    squared::graphics::Color text_color =
        squared::graphics::Color::from_rgba8(184, 255, 208);
    squared::graphics::Color panel_color =
        squared::graphics::Color::from_rgba8(22, 40, 61);
};

struct DiagnosticResult {
    DiagnosticConfig config;
    DiagnosticStage stage{DiagnosticStage::AssetRead};
    std::string detail;
};

struct LogWriteResult {
    bool written{false};
    std::string detail;
};

LogWriteResult write_diagnostic_log(std::string_view report)
{
    std::error_code error;
    const std::filesystem::path directory("/sdcard/Download");
    std::filesystem::create_directories(directory, error);
    if (error) {
        return {
            false,
            "create directory: " + error.message()
        };
    }

    errno = 0;
    std::ofstream output(
        std::filesystem::path(std::string(kDiagnosticLog)),
        std::ios::binary | std::ios::trunc
    );
    if (!output) {
        return {
            false,
            errno == 0
                ? "open failed"
                : std::string(std::strerror(errno))
        };
    }
    output.write(
        report.data(),
        static_cast<std::streamsize>(report.size())
    );
    output.put('\n');
    output.flush();
    if (!output) {
        return {false, "write or flush failed"};
    }
    return {true, {}};
}

std::optional<std::string> read_asset(
    std::string_view path,
    std::size_t maximum_bytes
)
{
    const std::string owned_path(path);
    SDL_RWops* stream = SDL_RWFromFile(owned_path.c_str(), "rb");
    if (!stream) return std::nullopt;

    const Sint64 signed_size = SDL_RWsize(stream);
    if (signed_size < 0 ||
        static_cast<Uint64>(signed_size) >
            static_cast<Uint64>(maximum_bytes)) {
        SDL_RWclose(stream);
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(signed_size), '\0');
    const std::size_t read = content.empty()
        ? 0
        : SDL_RWread(stream, content.data(), 1, content.size());
    SDL_RWclose(stream);
    if (read != content.size()) return std::nullopt;
    return content;
}

int hex_nibble(char value) noexcept
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::optional<squared::graphics::Color> parse_color(
    std::string_view encoded
) noexcept
{
    if (encoded.size() != 7 && encoded.size() != 9) return std::nullopt;
    if (encoded.front() != '#') return std::nullopt;

    std::uint8_t components[4]{0, 0, 0, 255};
    const std::size_t component_count =
        encoded.size() == 9 ? 4 : 3;
    for (std::size_t index = 0; index < component_count; ++index) {
        const int high = hex_nibble(encoded[1 + index * 2]);
        const int low = hex_nibble(encoded[2 + index * 2]);
        if (high < 0 || low < 0) return std::nullopt;
        components[index] = static_cast<std::uint8_t>(
            high * 16 + low
        );
    }
    return squared::graphics::Color::from_rgba8(
        components[0],
        components[1],
        components[2],
        components[3]
    );
}

bool read_optional_color(
    const squared::data::JsonValue& root,
    std::string_view name,
    squared::graphics::Color& destination,
    std::string& detail
)
{
    const auto* value = root.find(name);
    if (!value) return true;
    const auto* encoded = value->string_if();
    if (!encoded) {
        detail = std::string(name) + " must be a string";
        return false;
    }
    const auto parsed = parse_color(*encoded);
    if (!parsed) {
        detail = std::string(name) +
            " must use #RRGGBB or #RRGGBBAA";
        return false;
    }
    destination = *parsed;
    return true;
}

bool read_diagnostic_config(
    const squared::data::JsonValue& root,
    DiagnosticConfig& destination,
    std::string& detail
)
{
    if (!root.object_if()) {
        detail = "diagnostic root must be an object";
        return false;
    }

    const auto* message = root.find("message");
    const auto* font = root.find("font");
    const auto* point_size = root.find("pointSize");
    if (!message || !message->string_if() ||
        message->string_if()->empty()) {
        detail = "message must be a non-empty string";
        return false;
    }
    if (!font || !font->string_if() || font->string_if()->empty()) {
        detail = "font must be a non-empty string";
        return false;
    }

    std::uint64_t size = 0;
    if (point_size) {
        if (const auto* unsigned_size =
                point_size->unsigned_integer_if()) {
            size = *unsigned_size;
        } else if (const auto* signed_size =
                       point_size->signed_integer_if();
                   signed_size && *signed_size >= 0) {
            size = static_cast<std::uint64_t>(*signed_size);
        } else {
            detail = "pointSize must be a positive integer";
            return false;
        }
    } else {
        size = static_cast<std::uint64_t>(destination.point_size);
    }
    if (size < 12 || size > 72) {
        detail = "pointSize must be between 12 and 72";
        return false;
    }

    destination.message = *message->string_if();
    destination.font = *font->string_if();
    destination.point_size = static_cast<int>(size);
    return read_optional_color(
               root,
               "textColor",
               destination.text_color,
               detail
           ) &&
        read_optional_color(
               root,
               "panelColor",
               destination.panel_color,
               detail
           );
}

DiagnosticResult load_diagnostic()
{
    DiagnosticResult result;
    const auto source = read_asset(kDiagnosticAsset, 16U * 1024U);
    if (!source) {
        result.detail = "asset could not be read";
        return result;
    }

    const auto parsed = squared::data::parse_json(*source);
    if (!parsed) {
        result.stage = DiagnosticStage::JsonParse;
        result.detail = parsed.error.message;
        return result;
    }

    result.stage = DiagnosticStage::JsonSchema;
    if (!read_diagnostic_config(
            parsed.value,
            result.config,
            result.detail
        )) {
        return result;
    }

    const auto serialized = squared::data::write_json(parsed.value);
    if (!serialized) {
        result.stage = DiagnosticStage::JsonWrite;
        result.detail = serialized.error.message;
        return result;
    }

    const auto round_trip =
        squared::data::parse_json(serialized.text);
    if (!round_trip) {
        result.stage = DiagnosticStage::JsonRoundTrip;
        result.detail = round_trip.error.message;
        return result;
    }

    DiagnosticConfig verified;
    if (!read_diagnostic_config(
            round_trip.value,
            verified,
            result.detail
        )) {
        result.stage = DiagnosticStage::JsonRoundTrip;
        return result;
    }
    const auto repeated =
        squared::data::write_json(round_trip.value);
    if (!repeated || repeated.text != serialized.text) {
        result.stage = DiagnosticStage::JsonRoundTrip;
        result.detail = "deterministic serialization changed";
        return result;
    }

    result.config = std::move(verified);
    result.stage = DiagnosticStage::Ready;
    result.detail = "strict JSON round-trip passed";
    return result;
}

squared::graphics::Color diagnostic_color(
    DiagnosticStage stage
) noexcept
{
    using squared::graphics::Color;
    switch (stage) {
    case DiagnosticStage::AssetRead:
        return Color::from_rgba8(255, 0, 255);
    case DiagnosticStage::JsonParse:
        return Color::from_rgba8(255, 40, 40);
    case DiagnosticStage::JsonSchema:
        return Color::from_rgba8(255, 128, 0);
    case DiagnosticStage::JsonWrite:
        return Color::from_rgba8(255, 220, 0);
    case DiagnosticStage::JsonRoundTrip:
        return Color::from_rgba8(180, 64, 255);
    case DiagnosticStage::TtfInitialize:
        return Color::from_rgba8(0, 220, 255);
    case DiagnosticStage::FontOpen:
        return Color::from_rgba8(0, 128, 255);
    case DiagnosticStage::TextRender:
        return Color::white();
    case DiagnosticStage::Ready:
    default:
        return Color::from_rgba8(40, 200, 100);
    }
}

SDL_Color to_sdl_color(squared::graphics::Color source) noexcept
{
    const auto safe = source.clamped();
    const auto component = [](float value) {
        return static_cast<Uint8>(value * 255.0F + 0.5F);
    };
    return {
        component(safe.red),
        component(safe.green),
        component(safe.blue),
        component(safe.alpha)
    };
}

bool upload_text_surface(
    squared::graphics2d::Texture& texture,
    SDL_Surface* rendered
)
{
    if (!rendered) return false;
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(
        rendered,
        SDL_PIXELFORMAT_ABGR8888,
        0
    );
    if (!rgba) return false;

    const std::size_t row_bytes =
        static_cast<std::size_t>(rgba->w) * 4U;
    bool created = false;
    if (static_cast<std::size_t>(rgba->pitch) == row_bytes) {
        created = texture.create_rgba(
            rgba->w,
            rgba->h,
            static_cast<const std::uint8_t*>(rgba->pixels)
        );
    } else {
        std::vector<std::uint8_t> pixels(
            row_bytes * static_cast<std::size_t>(rgba->h)
        );
        const auto* source =
            static_cast<const std::uint8_t*>(rgba->pixels);
        for (int row = 0; row < rgba->h; ++row) {
            std::memcpy(
                pixels.data() +
                    static_cast<std::size_t>(row) * row_bytes,
                source +
                    static_cast<std::size_t>(row) *
                        static_cast<std::size_t>(rgba->pitch),
                row_bytes
            );
        }
        created = texture.create_rgba(
            rgba->w,
            rgba->h,
            pixels.data()
        );
    }
    SDL_FreeSurface(rgba);
    return created;
}

bool contains_status(float x, float y) noexcept
{
    return x >= kStatusX &&
        x <= kStatusX + kStatusSize &&
        y >= kStatusY &&
        y <= kStatusY + kStatusSize;
}

const squared::messaging::MessageId& time_indicator_message()
{
    static const squared::messaging::MessageId value(
        "sample.time.indicator-toggle"
    );
    return value;
}

const squared::messaging::EndpointId& application_endpoint()
{
    static const squared::messaging::EndpointId value(
        "sample.application.main"
    );
    return value;
}

const squared::messaging::MessageId& provider_state_message()
{
    static const squared::messaging::MessageId value(
        "sample.state.provider-ready"
    );
    return value;
}

const squared::messaging::EndpointId& provider_endpoint()
{
    static const squared::messaging::EndpointId value(
        "sample.provider.application"
    );
    return value;
}

class SnapshotProbe final : public squared::messaging::Telegraph {
public:
    bool handle_message(
        const squared::messaging::Telegram& telegram
    ) override
    {
        const bool* payload = telegram.payload().boolean_if();
        delivered =
            telegram.message() ==
                squared::messaging::MessageId(
                    "sample.snapshot.probe"
                ) &&
            payload && *payload;
        return delivered;
    }

    bool delivered{false};
};

bool verify_pending_snapshot()
{
    using namespace std::chrono_literals;
    squared::time::ManualTimepiece time;
    const squared::messaging::MessageDispatcherConfig config{
        .pending_capacity = 2,
        .subscription_capacity = 2,
        .maximum_deliveries_per_update = 2
    };
    squared::messaging::MessageDispatcher source(time, config);
    const auto scheduled = source.schedule(
        250ms,
        squared::messaging::Telegram(
            squared::messaging::MessageId(
                "sample.snapshot.probe"
            ),
            squared::data::JsonValue(true),
            std::nullopt,
            squared::messaging::EndpointId(
                "sample.snapshot.receiver"
            )
        )
    );
    if (!scheduled) return false;

    const auto snapshot = source.snapshot_pending();
    if (!snapshot) return false;
    const auto encoded =
        squared::data::write_json(snapshot.snapshot);
    if (!encoded) return false;
    const auto decoded =
        squared::data::parse_json(encoded.text);
    if (!decoded) return false;

    squared::messaging::MessageDispatcher restored(time, config);
    SnapshotProbe probe;
    auto registration = restored.register_endpoint(
        squared::messaging::EndpointId(
            "sample.snapshot.receiver"
        ),
        probe
    );
    if (!registration ||
        !restored.restore_pending(decoded.value)) {
        return false;
    }
    time.advance(250ms);
    return restored.update() == 1 && probe.delivered;
}

std::string telegram_json(
    const squared::messaging::Telegram& telegram
)
{
    squared::data::JsonValue::Object message;
    message.emplace(
        "message",
        squared::data::JsonValue(telegram.message().value())
    );
    message.emplace("payload", telegram.payload());
    if (telegram.sender()) {
        message.emplace(
            "sender",
            squared::data::JsonValue(
                telegram.sender()->value()
            )
        );
    }
    if (telegram.receiver()) {
        message.emplace(
            "receiver",
            squared::data::JsonValue(
                telegram.receiver()->value()
            )
        );
    }
    const auto encoded = squared::data::write_json(
        squared::data::JsonValue(std::move(message))
    );
    return encoded ? encoded.text : "{\"error\":\"encode failed\"}";
}

}  // namespace

namespace {{PROJECT_ID}} {
namespace {

class GeneratedApplication final
    : public squared::application::Application,
      public squared::messaging::Telegraph,
      public squared::messaging::TelegramProvider {
public:
    GeneratedApplication() noexcept
        : camera_(
              static_cast<float>(kLogicalWidth),
              static_cast<float>(kLogicalHeight),
              squared::graphics2d::CoordinateOrigin::TopLeft
          ),
          scripts_(visual_, kLogicalWidth, kLogicalHeight),
          messages_(
              timepiece_,
              {
                  .pending_capacity = 32,
                  .subscription_capacity = 8,
                  .maximum_deliveries_per_update = 16
              }
          )
    {
    }

    [[nodiscard]] bool create(
        squared::graphics::Context&
    ) override
    {
        if (!batch_.initialize() ||
            !white_texture_.create_solid(
                squared::graphics::Color::white()
            )) {
            SDL_Log("Squared graphics2d initialization failed");
            dispose();
            return false;
        }
        white_region_ =
            squared::graphics2d::TextureRegion(white_texture_);

        const bool atlas_loaded =
            lifecycle_atlas_.load(
                "graphics/lifecycle-status.atlas"
            );
        blue_status_ =
            lifecycle_atlas_.find_region("lifecycle-status", 0);
        green_status_ =
            lifecycle_atlas_.find_region("lifecycle-status", 1);
        atlas_regions_ready_ =
            atlas_loaded && blue_status_ && green_status_;
        if (!atlas_regions_ready_) {
            SDL_Log(
                "Lifecycle atlas unavailable; "
                "using red diagnostic square"
            );
        }

        prepare_diagnostic();
        pending_snapshot_ready_ = verify_pending_snapshot();
        if (!pending_snapshot_ready_) {
            SDL_Log(
                "Pending Telegram JSON snapshot diagnostic failed"
            );
        }
        auto endpoint_result = messages_.register_endpoint(
            application_endpoint(),
            *this
        );
        if (!endpoint_result) {
            SDL_Log("Application message endpoint registration failed");
            dispose();
            return false;
        }
        message_endpoint_ = std::move(
            endpoint_result.subscription
        );
        auto provider_result = messages_.register_provider(
            provider_state_message(),
            provider_endpoint(),
            *this
        );
        if (!provider_result) {
            SDL_Log(
                "Application state provider registration failed: %s",
                provider_result.detail.c_str()
            );
            dispose();
            return false;
        }
        state_provider_ = std::move(
            provider_result.subscription
        );
        auto state_result = messages_.subscribe(
            provider_state_message(),
            *this
        );
        if (!state_result) {
            SDL_Log(
                "Application state subscription failed: %s",
                state_result.detail.c_str()
            );
            dispose();
            return false;
        }
        provider_state_subscription_ = std::move(
            state_result.subscription
        );
        if (!schedule_time_indicator(true)) {
            SDL_Log("Application-time Telegram could not be scheduled");
            dispose();
            return false;
        }
        if (!scripts_.start("lua/bootstrap.lua")) {
            SDL_Log("Application scripting bootstrap failed");
            dispose();
            return false;
        }
        refresh_diagnostic_report();
        return true;
    }

    void handle_event(
        const squared::application::Event& event
    ) override
    {
        scripts_.handle_event(event);
        if (event.type ==
                squared::application::Event::Type::PointerDown &&
            contains_status(event.x, event.y)) {
            lifecycle_marked_ = !lifecycle_marked_;
        }
        if (event.type ==
            squared::application::Event::Type::QuitRequested) {
            quit_requested_ = true;
        }
        if (event.type ==
            squared::application::Event::Type::Resume) {
            refresh_diagnostic_report();
        }
    }

    void update(std::chrono::nanoseconds delta) override
    {
        timepiece_.advance(delta);
        static_cast<void>(messages_.update());
        scripts_.update(
            std::chrono::duration<double>(delta).count()
        );
    }

    void render(squared::graphics::Context& graphics) override
    {
        graphics.clear(
            squared::graphics::Color::from_rgba8(
                visual_.background_red,
                visual_.background_green,
                visual_.background_blue
            )
        );
        if (!batch_.begin(camera_)) return;

        const auto& status_region = atlas_regions_ready_
            ? (
                lifecycle_marked_
                    ? green_status_->region()
                    : blue_status_->region()
            )
            : white_region_;
        batch_.draw(
            status_region,
            kStatusX,
            kStatusY,
            kStatusSize,
            kStatusSize,
            atlas_regions_ready_
                ? squared::graphics::Color::white()
                : squared::graphics::Color::from_rgba8(
                    255,
                    40,
                    40
                )
        );
        batch_.draw(
            white_region_,
            kTimeIndicatorX,
            kTimeIndicatorY,
            kTimeIndicatorSize,
            kTimeIndicatorSize,
            time_indicator_state_
                ? squared::graphics::Color::from_rgba8(
                    0,
                    230,
                    255
                )
                : squared::graphics::Color::from_rgba8(
                    255,
                    64,
                    220
                )
        );
        batch_.draw(
            white_region_,
            kProviderIndicatorX,
            kProviderIndicatorY,
            kProviderIndicatorSize,
            kProviderIndicatorSize,
            provider_state_received_
                ? squared::graphics::Color::from_rgba8(
                    32,
                    224,
                    96
                )
                : squared::graphics::Color::from_rgba8(
                    255,
                    48,
                    48
                )
        );
        batch_.draw(
            white_region_,
            kSnapshotIndicatorX,
            kSnapshotIndicatorY,
            kSnapshotIndicatorSize,
            kSnapshotIndicatorSize,
            pending_snapshot_ready_
                ? squared::graphics::Color::from_rgba8(
                    64,
                    224,
                    128
                )
                : squared::graphics::Color::from_rgba8(
                    255,
                    160,
                    32
                )
        );
        batch_.draw(
            white_region_,
            kDiagnosticX,
            kDiagnosticY,
            kDiagnosticWidth,
            kDiagnosticHeight,
            diagnostic_.stage == DiagnosticStage::Ready
                ? diagnostic_.config.panel_color
                : diagnostic_color(diagnostic_.stage)
        );
        if (diagnostic_.stage == DiagnosticStage::Ready &&
            diagnostic_region_.valid()) {
            const float available_width =
                kDiagnosticWidth - 32.0F;
            const float natural_width =
                static_cast<float>(diagnostic_region_.width());
            const float natural_height =
                static_cast<float>(diagnostic_region_.height());
            const float scale = natural_width > available_width
                ? available_width / natural_width
                : 1.0F;
            const float draw_width = natural_width * scale;
            const float draw_height = natural_height * scale;
            batch_.draw(
                diagnostic_region_,
                kDiagnosticX + 16.0F,
                kDiagnosticY +
                    (kDiagnosticHeight - draw_height) * 0.5F,
                draw_width,
                draw_height,
                squared::graphics::Color::white()
            );
        }
        batch_.draw(
            white_region_,
            static_cast<float>(visual_.tile_x),
            static_cast<float>(visual_.tile_y),
            static_cast<float>(visual_.tile_size),
            static_cast<float>(visual_.tile_size),
            squared::graphics::Color::from_rgba8(
                visual_.tile_red,
                visual_.tile_green,
                visual_.tile_blue
            )
        );
        batch_.draw(
            white_region_,
            kReportX,
            kReportY,
            kReportWidth,
            kReportHeight,
            squared::graphics::Color::from_rgba8(
                12,
                20,
                30,
                230
            )
        );
        if (report_region_.valid()) {
            const float available_width = kReportWidth - 24.0F;
            const float available_height = kReportHeight - 20.0F;
            const float natural_width =
                static_cast<float>(report_region_.width());
            const float natural_height =
                static_cast<float>(report_region_.height());
            const float horizontal_scale =
                natural_width > available_width
                    ? available_width / natural_width
                    : 1.0F;
            const float vertical_scale =
                natural_height > available_height
                    ? available_height / natural_height
                    : 1.0F;
            const float scale =
                std::min(horizontal_scale, vertical_scale);
            batch_.draw(
                report_region_,
                kReportX + 12.0F,
                kReportY + 10.0F,
                natural_width * scale,
                natural_height * scale,
                squared::graphics::Color::white()
            );
        }
        batch_.end();
    }

    void dispose() override
    {
        if (disposed_) return;
        disposed_ = true;
        scripts_.shutdown();
        lifecycle_atlas_.destroy();
        diagnostic_text_.destroy();
        report_text_.destroy();
        white_texture_.destroy();
        batch_.destroy();
        blue_status_ = nullptr;
        green_status_ = nullptr;
        atlas_regions_ready_ = false;
    }

    [[nodiscard]] bool quit_requested() const noexcept override
    {
        return quit_requested_ || scripts_.quit_requested();
    }

    bool handle_message(
        const squared::messaging::Telegram& telegram
    ) override
    {
        if (telegram.message() == provider_state_message()) {
            const bool* ready = telegram.payload().boolean_if();
            if (!ready) {
                provider_detail_ = "FAIL invalid boolean payload";
                message_dump_ = telegram_json(telegram);
                refresh_diagnostic_report();
                return false;
            }
            provider_state_received_ = *ready;
            provider_detail_ = *ready
                ? "PASS"
                : "FAIL provider returned false";
            message_dump_ = telegram_json(telegram);
            refresh_diagnostic_report();
            return true;
        }
        if (telegram.message() != time_indicator_message()) {
            return false;
        }
        const bool* state = telegram.payload().boolean_if();
        if (!state) return false;
        time_indicator_state_ = *state;
        if (!time_message_received_) {
            time_message_received_ = true;
            refresh_diagnostic_report();
        }
        if (!schedule_time_indicator(!*state)) {
            SDL_Log(
                "Application-time Telegram reschedule failed"
            );
        }
        return true;
    }

    squared::messaging::ProviderResult provide(
        const squared::messaging::MessageId& message
    ) override
    {
        if (message == provider_state_message()) {
            return squared::messaging::ProviderResult::provided(
                squared::data::JsonValue(true)
            );
        }
        return squared::messaging::ProviderResult::no_current_state();
    }

private:
    std::string build_diagnostic_report() const
    {
        std::ostringstream report;
        report
            << "ATLAS: "
            << (atlas_regions_ready_ ? "PASS" : "FAIL")
            << '\n'
            << "JSON_TTF: "
            << (
                diagnostic_.stage == DiagnosticStage::Ready
                    ? "PASS"
                    : "FAIL " + diagnostic_.detail
            )
            << '\n'
            << "PROVIDER: " << provider_detail_ << '\n'
            << "SNAPSHOT: "
            << (pending_snapshot_ready_ ? "PASS" : "FAIL")
            << '\n'
            << "TIME: "
            << (time_message_received_ ? "PASS" : "PENDING")
            << '\n'
            << "LOG: ";
        if (!log_attempted_) {
            report << "PENDING";
        } else if (log_write_ready_) {
            report << "PASS " << kDiagnosticLog;
        } else {
            report << "FAIL " << log_detail_;
        }
        report << '\n' << "MESSAGE: " << message_dump_;
        return report.str();
    }

    void refresh_diagnostic_report()
    {
        diagnostic_report_ = build_diagnostic_report();
        const LogWriteResult first_write =
            write_diagnostic_log(diagnostic_report_);
        log_attempted_ = true;
        log_write_ready_ = first_write.written;
        log_detail_ = first_write.detail;
        diagnostic_report_ = build_diagnostic_report();
        if (log_write_ready_) {
            const LogWriteResult final_write =
                write_diagnostic_log(diagnostic_report_);
            if (!final_write.written) {
                log_write_ready_ = false;
                log_detail_ = final_write.detail;
                diagnostic_report_ = build_diagnostic_report();
            }
        }

        const std::string font_path =
            diagnostic_.config.font.empty()
                ? std::string(kFallbackDiagnosticFont)
                : diagnostic_.config.font;
        TTF_Font* font = TTF_OpenFont(font_path.c_str(), 18);
        if (!font) {
            SDL_Log(
                "Diagnostic report font failed: %s",
                TTF_GetError()
            );
            return;
        }
        SDL_Surface* rendered = TTF_RenderUTF8_Blended_Wrapped(
            font,
            diagnostic_report_.c_str(),
            SDL_Color{200, 255, 218, 255},
            static_cast<Uint32>(kReportWidth - 24.0F)
        );
        if (!rendered ||
            !upload_text_surface(report_text_, rendered)) {
            SDL_Log(
                "Diagnostic report render failed: %s",
                TTF_GetError()
            );
        }
        if (rendered) SDL_FreeSurface(rendered);
        TTF_CloseFont(font);
        report_region_ =
            squared::graphics2d::TextureRegion(report_text_);
        SDL_Log("%s", diagnostic_report_.c_str());
    }

    [[nodiscard]] bool schedule_time_indicator(bool state)
    {
        const auto result = messages_.schedule(
            kTimeIndicatorPeriod,
            squared::messaging::Telegram(
                time_indicator_message(),
                squared::data::JsonValue(state),
                std::nullopt,
                application_endpoint()
            )
        );
        return static_cast<bool>(result);
    }

    void prepare_diagnostic()
    {
        diagnostic_ = load_diagnostic();
        if (diagnostic_.stage == DiagnosticStage::Ready &&
            TTF_WasInit() == 0) {
            diagnostic_.stage = DiagnosticStage::TtfInitialize;
            diagnostic_.detail = TTF_GetError();
        }
        if (diagnostic_.stage == DiagnosticStage::Ready) {
            TTF_Font* font = TTF_OpenFont(
                diagnostic_.config.font.c_str(),
                diagnostic_.config.point_size
            );
            if (!font) {
                diagnostic_.stage = DiagnosticStage::FontOpen;
                diagnostic_.detail = TTF_GetError();
            } else {
                SDL_Surface* rendered = TTF_RenderUTF8_Blended(
                    font,
                    diagnostic_.config.message.c_str(),
                    to_sdl_color(diagnostic_.config.text_color)
                );
                if (!rendered) {
                    diagnostic_.stage =
                        DiagnosticStage::TextRender;
                    diagnostic_.detail = TTF_GetError();
                } else {
                    if (!upload_text_surface(
                            diagnostic_text_,
                            rendered
                        )) {
                        diagnostic_.stage =
                            DiagnosticStage::TextRender;
                        diagnostic_.detail =
                            "text texture upload failed";
                    }
                    SDL_FreeSurface(rendered);
                }
                TTF_CloseFont(font);
            }
        }
        diagnostic_region_ =
            squared::graphics2d::TextureRegion(diagnostic_text_);
        SDL_Log(
            "JSON/TTF diagnostic stage %d: %s",
            static_cast<int>(diagnostic_.stage),
            diagnostic_.detail.c_str()
        );
    }

    squared::graphics2d::OrthographicCamera camera_;
    squared::graphics2d::SpriteBatch batch_;
    squared::graphics2d::Texture white_texture_;
    squared::graphics2d::Texture diagnostic_text_;
    squared::graphics2d::Texture report_text_;
    squared::graphics2d::TextureAtlas lifecycle_atlas_;
    squared::graphics2d::TextureRegion white_region_;
    squared::graphics2d::TextureRegion diagnostic_region_;
    squared::graphics2d::TextureRegion report_region_;
    const squared::graphics2d::AtlasRegion* blue_status_{nullptr};
    const squared::graphics2d::AtlasRegion* green_status_{nullptr};
    DiagnosticResult diagnostic_;
    VisualState visual_;
    ScriptRuntime scripts_;
    squared::time::Timepiece timepiece_;
    squared::messaging::MessageDispatcher messages_;
    squared::messaging::Subscription message_endpoint_;
    squared::messaging::Subscription state_provider_;
    squared::messaging::Subscription provider_state_subscription_;
    bool atlas_regions_ready_{false};
    bool lifecycle_marked_{false};
    bool time_indicator_state_{false};
    bool time_message_received_{false};
    bool provider_state_received_{false};
    bool pending_snapshot_ready_{false};
    bool log_attempted_{false};
    bool log_write_ready_{false};
    std::string provider_detail_{"PENDING"};
    std::string log_detail_;
    std::string message_dump_{
        "{\"message\":\"sample.state.provider-ready\","
        "\"status\":\"pending\"}"
    };
    std::string diagnostic_report_;
    bool quit_requested_{false};
    bool disposed_{false};
};

}  // namespace

std::unique_ptr<squared::application::Application>
create_application()
{
    return std::make_unique<GeneratedApplication>();
}

}  // namespace {{PROJECT_ID}}
