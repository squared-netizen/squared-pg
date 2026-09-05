# Termux projects

`kit.termux` bridges a terminal application to **Termux:API** — the Android
facilities Termux exposes: battery, clipboard, notifications, dialogs, speech,
location, sensors.

```sh
sqpg new phone --template template.terminal.cpp --kit kit.terminal --kit kit.termux
cd phone && make && ./build/phone
```

```cpp
#include <squared/kit/termux.hpp>

if (auto battery = sq::termux::battery()) {
    sq::term::println(battery->percentage, "% ", battery->status);
}
(void)sq::termux::notify("Build finished", "3 warnings", "build");
if (auto name = sq::termux::dialog_text("Your name?")) {
    (void)sq::termux::toast("hello " + *name);
}
```

## How it works, and what that costs

Termux:API is **not a library**. It is a set of command-line programs that talk
to a companion Android app over a socket and print JSON. There is no C API to
link against, so this kit runs those programs and parses their output.

That is worth knowing rather than discovering:

- **Every call forks a process.** Milliseconds, not microseconds. Fine for a
  notification; wrong inside a loop that has to stay responsive.
- **`location()` and `dialog_*()` block.** GPS can take many seconds outdoors
  and never resolve indoors. A dialog waits for a human.
- **Arguments are shell-quoted for you.** Pass them as separate strings. Never
  build a command line yourself — text from a clipboard or a dialog is exactly
  the text most likely to contain a quote.

## Two things must be installed

```sh
pkg install termux-api        # the command-line programs
```

**and** the **Termux:API app** from F-Droid. The package alone does nothing —
it is a client for the app. This trips up nearly everyone once.

```cpp
sq::termux::available();   // are the programs installed?
sq::termux::probe();       // ...and does the app answer?
```

`probe()` costs a real round trip; call it once at startup. It reports the two
failures separately, because the fixes are different.

```sh
make termux-status
```

## Surface

| | |
|---|---|
| `available()`, `probe()` | is the API usable |
| `run(command, args)` | any termux-api command; returns `Result` |
| `battery()` | percentage, status, health, temperature |
| `clipboard_get()`, `clipboard_set()` | |
| `toast(text, position)` | brief on-screen message |
| `notify(title, content, id)`, `notification_remove(id)` | reusing an `id` replaces rather than stacks |
| `vibrate(ms, force)` | `force` overrides silent mode |
| `dialog_text(title, hint)`, `dialog_choice(title, options)` | empty optional when cancelled |
| `speak(text)` | text to speech |
| `location(provider)` | `network`, `gps` or `passive` |
| `share(path)`, `open(path_or_url)` | Android share sheet, system handler |
| `torch(on)` | |
| `sensor(name, count)`, `wifi_info()`, `telephony_info()` | raw JSON |

Anything not listed is one `run()` call away:

```cpp
auto result = sq::termux::run("termux-camera-info");
if (auto json = result.json()) { /* ... */ }
```

## The JSON type

`sq::termux::Json` is a small complete parser, included because termux-api
output is the only reason this kit exists and the alternative is everyone
writing the same thing badly.

```cpp
auto json = sq::termux::run("termux-sensor", {"-s", "accelerometer", "-n", "1"}).json();
if (json) {
    double x = (*json)["BMI160 Accelerometer"]["values"][0].as_number().value_or(0.0);
}
```

Two properties worth relying on:

**Chaining through a missing key is safe.** `root["a"]["b"][7]` on absent `a`
yields a null `Json`, not a crash. Check with `.is_null()` or use
`.as_*()`, which return `std::optional`.

**Accessors never coerce.** `as_number()` on `"5"` is empty; `as_string()` on
`5` is empty. A wrong assumption about a field's type shows up as an empty
optional rather than a plausible wrong value.

`string_or`, `number_or`, `int_or` and `bool_or` take a fallback for the common
one-field read.

Malformed input is rejected outright rather than half-parsed. The parser is
exercised by `tools/smoke.sh` against real termux-api response shapes,
including escapes, unicode, nulls, negative numbers and nested arrays.

## Platform

`kit.termux` declares `platforms: ["termux"]`, so `--platform linux` will
refuse it — correctly, since the commands do not exist there. The kit compiles
anywhere; it simply reports the API as unavailable at runtime. That is
deliberate: a project using it should build on a desktop for development even
though it can only *work* on a phone.
