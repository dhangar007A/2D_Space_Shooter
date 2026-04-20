/*
 * ============================================================
 *  NEBULA DEFENDER — 2D Space Shooter Game
 *  Computer Graphics Project using OpenGL
 * ============================================================
 *  Graphics Concepts Demonstrated:
 *   1. 2D Orthographic Projection  (gluOrtho2D)
 *   2. 2D Transformations          (glTranslatef, glRotatef, glScalef)
 *   3. Primitive Rendering         (GL_TRIANGLES, GL_QUADS, GL_POINTS,
 *                                   GL_LINES, GL_TRIANGLE_FAN, GL_LINE_LOOP)
 *   4. Per-vertex Color Gradients  (glColor3f per vertex)
 *   5. Alpha Blending              (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
 *   6. Additive Blending / Glow    (GL_SRC_ALPHA, GL_ONE)
 *   7. Particle System             (position, velocity, alpha-fade)
 *   8. Scrolling Parallax Starfield
 *   9. Real-time Animation Loop    (Windows timer / PeekMessage)
 *  10. 2D AABB Collision Detection
 *  11. Game State Machine          (Menu / Playing / Game-Over)
 *  12. Bitmap Text Rendering       (wglUseFontBitmaps / glCallLists)
 * ============================================================
 *  BUILD (no external dependencies — only WinAPI + OpenGL):
 *    g++ main.cpp -o nebula_defender.exe -lopengl32 -lglu32 -lgdi32 -mwindows
 *  Or (with console window for debug):
 *    g++ main.cpp -o nebula_defender.exe -lopengl32 -lglu32 -lgdi32
 * ============================================================
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

// ─── Global Window / RC ──────────────────────────────────────────────────────
HWND  g_hwnd  = NULL;
HDC   g_hdc   = NULL;
HGLRC g_hrc   = NULL;

// ─── Window & World Constants ────────────────────────────────────────────────
const int   WIN_W   = 800;
const int   WIN_H   = 700;
const float WORLD_W = 800.0f;
const float WORLD_H = 700.0f;

// ─── Math Helpers ────────────────────────────────────────────────────────────
#define PI 3.14159265f
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float randf(float lo, float hi) {
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

// ─── Color ───────────────────────────────────────────────────────────────────
struct Color { float r, g, b, a; };
const Color C_CYAN    = {0.0f, 1.0f, 1.0f, 1.0f};
const Color C_MAGENTA = {1.0f, 0.0f, 1.0f, 1.0f};
const Color C_YELLOW  = {1.0f, 1.0f, 0.0f, 1.0f};
const Color C_GREEN   = {0.0f, 1.0f, 0.4f, 1.0f};
const Color C_ORANGE  = {1.0f, 0.5f, 0.0f, 1.0f};
const Color C_RED     = {1.0f, 0.15f, 0.15f, 1.0f};
const Color C_WHITE   = {1.0f, 1.0f, 1.0f, 1.0f};

// ─── Timer ───────────────────────────────────────────────────────────────────
float getTime() {
    return (float)GetTickCount() / 1000.0f;
}

// ============================================================
//  TEXT RENDERING (using Windows GDI font → OpenGL bitmap)
// ============================================================
GLuint g_fontBase = 0;

void initFont(HDC hdc) {
    g_fontBase = glGenLists(128);
    HFONT hFont = CreateFontA(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH,
        "Courier New");
    HFONT old = (HFONT)SelectObject(hdc, hFont);
    wglUseFontBitmaps(hdc, 0, 128, g_fontBase);
    SelectObject(hdc, old);
    DeleteObject(hFont);
}

void drawText(float x, float y, const std::string& s, Color c) {
    glColor4f(c.r, c.g, c.b, c.a);
    glRasterPos2f(x, y);
    glListBase(g_fontBase);
    glCallLists((GLsizei)s.size(), GL_UNSIGNED_BYTE, s.c_str());
}

// ============================================================
//  DRAWING PRIMITIVES
// ============================================================
// Draw filled circle with GL_TRIANGLE_FAN
void drawCircle(float x, float y, float r, Color c, int segs = 24) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= segs; i++) {
            float a = 2.0f * PI * i / segs;
            glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
        }
    glEnd();
}

void drawCircleOutline(float x, float y, float r, Color c, int segs = 24) {
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segs; i++) {
            float a = 2.0f * PI * i / segs;
            glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
        }
    glEnd();
}

// CG Concept #6: Additive blending glow halo
void drawGlow(float x, float y, float r, Color c) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive
    for (int l = 3; l >= 1; l--) {
        Color gc = {c.r, c.g, c.b, c.a * 0.12f / l};
        drawCircle(x, y, r * (1.0f + l * 0.5f), gc);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

// ============================================================
//  PARTICLE SYSTEM
//  CG Concept #7: Particles with velocity, gravity, alpha-fade
// ============================================================
struct Particle {
    float x, y, vx, vy, life, decay, size;
    Color color;
    bool  active;
};
const int MAX_PARTICLES = 700;
Particle  particles[MAX_PARTICLES];

void initParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].active = false;
}

void spawnExplosion(float px, float py, Color base, int count = 30) {
    int n = 0;
    for (int i = 0; i < MAX_PARTICLES && n < count; i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].x = px; particles[i].y = py;
            float a = randf(0, 2.0f * PI);
            float s = randf(50, 200);
            particles[i].vx = cosf(a) * s;
            particles[i].vy = sinf(a) * s;
            particles[i].life  = 1.0f;
            particles[i].decay = randf(0.8f, 2.5f);
            particles[i].size  = randf(2.0f, 6.0f);
            particles[i].color = {
                base.r * randf(0.7f, 1.0f),
                base.g * randf(0.7f, 1.0f),
                base.b * randf(0.7f, 1.0f), 1.0f};
            n++;
        }
    }
}

void updateParticles(float dt) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        particles[i].x    += particles[i].vx * dt;
        particles[i].y    += particles[i].vy * dt;
        particles[i].vy   -= 60.0f * dt;               // gravity
        particles[i].life -= particles[i].decay * dt;
        if (particles[i].life <= 0) particles[i].active = false;
    }
}

// CG Concept #3 (GL_POINTS) + #6 (additive blend for glow)
void drawParticles() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_POINTS);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        float a = particles[i].life;
        glColor4f(particles[i].color.r,
                  particles[i].color.g,
                  particles[i].color.b, a);
        glPointSize(particles[i].size * a + 1.0f);
        glVertex2f(particles[i].x, particles[i].y);
    }
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

// ============================================================
//  STAR FIELD (Parallax scrolling)
//  CG Concept #8: Multi-layer parallax background
// ============================================================
const int STAR_COUNT = 220;
struct Star { float x, y, speed, brightness, size; };
Star stars[STAR_COUNT];

void initStars() {
    for (int i = 0; i < STAR_COUNT; i++) {
        float layer         = randf(0, 1.0f);
        stars[i].x          = randf(0, WORLD_W);
        stars[i].y          = randf(0, WORLD_H);
        stars[i].speed      = lerp(18.0f, 65.0f, layer);
        stars[i].brightness = lerp(0.25f, 1.0f,  layer);
        stars[i].size       = lerp(0.5f,  2.5f,  layer);
    }
}

void updateStars(float dt) {
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].y -= stars[i].speed * dt;
        if (stars[i].y < 0) {
            stars[i].x = randf(0, WORLD_W);
            stars[i].y = WORLD_H + 5.0f;
        }
    }
}

void drawStars() {
    for (int i = 0; i < STAR_COUNT; i++) {
        float b = stars[i].brightness;
        glPointSize(stars[i].size);
        glBegin(GL_POINTS);
            glColor3f(b * 0.8f, b * 0.85f, b);
            glVertex2f(stars[i].x, stars[i].y);
        glEnd();
    }
}

// ============================================================
//  PLAYER
// ============================================================
struct Player {
    float x, y, speed;
    int   lives;
    float shootCool, invTimer, thrustAnim;
    bool  alive;
    void init() {
        x=WORLD_W/2; y=80; speed=290;
        lives=3; shootCool=0; invTimer=0; thrustAnim=0; alive=true;
    }
};
Player player;

// CG Concept #2 + #4: glTranslate + per-vertex colour on ship polygons
void drawPlayer(const Player& p) {
    if (!p.alive) return;
    if (p.invTimer > 0 && (int)(p.invTimer * 10) % 2 == 0) return;

    glPushMatrix();
    glTranslatef(p.x, p.y, 0);   // CG: Translation

    // Thruster exhaust flame
    float tf = 10.0f + sinf(p.thrustAnim * 15.0f) * 5.0f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_TRIANGLES);
        // Left thruster
        glColor4f(0.0f, 0.7f, 1.0f, 0.9f); glVertex2f(-14, -18);
        glColor4f(0.1f, 0.1f, 1.0f, 0.4f); glVertex2f(-19, -18 - tf);
        glColor4f(1.0f, 1.0f, 0.0f, 0.0f); glVertex2f(-10, -18 - tf*0.6f);
        // Right thruster
        glColor4f(0.0f, 0.7f, 1.0f, 0.9f); glVertex2f( 14, -18);
        glColor4f(0.1f, 0.1f, 1.0f, 0.4f); glVertex2f( 19, -18 - tf);
        glColor4f(1.0f, 1.0f, 0.0f, 0.0f); glVertex2f( 10, -18 - tf*0.6f);
    glEnd();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);

    // Ship hull — per-vertex gradient
    glBegin(GL_TRIANGLES);
        glColor3f(0.0f, 1.0f, 1.0f); glVertex2f(  0,  28);   // nose (cyan)
        glColor3f(0.2f, 0.5f, 0.8f); glVertex2f(-18, -10);
        glColor3f(0.2f, 0.5f, 0.8f); glVertex2f( 18, -10);

        glColor3f(0.1f, 0.35f, 0.65f); glVertex2f(-18, -10);
        glColor3f(0.05f,0.1f, 0.30f);  glVertex2f(-12, -20);
        glColor3f(0.05f,0.1f, 0.30f);  glVertex2f( 12, -20);

        glColor3f(0.1f, 0.35f, 0.65f); glVertex2f(-18, -10);
        glColor3f(0.05f,0.1f, 0.30f);  glVertex2f( 12, -20);
        glColor3f(0.1f, 0.35f, 0.65f); glVertex2f( 18, -10);

        // Wings
        glColor3f(0.0f, 0.85f, 0.85f); glVertex2f(-18, -10);
        glColor3f(0.0f, 0.3f,  0.55f); glVertex2f(-40, -22);
        glColor3f(0.0f, 0.3f,  0.55f); glVertex2f(-12, -20);

        glColor3f(0.0f, 0.85f, 0.85f); glVertex2f( 18, -10);
        glColor3f(0.0f, 0.3f,  0.55f); glVertex2f( 40, -22);
        glColor3f(0.0f, 0.3f,  0.55f); glVertex2f( 12, -20);
    glEnd();

    // Cockpit
    drawCircle(0, 8, 7, {0.4f, 0.95f, 1.0f, 0.85f});
    // Wing-tip glow
    drawGlow(-18, -10, 4, {0.0f, 1.0f, 1.0f, 0.7f});
    drawGlow( 18, -10, 4, {0.0f, 1.0f, 1.0f, 0.7f});

    glPopMatrix();
}

// ============================================================
//  BULLETS
// ============================================================
struct Bullet {
    float x, y, vy;
    bool  active, isPlayer;
};
const int MAX_BULLETS = 90;
Bullet    bullets[MAX_BULLETS];

void initBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
}

void spawnBullet(float x, float y, float vy, bool isPlayer) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i] = {x, y, vy, true, isPlayer}; return;
        }
    }
}

void updateBullets(float dt) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].y += bullets[i].vy * dt;
        if (bullets[i].y > WORLD_H + 10 || bullets[i].y < -10)
            bullets[i].active = false;
    }
}

// CG Concept #3 (GL_QUADS), #6 (glow)
void drawBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        float x = bullets[i].x, y = bullets[i].y;
        Color ci = bullets[i].isPlayer ? C_CYAN   : C_RED;
        Color co = bullets[i].isPlayer ? C_WHITE  : C_ORANGE;

        drawGlow(x, y, 5, ci);

        glBegin(GL_QUADS);
            glColor4f(co.r, co.g, co.b, 1.0f);
            glVertex2f(x-2, y-10); glVertex2f(x+2, y-10);
            glColor4f(ci.r, ci.g, ci.b, 1.0f);
            glVertex2f(x+2, y+10); glVertex2f(x-2, y+10);
        glEnd();
    }
}

// ============================================================
//  ENEMIES — Three types with distinct shapes
// ============================================================
struct Enemy {
    float x, y;
    int   type;          // 0=Scout, 1=Fighter, 2=Commander
    bool  alive;
    float shootTimer, shootInterval;
    float pulseAnim;
};

const int ENEMY_COLS = 9;
const int ENEMY_ROWS = 4;
const int MAX_ENEMIES = ENEMY_COLS * ENEMY_ROWS;
Enemy   enemies[MAX_ENEMIES];
float   enemyDir  = 1.0f;
int     waveNumber = 1;

// ──── Scout ──────────────────────────────────────────────────────────────────
void drawScout(float x, float y, float t) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    float s = 1.0f + sinf(t * 3.0f) * 0.04f;
    glScalef(s, s, 1);  // CG: Scale (pulsing)

    glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.2f, 1.0f, 0.4f); glVertex2f(0, 0);
        for (int i = 0; i <= 20; i++) {
            float a = 2*PI*i/20;
            glColor3f(0.0f, 0.4f, 0.15f);
            glVertex2f(cosf(a)*14, sinf(a)*9);
        }
    glEnd();
    // Eyes
    drawCircle(-5, 3, 3, {1.0f, 0.0f, 0.0f, 1.0f});
    drawCircle( 5, 3, 3, {1.0f, 0.0f, 0.0f, 1.0f});
    // Antennae
    glColor3f(0.0f, 1.0f, 0.4f); glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(-6,9); glVertex2f(-9,17);
        glVertex2f( 6,9); glVertex2f( 9,17);
    glEnd();
    drawCircle(-9,17,2, C_GREEN);
    drawCircle( 9,17,2, C_GREEN);
    glPopMatrix();
}

// ──── Fighter ────────────────────────────────────────────────────────────────
void drawFighter(float x, float y, float t) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(sinf(t*2)*4, 0,0,1);  // CG: Rotation (wobble)

    glBegin(GL_TRIANGLES);
        glColor3f(0.85f,0.25f,1.0f); glVertex2f( 0, 13);
        glColor3f(0.35f,0.0f, 0.7f); glVertex2f(-13,-8);
        glColor3f(0.35f,0.0f, 0.7f); glVertex2f( 13,-8);
        // Left claw
        glColor3f(0.95f,0.1f,0.95f); glVertex2f(-13,-8);
        glColor3f(0.15f,0.0f,0.4f);  glVertex2f(-24,-3);
        glColor3f(0.15f,0.0f,0.4f);  glVertex2f(-19,-19);
        // Right claw
        glColor3f(0.95f,0.1f,0.95f); glVertex2f( 13,-8);
        glColor3f(0.15f,0.0f,0.4f);  glVertex2f( 24,-3);
        glColor3f(0.15f,0.0f,0.4f);  glVertex2f( 19,-19);
    glEnd();
    drawCircle(0, 2, 5, {1.0f, 0.6f, 1.0f, 1.0f});
    glPopMatrix();
}

// ──── Commander ──────────────────────────────────────────────────────────────
void drawCommander(float x, float y, float t) {
    glPushMatrix();
    glTranslatef(x, y, 0);

    // Saucer body
    glBegin(GL_TRIANGLE_FAN);
        glColor3f(1.0f,0.75f,0.0f); glVertex2f(0,0);
        for (int i = 0; i <= 24; i++) {
            float a = 2*PI*i/24;
            glColor3f(0.55f,0.28f,0.0f);
            glVertex2f(cosf(a)*19, sinf(a)*10);
        }
    glEnd();
    // Dome
    glBegin(GL_TRIANGLE_FAN);
        glColor3f(1.0f,0.92f,0.35f); glVertex2f(0,10);
        for (int i = 0; i <= 16; i++) {
            float a = PI*i/16;
            glColor3f(0.75f,0.5f,0.1f);
            glVertex2f(cosf(a)*11, 10+sinf(a)*9);
        }
    glEnd();
    // Spinning ring — CG: Rotation transform
    glPushMatrix();
    glRotatef(t * 90.0f, 0,0,1);
    drawCircleOutline(0,0,23, {1.0f,0.6f,0.0f,0.9f}, 32);
    glPopMatrix();
    // Pulsing eye
    float ep = 0.5f + 0.5f*sinf(t*5);
    drawCircle(0, 2, 6, {1.0f, ep*0.3f, 0.0f, 1.0f});
    glPopMatrix();
}

void drawEnemy(const Enemy& e) {
    if (!e.alive) return;
    if      (e.type==0) drawScout    (e.x, e.y, e.pulseAnim);
    else if (e.type==1) drawFighter  (e.x, e.y, e.pulseAnim);
    else                drawCommander(e.x, e.y, e.pulseAnim);
}

// ============================================================
//  SHIELDS
// ============================================================
const int   SHIELD_COUNT = 4;
struct Shield { float x, y; int hp; };
Shield shields[SHIELD_COUNT];

void initShields() {
    for (int i = 0; i < SHIELD_COUNT; i++) {
        shields[i] = {100.0f + i * 190.0f, 150.0f, 12};
    }
}

void drawShields() {
    for (int i = 0; i < SHIELD_COUNT; i++) {
        if (shields[i].hp <= 0) continue;
        float r2 = shields[i].hp / 12.0f;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1-r2, r2*0.8f, 0.2f, 0.5f+0.35f*r2);
        glBegin(GL_QUADS);
            glVertex2f(shields[i].x-32, shields[i].y-14);
            glVertex2f(shields[i].x+32, shields[i].y-14);
            glVertex2f(shields[i].x+32, shields[i].y+14);
            glVertex2f(shields[i].x-32, shields[i].y+14);
        glEnd();
        glDisable(GL_BLEND);
        glColor3f(1-r2*0.5f, r2*0.6f+0.4f, 0.5f); glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(shields[i].x-32, shields[i].y-14);
            glVertex2f(shields[i].x+32, shields[i].y-14);
            glVertex2f(shields[i].x+32, shields[i].y+14);
            glVertex2f(shields[i].x-32, shields[i].y+14);
        glEnd();
    }
}

// ============================================================
//  GAME STATE
// ============================================================
enum GameState { S_MENU, S_PLAY, S_OVER };
GameState gState = S_MENU;

int   score   = 0;
int   hiScore = 0;
bool  keyLeft = false, keyRight = false;
float gameTime = 0;

// ============================================================
//  AABB COLLISION  (CG Concept #10)
// ============================================================
bool aabb(float ax,float ay,float aw,float ah,
          float bx,float by,float bw,float bh) {
    return fabsf(ax-bx) < (aw+bw)*0.5f &&
           fabsf(ay-by) < (ah+bh)*0.5f;
}

// ============================================================
//  WAVE INIT
// ============================================================
void initWave() {
    int idx = 0;
    float sp = (waveNumber-1)*0.14f;
    for (int row = 0; row < ENEMY_ROWS; row++) {
        int type = (row==0)?2:(row==1)?1:0;
        for (int col = 0; col < ENEMY_COLS; col++) {
            float bx = 88.0f + col*72.0f;
            float by = WORLD_H - 110.0f - row*65.0f;
            enemies[idx++] = {
                bx, by, type, true,
                randf(1.5f,3.5f)/(1+sp),
                randf(1.5f,3.5f)/(1+sp),
                randf(0,2*PI)
            };
        }
    }
    enemyDir = 1.0f;
}

// ============================================================
//  FULL GAME INIT
// ============================================================
void initGame() {
    player.init();
    initBullets();
    initParticles();
    initShields();
    waveNumber = 1;
    score      = 0;
    gameTime   = 0;
    initWave();
    gState = S_PLAY;
}

// ============================================================
//  UPDATE GAME  (called every frame)
// ============================================================
void updateGame(float dt) {
    gameTime += dt;

    if (keyLeft  && player.x > 32)  player.x -= player.speed * dt;
    if (keyRight && player.x < WORLD_W-32) player.x += player.speed * dt;

    player.shootCool  = (std::max)(0.0f, player.shootCool  - dt);
    player.invTimer   = (std::max)(0.0f, player.invTimer   - dt);
    player.thrustAnim += dt;

    updateStars(dt);
    updateBullets(dt);
    updateParticles(dt);

    // Enemy movement
    float spd = (32.0f + waveNumber*6.0f) * enemyDir;
    bool  hitWall = false;
    int   alive   = 0;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].alive) continue;
        alive++;
        enemies[i].x        += spd * dt;
        enemies[i].pulseAnim += dt;

        if (enemies[i].x > WORLD_W-28 || enemies[i].x < 28) hitWall = true;

        // Enemy shoot
        enemies[i].shootTimer -= dt;
        if (enemies[i].shootTimer <= 0) {
            enemies[i].shootTimer = enemies[i].shootInterval;
            // Only bottommost in column fires
            bool bot = true;
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (!enemies[j].alive || j==i) continue;
                if (fabsf(enemies[j].x - enemies[i].x) < 12 &&
                    enemies[j].y < enemies[i].y) { bot = false; break; }
            }
            if (bot) spawnBullet(enemies[i].x, enemies[i].y-16, -190, false);
        }
    }
    if (hitWall) {
        enemyDir *= -1;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (enemies[i].alive) enemies[i].y -= 22;
    }

    // Collisions: player bullet vs enemy
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active || !bullets[b].isPlayer) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!enemies[e].alive) continue;
            if (aabb(bullets[b].x,bullets[b].y,4,20,
                     enemies[e].x, enemies[e].y,18,14)) {
                bullets[b].active = false;
                enemies[e].alive  = false;
                Color bc = (enemies[e].type==2)?C_YELLOW:
                           (enemies[e].type==1)?C_MAGENTA:C_GREEN;
                spawnExplosion(enemies[e].x, enemies[e].y, bc, 36);
                score += (enemies[e].type+1)*10;
                if (score > hiScore) hiScore = score;
            }
        }
    }

    // Collisions: enemy bullet vs shield
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active || bullets[b].isPlayer) continue;
        for (int s = 0; s < SHIELD_COUNT; s++) {
            if (shields[s].hp <= 0) continue;
            if (aabb(bullets[b].x,bullets[b].y,4,20,
                     shields[s].x, shields[s].y,64,28)) {
                bullets[b].active = false;
                shields[s].hp--;
                spawnExplosion(bullets[b].x,bullets[b].y,C_GREEN,8);
            }
        }
    }

    // Collisions: enemy bullet vs player
    if (player.alive && player.invTimer <= 0) {
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (!bullets[b].active || bullets[b].isPlayer) continue;
            if (aabb(bullets[b].x,bullets[b].y,4,20,
                     player.x, player.y, 38, 52)) {
                bullets[b].active = false;
                player.lives--;
                player.invTimer = 2.5f;
                spawnExplosion(player.x, player.y, C_CYAN, 22);
                if (player.lives <= 0) {
                    player.alive = false;
                    spawnExplosion(player.x, player.y, C_CYAN, 70);
                    gState = S_OVER;
                }
            }
        }
    }

    // Enemy reaches bottom → game over
    for (int e = 0; e < MAX_ENEMIES; e++)
        if (enemies[e].alive && enemies[e].y < 55) { gState = S_OVER; }

    // Wave cleared
    if (alive == 0) {
        waveNumber++;
        initShields(); initWave();
        spawnExplosion(WORLD_W/2, WORLD_H/2, C_YELLOW, 80);
    }
}

// ============================================================
//  HUD  (CG Concept #12: Bitmap text)
// ============================================================
void drawHUD() {
    // Background bar  (GL_QUADS)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0.08f,0.75f);
    glBegin(GL_QUADS);
        glVertex2f(0,WORLD_H-38); glVertex2f(WORLD_W,WORLD_H-38);
        glVertex2f(WORLD_W,WORLD_H); glVertex2f(0,WORLD_H);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(0,0.7f,0.7f); glLineWidth(1.0f);
    glBegin(GL_LINES);
        glVertex2f(0,WORLD_H-38); glVertex2f(WORLD_W,WORLD_H-38);
    glEnd();

    char buf[64];
    sprintf(buf, "SCORE: %d", score);
    drawText(18, WORLD_H-24, buf, C_CYAN);

    sprintf(buf, "BEST: %d", hiScore);
    drawText(WORLD_W/2-55, WORLD_H-24, buf, C_YELLOW);

    sprintf(buf, "WAVE: %d", waveNumber);
    drawText(WORLD_W-130, WORLD_H-24, buf, C_MAGENTA);

    // Life icons
    drawText(16, 22, "LIVES:", C_WHITE);
    for (int i = 0; i < player.lives; i++) {
        glPushMatrix();
        glTranslatef(80+i*26.0f, 18, 0);
        glScalef(0.5f,0.5f,1);
        glBegin(GL_TRIANGLES);
            glColor3f(0,1,1); glVertex2f(0,20);
            glColor3f(0,.4f,.8f); glVertex2f(-13,-8);
            glColor3f(0,.4f,.8f); glVertex2f( 13,-8);
        glEnd();
        glPopMatrix();
    }

    glColor3f(0,.35f,.35f);
    glBegin(GL_LINES); glVertex2f(0,40); glVertex2f(WORLD_W,40); glEnd();
}

// ============================================================
//  MENU
// ============================================================
void drawMenu() {
    float t = getTime();

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
        glColor4f(0,0.4f,0.6f,0.3f); glVertex2f(60,575); glVertex2f(740,575);
        glColor4f(0,0.1f,0.2f,0.0f); glVertex2f(740,615); glVertex2f(60,615);
    glEnd();
    glDisable(GL_BLEND);

    drawText(175, 582, "N E B U L A   D E F E N D E R", C_CYAN);
    drawText(220, 548, "2D OpenGL Space Shooter  |  Computer Graphics", C_WHITE);

    drawText(220, 495, "CONTROLS:", C_YELLOW);
    drawText(195, 470, "ARROW LEFT / RIGHT  -  Move Ship", C_WHITE);
    drawText(195, 445, "SPACE               -  Fire Laser", C_WHITE);
    drawText(195, 420, "ESC                 -  Quit", C_WHITE);

    drawText(195, 378, "ENEMY SCORE GUIDE:", C_YELLOW);

    // Live enemy previews in the menu
    float sc = (t > 0)?t:0;
    drawScout    (210, 345, sc); drawText(250,340,"Scout        =  10 pts",C_GREEN);
    drawFighter  (210, 300, sc); drawText(250,295,"Fighter      =  20 pts",C_MAGENTA);
    drawCommander(210, 243, sc); drawText(250,238,"Commander    =  30 pts",C_YELLOW);

    if ((int)(t*2)%2==0)
        drawText(218, 182, "Press  ENTER  to  Start!", C_CYAN);

    drawText(185, 135, "Shields protect you - guard them well!", C_WHITE);
    drawText(190,  95, "CG Demonstrated: Transforms | Blending | Particles", C_WHITE);
}

// ============================================================
//  GAME OVER
// ============================================================
void drawGameOver() {
    float t = getTime();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0.05f,0.8f);
    glBegin(GL_QUADS);
        glVertex2f(0,0); glVertex2f(WORLD_W,0);
        glVertex2f(WORLD_W,WORLD_H); glVertex2f(0,WORLD_H);
    glEnd();
    glDisable(GL_BLEND);

    drawText(260, 438, "-- GAME  OVER --", C_RED);
    char buf[64];
    sprintf(buf, "Final Score : %d", score);   drawText(265,398,buf,C_YELLOW);
    sprintf(buf, "Wave Reached: %d", waveNumber); drawText(265,368,buf,C_CYAN);
    sprintf(buf, "Hi-Score    : %d", hiScore);  drawText(265,338,buf,C_WHITE);

    if ((int)(t*2)%2==0)
        drawText(205, 288, "Press ENTER to Play Again", C_CYAN);
    drawText(250, 258, "ESC to Quit", C_WHITE);
}

// ============================================================
//  MAIN RENDER
// ============================================================
void render() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Background sky gradient  (CG Concept #4: per-vertex colors)
    glBegin(GL_QUADS);
        glColor3f(0.02f,0.02f,0.08f); glVertex2f(0,0);
        glColor3f(0.02f,0.02f,0.08f); glVertex2f(WORLD_W,0);
        glColor3f(0.04f,0.04f,0.16f); glVertex2f(WORLD_W,WORLD_H);
        glColor3f(0.04f,0.04f,0.16f); glVertex2f(0,WORLD_H);
    glEnd();

    drawStars();

    if (gState == S_MENU) {
        drawMenu();
    } else {
        drawShields();
        for (int i = 0; i < MAX_ENEMIES; i++) drawEnemy(enemies[i]);
        drawBullets();
        drawParticles();
        drawPlayer(player);
        drawHUD();
        if (gState == S_OVER) { updateStars(0); drawGameOver(); }
    }

    SwapBuffers(g_hdc);
}

// ============================================================
//  OPENGL SETUP (using WGL — no external windowing library)
//  CG Concept #1: 2D Orthographic projection
// ============================================================
bool setupOpenGL(HWND hwnd) {
    g_hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 16;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(g_hdc, &pfd);
    if (!fmt || !SetPixelFormat(g_hdc, fmt, &pfd)) return false;

    g_hrc = wglCreateContext(g_hdc);
    if (!g_hrc) return false;
    wglMakeCurrent(g_hdc, g_hrc);

    glViewport(0,0,WIN_W,WIN_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WORLD_W, 0, WORLD_H);   // CG: 2D Orthographic Projection
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.02f, 0.02f, 0.08f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);

    initFont(g_hdc);
    return true;
}

// ============================================================
//  WINDOWS MESSAGE HANDLER
// ============================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_KEYDOWN:
        switch (wp) {
        case VK_LEFT:   keyLeft  = true;  break;
        case VK_RIGHT:  keyRight = true;  break;
        case VK_SPACE:
            if (gState == S_PLAY && player.shootCool <= 0 && player.alive) {
                spawnBullet(player.x,      player.y+30, 430, true);
                spawnBullet(player.x-18,   player.y-4,  410, true);
                spawnBullet(player.x+18,   player.y-4,  410, true);
                player.shootCool = 0.22f;
                spawnExplosion(player.x, player.y+30, C_CYAN, 5);
            }
            break;
        case VK_RETURN:
            if (gState == S_MENU || gState == S_OVER) initGame();
            break;
        case VK_ESCAPE: PostQuitMessage(0); break;
        }
        break;

    case WM_KEYUP:
        if (wp == VK_LEFT)  keyLeft  = false;
        if (wp == VK_RIGHT) keyRight = false;
        break;

    case WM_SIZE:
        glViewport(0,0,LOWORD(lp),HIWORD(lp));
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0,WORLD_W,0,WORLD_H);
        glMatrixMode(GL_MODELVIEW);
        break;

    case WM_DESTROY:
        if (g_hrc) { wglMakeCurrent(NULL,NULL); wglDeleteContext(g_hrc); }
        if (g_hdc) ReleaseDC(hwnd, g_hdc);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// ============================================================
//  WINMAIN — Entry point
// ============================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    srand((unsigned)time(NULL));

    // Register window class
    WNDCLASSA wc   = {};
    wc.style       = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance   = hInst;
    wc.hCursor     = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "NebulaDefender";
    RegisterClassA(&wc);

    // Adjust window rect so client area is exactly WIN_W x WIN_H
    RECT rc = {0, 0, WIN_W, WIN_H};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowA(
        "NebulaDefender",
        "Nebula Defender  |  Computer Graphics  |  2D OpenGL Space Shooter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInst, NULL);

    if (!g_hwnd || !setupOpenGL(g_hwnd)) {
        MessageBoxA(NULL, "OpenGL initialization failed!", "Error", MB_OK|MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // Init game world
    initStars();
    gState = S_MENU;

    // ── Game loop (CG Concept #9: Real-time animation loop) ──────────────────
    float prevTime = getTime();
    MSG   msg      = {};

    while (true) {
        // Process all queued messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        float now = getTime();
        float dt  = now - prevTime;
        prevTime  = now;
        if (dt > 0.08f) dt = 0.08f;   // clamp

        if (gState == S_PLAY)
            updateGame(dt);
        else {
            updateStars(dt);
            updateParticles(dt);
        }

        render();

        // ~60fps cap
        Sleep(14);
    }

done:
    if (g_fontBase) glDeleteLists(g_fontBase, 128);
    return (int)msg.wParam;
}
