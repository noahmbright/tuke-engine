# Pong

Skill checks:
* AABB collision detection
* How to batch draw calls - one pipeline for ball and both paddles?
* Animation - shaders and hand drawn
* Use cubes for paddles and have two camera angles
* Textured cubes
* Cubemaps/skybox
* Lights falling on paddles
* Text rendering for menus and scores
* Render queue

Expandable scope – adding effects (particles, trails, distortions, bloom) tests:
* Dynamic uniform updates
* Push constants
* Descriptor updates
* Multi-pass rendering
* Sprite batching or instancing

## Main menu

Shader art already running as fullscreen quad background. Layer UI on top in same render pass, depth testing disabled for UI pipeline.

Screens:
* Main menu — shader background, buttons: Story Mode, Free Play 1v1, Settings
* Free play — character select screen (4 characters, portraits, names), then straight into game
* Story mode — character select, then cutscene intro, then game
* Settings — enough to understand what a settings subsystem involves

Infrastructure needed (in order):
* UI coordinate system — screen-space pixels or normalized [0,1]; decide before anything else
* Text rendering (stb_truetype already stubbed) — unlocks button labels, dialogue, scores
* Mouse picking — screen-space rect hit test, no GPU work needed

## Character system

4 characters. Each has:
* Name, portrait image
* Unique paddle shader / color / effect
* Possibly a signature skill (ties into combat/HUD charge bar)

Serialization: characters defined in data files (JSON or simple hand-rolled format), loaded at startup.
Player profiles: save file per player — selected character, win/loss record, unlocks.
Figure out a minimal save format (binary struct dump is fine to start, upgrade later).

## Story mode

Visual-novel style — no 3D cutscenes, just 2D panels, portraits, and dialogue.

Cutscene format:
* Sequence of frames: background image + one or two character portraits + dialogue text
* Advance on keypress or click
* Cutscene data is a flat array of frames, authored in a data file

Serialization: story script as a simple text or JSON file. Each entry is scene + speaker + line.
State tracking: which chapter the player is on, what choices were made (if any).

Questions to figure out:
* How to handle branching (if at all) — linear is fine for v1
* How to animate portraits (idle breathing, reaction poses) — flipbook or shader wobble
* Transition effects between cutscene frames

## Free play

Character select → 1v1 match → score screen → rematch or back to menu.
Eventually: local multiplayer on same keyboard, then networked.

## Gameplay background shader

Noise-based fragment shader running behind the game field, purely aesthetic.
Instanced fullscreen quad like the main menu but rendered first, then game geometry on top.
Low stakes — good place to play with fbm or domain-warped noise.

## Combat / HUD

* Health bars or skill charge bars rendered as screen-space quads
* Could be pure geometry (colored rects) before committing to text rendering
* Paddle hit → charge meter fills → skill activates; gives gameplay loop a skill expression layer

## Particles

💡 Funny / over the top effects:
* Ball trails: use instanced quads with alpha fade
* Explosion on paddle hit: full-screen shader effect or particle burst
* Screen shake: modify your camera matrix on impact frames
* Bloom / CRT effect: first implementation of a postprocessing pipeline
* Ridiculous scaling: have the ball or paddle grow / squash with sinusoidal oscillations using dynamic uniforms
*💡 Sound effect integration: if you have any audio system or want to integrate one (optional but rewarding).

I will want to use it as a test bed for a particle system and having simple
game state - a pause menu, a main menu, a character select screen. I want to
have scores with text rendering. I want to have animated backgrounds, either
through shader art or hand drawn animation that I texture onto a background
quad, and I want to have powerups appear that the ball can collect, and cause
things like the next paddle to hit it to explode

Particles
* Design a GPU-friendly particle system using instanced quads
* Store per-particle data in a dynamic vertex buffer updated each frame
* Sort if implementing transparent particles later

🏆 4. Stretch goals for maximum engine validation
* Split screen mode with two independent games running side by side
* Replay system – serialize game state and input sequence, replay deterministically
* Advanced shader effects
* Shockwave distortions
* Chromatic aberration
* Procedural background (raymarching or noise-based fragment shader)
* Basic audio integration – lightweight library like miniaudio to test sound pipeline


