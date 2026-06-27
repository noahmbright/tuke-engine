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
* Main menu — shader background, clickable buttons (Story Mode, Free Play 1v1, Settings)
* Character select — 4 characters, portraits, names; practice managing cast data
* Story mode — animations, dialogue system; isolated environment to learn cutscene/narrative infra
* Settings — just enough to understand what a settings subsystem involves

Infrastructure needed (in order):
* Text rendering (stb_truetype already stubbed) — unlocks labels, dialogue, scores
* Mouse picking — screen-space rect hit test, no GPU work needed
* UI coordinate system decision — screen-space vs normalized before anything else

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


