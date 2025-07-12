# Pong

Skill checks:
* AABB collision detection
* How to batch draw calls - one pipeline for ball and both paddles?
* Vulkan renders upside down
* Game state - pause screen, main menu, game, character select
* Animation - shaders and hand drawn
* Use cubes for paddles and have two camera angles
* Textured cubes
* Cubemaps/skybox
* Lights falling on paddles
* Text rendering for menus and scores

Expandable scope – adding effects (particles, trails, distortions, bloom) tests:
* Dynamic uniform updates
* Push constants
* Descriptor updates
* Multi-pass rendering
* Sprite batching or instancing

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
