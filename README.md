# ESP8266 firmware skeleton

A reusable base for ESP8266 projects: WiFi provisioning with a captive portal,
persistent configuration, a web server serving gzipped assets from flash, OTA
updates, and boot-loop recovery. Built and tested on an ESP-01S (1 MB flash),
but nothing in the skeleton touches board-specific hardware.

Flash it once over serial. Everything after that goes over the air.

## Getting started

```
pio run -e serial -t upload     # first flash
pio run -t upload               # every time after (OTA is the default env)
pio run -e serial -t monitor    # serial console
```

On first boot the device has no credentials, so it starts an access point
called `ESP-Setup-XXXXXX`. Join it from a phone; the captive portal should open
by itself. Pick a network, enter the password, and the device switches over and
is then reachable at `http://esp01s.local` (or whatever hostname you set).

## Layout

```
platformio.ini
tools/gen_web_assets.py     build-time gzip + PROGMEM header generator
web/index.html              the UI, authored as a normal file
include/generated/          build output, gitignored
src/
  main.cpp  App.{h,cpp}  Config.h
  core/     AppState.h  Health.{h,cpp}  ITelemetry.h  Log.{h,cpp}
            FixedString.h  Callback.h  FlashSector.{h,cpp}
  config/   ConfigStore.{h,cpp}
  net/      NetworkManager.{h,cpp}  CaptivePortal.{h,cpp}
  web/      IRouteProvider.h  HttpService.{h,cpp}  JsonWriter.h
            ResponseBuffer.{h,cpp}  AssetHandler.{h,cpp}  ConfigRoutes.{h,cpp}
  ota/      OtaService.{h,cpp}
  app/      LedController.{h,cpp}  LedRoutes.{h,cpp}   <-- example, deletable
```

`App` is the only object with static storage duration. Everything else is a
member of it, constructed in a defined order and wired together in `begin()`.
No module reaches sideways to another; dependencies are passed by reference.

Every module has `begin()` and `tick()`. `tick()` must return promptly —
nothing in this codebase calls `delay()` in normal operation.

Nothing in this codebase heap-allocates during normal operation either.
`core/FixedString.h` (fixed-capacity string, rejects rather than truncates
on overflow) and `core/Callback.h` (instance pointer + function-pointer
thunk) stand in for `String` and `std::function` wherever state is held
long-term. `pio run -e heapcheck -t upload` builds with `-g` and a trap flag
(`APP_TRAP_HEAP_ALLOC`) reserved for verifying that at runtime — the trap
itself isn't wired into any allocator yet, so it currently just builds a
debug image.

## Building on it

`src/app` is an example: an LED you can toggle from the web UI. Delete the
directory, the two members in `App.h`, and the `addProvider(_ledRoutes)` line,
and you have a bare skeleton.

**Adding routes.** Implement `IRouteProvider`, register it in `App::begin()`
with `_http.addProvider()`. Build responses with `JsonWriter` over the shared
`ResponseBuffer`; the server is single-threaded so one buffer is safe.

```cpp
class MyRoutes : public IRouteProvider {
public:
  void registerRoutes(HttpServer& server) override;
};
```

**Reacting to state.** Implement `ITelemetry` and call `app.setTelemetry(&mine)`
before `app.begin()`. You get network state, mode changes, and OTA progress.
This is where an MQTT client or a status display would hook in.

**Storing configuration.** `ConfigStore` reads and writes one packed struct
(magic + schema version + fields + CRC16) directly against the SDK's
reserved EEPROM flash sector (`core/FlashSector.h`, no filesystem, no heap)
and already holds WiFi credentials, a hostname and display name, and MQTT
broker settings. It's a single sector with no wear-levelling or write
atomicity — a power cut mid-save can lose the whole record, not just the
field being changed. Bump `SCHEMA_VERSION` and fill in `migrate()` when you
change the layout.

**The UI belongs to your application.** `web/index.html` is yours to replace.
The build script gzips whatever is in `web/` into a PROGMEM table and fails the
build if the total exceeds `custom_web_max_bytes`.

**Forcing setup mode.** Call `app.requestProvisioningOnBoot()` before
`app.begin()`. Wiring that to a button is application-specific:

```cpp
void setup() {
  pinMode(3, INPUT_PULLUP);
  if (digitalRead(3) == LOW) {
    app.requestProvisioningOnBoot();
  }
  app.begin();
}
```

## HTTP API

| Method | Path | Notes |
|---|---|---|
| GET | `/` | the UI, gzipped, with ETag |
| GET | `/api/info` | version, state, heap, boot count, OTA headroom |
| GET | `/api/log` | recent log lines from the ring buffer |
| GET | `/api/wifi/status` | connection state |
| GET | `/api/wifi/scan` | `202` while scanning, `200` with results, `409` if busy |
| POST | `/api/wifi/config` | `ssid`, `password` |
| POST | `/api/wifi/forget` | clears credentials, returns to setup mode |
| GET/POST | `/api/settings` | `hostname`, `friendlyName` |
| GET/POST | `/api/broker` | MQTT host, port, username, password, base topic |
| POST | `/api/restart` | `clear=1` also resets the boot counter |
| GET/POST | `/api/led` | example application route |

Mutating requests respond *before* acting when the action would break the
connection they arrived on.

## Recovery

The ESP8266 has no OTA rollback. If a bad image boots and crashes, nothing
automatically reverts it. Two mechanisms stand in for that:

**Safe mode.** A counter in RTC user memory increments on every boot and clears
after 30 seconds of uptime. Past five consecutive unstable boots the device
comes up in safe mode: the network, the config routes and OTA still run, but
application routes are not mounted. Push a working image, then
`POST /api/restart` with `clear=1`.

RTC memory survives a reset or a crash but not a power cut, so someone
unplugging the device a few times is not mistaken for a boot loop.

**Serial.** GPIO0 low at power-on, flash with `-e serial`. Always available,
and the reason to keep an adapter within reach when changing the network or
update code.

## Flash budget

This matters more than PlatformIO's percentage suggests. OTA works by writing
the incoming image into the space after the running one, so:

```
usable ceiling  =  (sketch region - current image - 4 KB) ...
                   which in practice caps the image at about half the region
```

On this 1 MB board (no filesystem partition — see Storing configuration
above) that is roughly **500 KB**, not the ~1000 KB PlatformIO reports.
Cross it and OTA stops working permanently, with a serial adapter as the
only way back.

Watch `freeSketch` in `/api/info`. When it drops below your image size, the
next update will not fit.

## Known limitations

- **No TLS.** BearSSL costs more flash and heap than this hardware can spare.
  Credentials cross the wire in plaintext, and the setup AP is open. Fine on a
  trusted LAN; not fine on a hostile one.
- **No authentication.** Every endpoint is open to anyone who can reach the
  device. OTA has no password by default either — set `OTA_PASSWORD` (see
  Configuration below) if the device isn't on a trusted network. The HTTP
  API has no auth of its own regardless.
- **Credentials are stored in plaintext.** There is no secure element; anyone
  with physical access and a serial adapter can read them.
- **mDNS is required** for the `.local` address the setup flow points people at.

## Configuration

Timings and defaults live in `src/Config.h`: connect timeout, AP fallback
delay, scan cache lifetime, safe-mode threshold, WiFi TX power, OTA start
delay, OTA hostname and password.

**OTA password.** Unset by default — ArduinoOTA accepts updates without a
challenge. Set the `OTA_PASSWORD` environment variable before building to
require one; it's picked up by both the firmware (`Config::OTA_PASSWORD`)
and the `espota` upload tool (`[env:ota]`'s `--auth`) from the same variable,
so they can't drift out of sync:

```
export OTA_PASSWORD=yourpassword
pio run -t upload
```

Changing this only takes effect on the *next* build — if the device is
currently running a build with a password set, that upload still needs the
old password to get the new image on, even if the new image itself won't
require one afterwards.
