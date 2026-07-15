# Effekseer Extension for Defold

This Native Extension integrates the [Effekseer](https://effekseer.github.io/) particle system into the Defold game engine, allowing you to play complex 3D and 2D effects created in the Effekseer editor.

## Features

* Play effects in `.efkefc` format.
* Support for textures, materials, and models used in the effects.
* High-performance caching system: effects are loaded into video memory once and "fired" instantly.
* **Two ways to use:** via a ready-to-use smart component (`emitter.script`) or directly from code (C++ API).
* Control over position, rotation (via quaternions), scale, speed, and pause state.
* Integration with Defold's render pipeline (effects are drawn automatically via Post-Render callback).

## Installation

To use this extension in your project, add a link to this repository (as a zip archive) to the `Dependencies` section in your `game.project` file.

You also need to make sure that your resources folder (containing `.efkefc` files and textures) is added to the `Custom Resources` section in the `game.project` file (e.g., `/assets`) so that Defold includes these files in the build.

## Render Script Setup

Effekseer effects require updating and passing camera and projection matrices every frame. Add `effekseer` calls to your `.render_script` file:

```lua
function init(self)
    if effekseer then effekseer.init() end
end

function update(self, dt)
    if effekseer then effekseer.update(dt) end
    
    -- ... your render logic, render.draw() calls ...

    if effekseer then
        local view = self.state.cameras.camera_world.view
        local proj = self.state.cameras.camera_world.proj

        effekseer.set_camera_matrix(view)
        effekseer.set_projection_matrix(proj)
    end
end
```

---

## Method 1: Using the Component (The Defold Way)

This is the most convenient way to attach an effect to a game object (e.g., a rocket, a character). The effect will automatically follow the object, rotate, and scale with it.

### 1. Registering Paths
Add the paths to your effects in any global script (e.g., `main.script`) **before** calling the effects:
```lua
local effekseer_module = require("effekseer.effekseer")
effekseer_module.register(hash("sword"), "/assets/sword/sword.efkefc")
effekseer_module.register(hash("explosion"), "/assets/explosion.efkefc")
```

### 2. Component Setup
Add the `effekseer/emitter.script` component to any Game Object.
In the **Properties** window, configure the properties:
* `effect_id` (hash): Effect ID (e.g., `sword`).
* `play_on_init` (bool): Start the effect immediately when the object spawns.
* `track_transform` (bool): Track the object's position/rotation (disable for static effects for optimization).
* `scale` (number): Effect scale.
* `speed` (number): Playback speed (1.0 is normal).
* `loop` (bool): Loop playback.
* `paused` (bool): Freeze the effect on start.

### 3. Message Passing
You can control the effect by sending it messages (`msg.post`):
```lua
msg.post("#emitter", "play")
msg.post("#emitter", "stop")
msg.post("#emitter", "pause")
msg.post("#emitter", "resume")
msg.post("#emitter", "set_speed", { speed = 2.5 })
```

---

## Method 2: Using Directly from Code (C++ API)

If you just need to "spawn" an effect at a specific point in space without attaching it to a Game Object, use the direct API:

```lua
function init(self)
    -- 1. Specify the file path (or get it from your own config)
    local path = "/assets/explosion.efkefc"
    
    -- Load the binary from resources
    local data = sys.load_resource(path)
    
    -- Get the base directory for textures (in this case "/assets/")
    local base_path = string.match(path, "(.*/)")
    
    -- 2. CACHE THE EFFECT (returns the internal ID of the loaded effect)
    self.cached_id = effekseer.load_effect(data, 1.0, base_path)
    
    -- 3. PLAY AT ANY TIME (instantly from cache)
    self.instance_id = effekseer.play(self.cached_id, { loop = false, speed = 1.2 })
    
    -- 4. Move to the desired point
    effekseer.set_location(self.instance_id, 100, 200, 0)
    
    -- Example: rotation using quaternions
    local rot = vmath.quat_rotation_z(1.5)
    effekseer.set_rotation_quat(self.instance_id, rot.x, rot.y, rot.z, rot.w)
end
```

---

## C++ API Documentation (`effekseer` namespace)

* **`effekseer.init()`** — Initializes the context. Call this in your render script.
* **`effekseer.update(dt)`** — Updates particle logic. Call this every frame.
* **`effekseer.draw()`** — Explicitly draw the effects (useful if not using Post-Render callbacks).
* **`effekseer.clear()`** — Clear the internal state or graphics buffer.
* **`effekseer.load_effect(data, [scale], [basePath])`** — Parses and caches the binary and textures in video memory. Returns `loaded_effect_id`.
* **`effekseer.play(loaded_effect_id, [options])`** — Plays an effect from the cache. 
  * `options` (table): `{ loop = bool, speed = float, callback = function(id) end }`
  * Returns `instance_id` (a unique ID of the playing instance).
* **`effekseer.play_effect(...)`** — *(Deprecated)* Slow function without caching.
* **`effekseer.set_location(instance_id, x, y, z)`**
* **`effekseer.set_rotation_quat(instance_id, x, y, z, w)`** — Rotation via quaternions (solves Gimbal Lock).
* **`effekseer.set_scale(instance_id, sx, sy, sz)`**
* **`effekseer.set_speed(instance_id, speed)`** — Change the speed of a specific instance.
* **`effekseer.set_paused(instance_id, is_paused)`** — Pause / unpause (`true`/`false`).
* **`effekseer.stop_effect(instance_id)`** — Stops an effect instance.
* **`effekseer.stop_all_effects()`** — Stops all active effects on the screen.
* **`effekseer.set_camera_matrix(matrix4)`** / **`effekseer.set_projection_matrix(matrix4)`** — Render matrices.
* **`effekseer.get_playing_count()`** — Returns the number of currently active effect instances.
