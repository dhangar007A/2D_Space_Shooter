# Nebula Defender — Lab Presentation Guide

This guide is designed to help you explain your project during your lab viva or presentation. It breaks down the game into simple terms and connects the visual elements to the Computer Graphics (CG) concepts you learned in class.

---

## 1. Project Overview (The "Elevator Pitch")
**What to say:**
*"For my Computer Graphics project, I built 'Nebula Defender,' a 2D Space Shooter game using C++ and standard OpenGL. I chose not to use external game engines or heavy libraries like FreeGLUT. Instead, I used standard Windows APIs to create the window and pure OpenGL to render all the graphics. The game features multiple enemy waves, destructible shields, collision detection, and particle explosions, all built from scratch using core CG concepts."*

---

## 2. Core CG Concepts Explained (What they will ask you)

If the examiner asks: **"What computer graphics concepts did you use?"**, here is how to map the theory to your code:

### Concept 1: 2D Orthographic Projection (The Camera)
*   **Where it is in the game:** How the 2D world is mapped to the window.
*   **How you did it:** *"I used `gluOrtho2D(0, 800, 0, 700)` inside the OpenGL setup. This creates a 2D coordinate system where the bottom-left corner is (0,0) and the top-right is (800, 700). This makes it very easy to position objects using exact pixel coordinates rather than complex 3D math."*

### Concept 2: Geometric Transformations (`glTranslate`, `glRotate`, `glScale`)
*   **Where it is in the game:** Moving the ship, enemies, and animating them.
*   **How you did it:**
    *   **Translation:** *"Every time the player presses the arrow keys, I update their (X,Y) coordinates and use `glTranslatef(x, y, 0)` so OpenGL draws the ship at the new location."*
    *   **Rotation:** *"The purple 'Fighter' enemy uses `glRotatef` with a sine wave function to create a wobbly, flying animation. The yellow 'Commander' enemy uses rotation to spin the ring around its body."*
    *   **Scaling:** *"The green 'Scout' enemy uses `glScalef` with a sine wave to create a pulsing/breathing animation."*

### Concept 3: Primitive Drawing (The Shapes)
*   **Where it is in the game:** Everything you see!
*   **How you did it:** *"I didn't load any external image files (sprites). Everything is drawn using OpenGL primitives. For example, the bullets are `GL_QUADS`, the ship wings are `GL_TRIANGLES`, the shields are drawn using a mix of `GL_QUADS` (filled) and `GL_LINE_LOOP` (outline), and the explosion particles are `GL_POINTS`."*

### Concept 4: Color Interpolation (Gradients)
*   **Where it is in the game:** The smooth color transitions on the player's ship and the background sky.
*   **How you did it:** *"OpenGL automatically interpolates (blends) colors between vertices. For the ship's body, I assigned a bright cyan color (`glColor3f`) to the top vertex and a dark blue to the bottom vertices. OpenGL fills the interior with a smooth gradient."*

### Concept 5: Alpha Blending (Transparency & Glow)
*   **Where it is in the game:** The HUD/Score background, the transparent shields, and the neon glow effects.
*   **How you did it:**
    *   **Normal Transparency:** *"I used `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` to make the bottom score panel and the shields slightly see-through."*
    *   **Additive Blending (The Glow Effect):** *"To make the bullets and explosions look like glowing neon lights, I used additive blending: `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`. This adds the color values together, making overlapping areas brighter, just like real light."*

### Concept 6: Collision Detection (AABB)
*   **Where it is in the game:** Detecting when a bullet hits an enemy or the player.
*   **How you did it:** *"I wrote a custom 'Axis-Aligned Bounding Box' (AABB) collision function. It continuously checks if the rectangular area of a bullet overlaps with the rectangular area of an enemy. If they intersect, the bullet is destroyed, an explosion is triggered, and the score increases."*

---

## 3. The "Wow Factor" Features (Things to highlight)

If you want to impress the examiner, point out these features:

1.  **The Particle System:** *"When an enemy is destroyed, it triggers a particle system. It generates dozens of `GL_POINTS`, gives them a random angle and velocity, and applies 'gravity' to make them fall, while slowly reducing their Alpha value so they fade out over time."*
2.  **Parallax Background:** *"Notice the stars in the background. It's an array of points moving downwards. But they have different speeds and sizes based on a simulated 'depth' layer. Slower, smaller stars look further away, creating a 3D parallax effect in a 2D game."*
3.  **Zero Dependencies:** *"The whole executable is around 70KB because it doesn't rely on massive external libraries. It uses standard Windows libraries to create the context, making it extremely lightweight."*

---

## 4. Expected Questions & Quick Answers

*   **Q: Why is there no `main()` loop like `while(true)`?**
    *   *A: There actually is! Because I used the Windows API directly, the `main()` function contains a `while(true)` loop that uses `PeekMessage` to check for keyboard inputs without blocking, calculates the time difference (delta time), updates the game logic, and then calls `render()`.*
*   **Q: How do you manage the different screens (Menu vs Game)?**
    *   *A: I use a 'State Machine'. There is an `enum GameState { S_MENU, S_PLAY, S_OVER }`. In the render function, an `if` statement checks this variable and decides whether to draw the menu text or the actual gameplay.*
*   **Q: How did you render the text for the score?**
    *   *A: Rendering text in pure OpenGL is actually quite hard without libraries. I used a Windows-specific function called `wglUseFontBitmaps`. It takes a standard Windows font (like Courier New), converts the letters into OpenGL Display Lists, and then I use `glCallLists` to draw strings of text to the screen.*
