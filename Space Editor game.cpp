#define _USE_MATH_DEFINES
#define GLUT_DISABLE_ATEXIT_HACK

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <glut.h>

#ifdef _MSC_VER
#undef exit
#endif

// =====================
// Data structures
// =====================
struct Vec2 { float x, y; Vec2(float X = 0, float Y = 0) :x(X), y(Y) {} };

struct Collectible { Vec2 pos; bool active = true; float phase = 0.0f; };
struct Obstacle { Vec2 pos; float w, h; };

enum PowerType { P_SHIELD = 0, P_SPEED = 1 };
struct PowerUp { Vec2 pos; bool active = true; PowerType type = P_SHIELD; float phase = 0.0f; };

// =====================
// Game state
// =====================
int windowWidth = 800, windowHeight = 600;

// UI and world extents
const float UI_TOP_HEIGHT = 0.12f;   // normalized screen units for panels
const float UI_BOTTOM_HEIGHT = 0.10f;
const float WORLD_LEFT = -1.0f, WORLD_RIGHT = 1.0f;
const float WORLD_BOTTOM = -1.0f, WORLD_TOP = 3.0f; // taller world

float playerX = 0.0f, playerY = -0.9f;
float playerAngle = 0.0f; // rotation to face movement
float playerSpeed = 0.05f;
int score = 0;
int lives = 5;
float gameTimer = 30.0f; // <-- changed to 30 seconds
bool gameOver = false;
bool gameWin = false;
bool gameStarted = false; // editing mode initially

std::vector<Collectible> collectibles;
std::vector<Obstacle> obstacles;
std::vector<PowerUp> powerups;

// placement tools
enum Tool { TOOL_NONE = 0, TOOL_OBSTACLE, TOOL_COLLECTIBLE, TOOL_P_SHIELD, TOOL_P_SPEED };
Tool selectedTool = TOOL_NONE;

// powerup active state
bool shieldActive = false;
float shieldTimer = 0.0f; // seconds remaining
float shieldDuration = 5.0f; // user requested 5s
// speed power-up variables
bool speedActive = false;
float speedTimer = 0.0f;
float speedDuration = 5.0f; // speed lasts this many seconds
float basePlayerSpeed = 0.05f;
float speedMultiplier = 1.8f; // how much faster when speed powerup active

// animations
float globalTime = 0.0f;
float lastMoveTime = -100.0f; // used to show brief thruster flame when player recently moved

// target bezier movement
Vec2 targetPos(0.0f, 0.7f); // ensure visible frame
float targetAnimT = 0.0f; // 0..1 parameter along bezier
std::vector<Vec2> targetBezier;

// UI messages
std::string statusMessage = "Place objects then press R to start";
float messageTimer = 0.0f;

// helpers
float randf(float a, float b) { return a + (float(rand()) / RAND_MAX) * (b - a); }

// =====================
// Drawing helpers & primitives (we use many different GL primitives explicitly)
// =====================

void drawQuad(float x, float y, float w, float h) { // GL_QUADS
    glBegin(GL_QUADS);
    glVertex2f(x - w, y - h);
    glVertex2f(x + w, y - h);
    glVertex2f(x + w, y + h);
    glVertex2f(x - w, y + h);
    glEnd();
}

void drawCircle(float cx, float cy, float r, int segs = 24) { // GL_TRIANGLE_FAN
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0;i <= segs;i++) { float a = i * 2 * M_PI / segs; glVertex2f(cx + cos(a) * r, cy + sin(a) * r); }
    glEnd();
}

void drawLine(float x1, float y1, float x2, float y2) { // GL_LINES
    glBegin(GL_LINES); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
}

void drawLineStrip(const std::vector<Vec2>& pts) { // GL_LINE_STRIP
    glBegin(GL_LINE_STRIP); for (auto& p : pts) glVertex2f(p.x, p.y); glEnd();
}

void drawLineLoop(const std::vector<Vec2>& pts) { // GL_LINE_LOOP
    glBegin(GL_LINE_LOOP); for (auto& p : pts) glVertex2f(p.x, p.y); glEnd();
}

void drawPoint(float x, float y) { glBegin(GL_POINTS); glVertex2f(x, y); glEnd(); } // GL_POINTS (allowed in earlier description)

// heart (used for health and extra life powerup) - GL_POLYGON
void drawHeart(float x, float y, float size) {
    glBegin(GL_POLYGON);
    for (float t = 0;t < 2 * M_PI;t += 0.05f) {
        float X = 16 * pow(sin(t), 3);
        float Y = 13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t);
        X /= 18; Y /= 18;
        glVertex2f(x + X * size, y + Y * size);
    }
    glEnd();
}

// star as GL_TRIANGLES (for collectibles) - uses GL_TRIANGLES primitive
void drawStarTriangles(float cx, float cy, float outerR) {
    const int points = 5;
    std::vector<Vec2> outer(points), inner(points);
    for (int i = 0;i < points;i++) {
        float a = i * 2 * M_PI / points - M_PI / 2.0f;
        outer[i] = Vec2(cx + cos(a) * outerR, cy + sin(a) * outerR);
        float a2 = a + M_PI / points;
        inner[i] = Vec2(cx + cos(a2) * outerR * 0.45f, cy + sin(a2) * outerR * 0.45f);
    }
    glBegin(GL_TRIANGLES);
    for (int i = 0;i < points;i++) {
        int ni = (i + 1) % points;
        // triangle for each point: outer[i], inner[i], outer[ni]
        glVertex2f(outer[i].x, outer[i].y);
        glVertex2f(inner[i].x, inner[i].y);
        glVertex2f(outer[ni].x, outer[ni].y);
    }
    glEnd();
}

// score powerup drawn with GL_TRIANGLE_STRIP + GL_LINE_STRIP (two different primitives)
void drawScorePowerupShape(float cx, float cy, float s) {
    // diamond using triangle strip
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(cx, cy + s);
    glVertex2f(cx + s, cy);
    glVertex2f(cx, cy - s);
    glVertex2f(cx - s, cy);
    glEnd();
    // outline using line strip
    std::vector<Vec2> outline = { {cx,cy + s},{cx + s,cy},{cx,cy - s},{cx - s,cy},{cx,cy + s} };
    glColor3f(0, 0, 0);
    drawLineStrip(outline);
}

// shield icon: GL_POLYGON + GL_LINE_LOOP (two primitives)
void drawShieldIcon(float cx, float cy, float s) {
    glBegin(GL_POLYGON);
    glVertex2f(cx - s * 0.5f, cy + s * 0.2f);
    glVertex2f(cx + s * 0.5f, cy + s * 0.2f);
    glVertex2f(cx + s * 0.25f, cy - s * 0.1f);
    glVertex2f(cx, cy - s * 0.5f);
    glVertex2f(cx - s * 0.25f, cy - s * 0.1f);
    glEnd();
    std::vector<Vec2> loop;
    for (int i = 0;i < 20;i++) { float a = i * 2 * M_PI / 20; loop.push_back(Vec2(cx + cos(a) * s * 0.25f, cy + sin(a) * s * 0.15f)); }
    drawLineLoop(loop);
}

// obstacle primitive: GL_QUADS + GL_LINE_LOOP (2 primitives)
void drawObstacleIcon(float cx, float cy, float w, float h) {
    glBegin(GL_QUADS); glVertex2f(cx - w, cy - h); glVertex2f(cx + w, cy - h); glVertex2f(cx + w, cy + h); glVertex2f(cx - w, cy + h); glEnd();
    std::vector<Vec2> loop = { {cx - w,cy - h},{cx + w,cy - h},{cx + w,cy + h},{cx - w,cy + h} };
    drawLineLoop(loop);
}

// collectible icon: GL_TRIANGLES (star), GL_TRIANGLE_FAN (circle), GL_LINES (line) -> 3 different primitives
void drawCollectibleIcon(float cx, float cy, float s) {
    glColor3f(1.0f, 0.9f, 0.2f); drawStarTriangles(cx, cy, s * 0.9f);
    glColor3f(1, 1, 1); drawCircle(cx, cy, s * 0.25f, 12);
    glColor3f(0, 0, 0); drawLine(cx - s * 0.6f, cy, cx + s * 0.6f, cy);
}

// =====================
// Player (uses >=4 different primitives: GL_POLYGON, GL_TRIANGLES, GL_TRIANGLE_FAN, GL_LINES)
// =====================
void drawPlayer() {
    // Draw rocket centered at origin (local coordinates). Caller should translate/rotate to the player's world position.
    // fuselage (GL_POLYGON)
    glColor3f(0.9f, 0.9f, 0.95f);
    glBegin(GL_POLYGON);
    glVertex2f(0.0f, 0.10f);
    glVertex2f(0.035f, 0.06f);
    glVertex2f(0.035f, -0.06f);
    glVertex2f(0.0f, -0.11f);
    glVertex2f(-0.035f, -0.06f);
    glVertex2f(-0.035f, 0.06f);
    glEnd();

    // nose cone (GL_TRIANGLES)
    glColor3f(0.95f, 0.6f, 0.2f);
    glBegin(GL_TRIANGLES);
    glVertex2f(0.0f, 0.10f);
    glVertex2f(-0.02f, 0.045f);
    glVertex2f(0.02f, 0.045f);
    glEnd();

    // fins (GL_TRIANGLES)
    glColor3f(0.8f, 0.15f, 0.15f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-0.02f, -0.02f);
    glVertex2f(-0.09f, -0.06f);
    glVertex2f(-0.02f, -0.06f);
    glVertex2f(0.02f, -0.02f);
    glVertex2f(0.09f, -0.06f);
    glVertex2f(0.02f, -0.06f);
    glEnd();

    // cockpit/window (GL_TRIANGLE_FAN) and outline (GL_LINE_LOOP)
    glColor3f(0.2f, 0.45f, 0.85f);
    drawCircle(0.0f, 0.02f, 0.02f, 20);
    glColor3f(0.02f, 0.02f, 0.02f);
    std::vector<Vec2> wloop;
    for (int i = 0;i <= 20;i++) { float a = i * 2.0f * M_PI / 20.0f; wloop.push_back(Vec2(cos(a) * 0.02f, 0.02f + sin(a) * 0.02f)); }
    drawLineLoop(wloop);

    // antenna lines (GL_LINES)
    glColor3f(0.02f, 0.02f, 0.02f);
    drawLine(0.01f, 0.08f, 0.01f, 0.12f);
    drawLine(0.01f, 0.12f, 0.03f, 0.13f);

    // thruster point (GL_POINTS)
    glPointSize(4.0f); drawPoint(0.0f, -0.11f); glPointSize(1.0f);

    // thruster flame when player just moved (GL_TRIANGLE_FAN)
    if (globalTime - lastMoveTime < 0.25f) {
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(1.0f, 0.6f, 0.0f);
        glVertex2f(0.0f, -0.11f);
        glColor3f(1.0f, 0.2f, 0.0f);
        glVertex2f(-0.03f, -0.18f);
        glVertex2f(0.0f, -0.14f);
        glVertex2f(0.03f, -0.18f);
        glEnd();
    }
}

// =====================
// Rendering UI panels and icons
// Top/bottom panels use GL_QUADS (at least 1 primitive each)
// Health drawn with at least 2 primitives (heart polygon + small circle)
// =====================

void displayText(float x, float y, const std::string& text) { glRasterPos2f(x, y); for (char c : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c); }

void drawTopPanel() {
    // background quad (GL_QUADS)
    glColor3f(0.02f, 0.02f, 0.02f); drawQuad(0.0f, 1.0f - UI_TOP_HEIGHT / 2.0f, 1.0f, UI_TOP_HEIGHT / 2.0f);
    // health: draw hearts (GL_POLYGON) + small inner circles (GL_TRIANGLE_FAN) -> 2 primitives per health
    float sx = -0.9f; float y = 1.0f - UI_TOP_HEIGHT / 2.0f;
    for (int i = 0;i < lives;i++) {
        glColor3f(1.0f, 0.15f, 0.25f); drawHeart(sx + i * 0.08f, y, 0.03f); // GL_POLYGON
        glColor3f(0.8f, 0.2f, 0.3f); drawCircle(sx + i * 0.08f, y - 0.0f, 0.01f, 8); // GL_TRIANGLE_FAN
    }
    // score and time text
    glColor3f(1, 1, 1);
    displayText(-0.05f, 1.0f - UI_TOP_HEIGHT / 2.0f, std::string("Score: ") + std::to_string(score));
    displayText(0.5f, 1.0f - UI_TOP_HEIGHT / 2.0f, std::string("Time: ") + std::to_string((int)gameTimer));
    // active powerup and its timer (if any)
    if (shieldActive) { char buf[64]; sprintf(buf, "Shield: %.1fs", shieldTimer); displayText(0.2f, 1.0f - UI_TOP_HEIGHT / 2.0f, buf); }
    if (speedActive) { char buf2[64]; sprintf(buf2, "Speed: %.1fs", speedTimer); displayText(0.36f, 1.0f - UI_TOP_HEIGHT / 2.0f, buf2); }
}

void drawBottomPanel() {
    // background quad (GL_QUADS)
    glColor3f(0.02f, 0.02f, 0.02f); drawQuad(0.0f, -1.0f + UI_BOTTOM_HEIGHT / 2.0f, 1.0f, UI_BOTTOM_HEIGHT / 2.0f);
    // draw icons for tools (obstacle, collectible, shield, score powerup)
    float y = -1.0f + UI_BOTTOM_HEIGHT / 2.0f;
    float startX = -0.8f; float gap = 0.45f;
    // obstacle icon (GL_QUADS + GL_LINE_LOOP)
    glColor3f(0.6f, 0.3f, 0.2f); drawObstacleIcon(startX, y, 0.06f, 0.04f);
    displayText(startX - 0.04f, y - 0.06f, "Obstacle");
    // collectible icon (GL_TRIANGLES + GL_TRIANGLE_FAN + GL_LINES)
    glColor3f(1.0f, 0.9f, 0.2f); drawCollectibleIcon(startX + gap, y, 0.06f);
    displayText(startX + gap - 0.05f, y - 0.06f, "Collectible");
    // shield icon (GL_POLYGON + GL_LINE_LOOP)
    glColor3f(0.2f, 0.6f, 1.0f); drawShieldIcon(startX + gap * 2, y, 0.06f);
    displayText(startX + gap * 2 - 0.03f, y - 0.06f, "Shield (5s)");
    // score powerup icon (GL_TRIANGLE_STRIP + GL_LINE_STRIP)
    glColor3f(1.0f, 0.9f, 0.2f); drawScorePowerupShape(startX + gap * 3, y, 0.05f);
    displayText(startX + gap * 3 - 0.03f, y - 0.06f, "Speed (5s)");

    // selection highlight (GL_LINE_LOOP)
    float selX = startX + (selectedTool == TOOL_OBSTACLE ? 0 : (selectedTool == TOOL_COLLECTIBLE ? gap : (selectedTool == TOOL_P_SHIELD ? gap * 2 : (selectedTool == TOOL_P_SPEED ? gap * 3 : 0))));
    if (selectedTool != TOOL_NONE) { glColor3f(0.8f, 0.8f, 0.8f); glBegin(GL_LINE_LOOP); glVertex2f(selX - 0.08f, y - 0.05f); glVertex2f(selX + 0.08f, y - 0.05f); glVertex2f(selX + 0.08f, y + 0.05f); glVertex2f(selX - 0.08f, y + 0.05f); glEnd(); }
}

// =====================
// Utility: convert window mouse coords to world coords (excluding UI panels)
// =====================
Vec2 windowToWorld(int mx, int my) {
    float nx = (2.0f * mx / windowWidth) - 1.0f;
    float ny = 1.0f - (2.0f * my / windowHeight);
    return Vec2(nx, ny);
}

bool pointInsideGameArea(const Vec2& p) {
    if (p.x < WORLD_LEFT + 0.02f || p.x > WORLD_RIGHT - 0.02f) return false;
    float topLimit = 1.0f - UI_TOP_HEIGHT;
    float bottomLimit = -1.0f + UI_BOTTOM_HEIGHT;
    if (p.y > topLimit - 0.01f) return false;
    if (p.y < bottomLimit + 0.01f) return false;
    if (p.y < WORLD_BOTTOM || p.y > WORLD_TOP) return false;
    return true;
}

bool tooCloseToExisting(const Vec2& p, float minDist) {
    for (auto& c : collectibles) if (hypot(c.pos.x - p.x, c.pos.y - p.y) < minDist) return true;
    for (auto& o : obstacles) if (hypot(o.pos.x - p.x, o.pos.y - p.y) < minDist) return true;
    for (auto& pu : powerups) if (hypot(pu.pos.x - p.x, pu.pos.y - p.y) < minDist) return true;
    return false;
}

int obstacleIndexAt(float nx, float ny) {
    for (size_t i = 0;i < obstacles.size();++i) { auto& o = obstacles[i]; if (fabs(nx - o.pos.x) < (o.w + 0.04f) && fabs(ny - o.pos.y) < (o.h + 0.04f)) return (int)i; }
    return -1;
}

// =====================
// Mouse handling
// =====================

void mouseClick(int button, int state, int mx, int my) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        Vec2 w = windowToWorld(mx, my);
        float yPanel = -1.0f + UI_BOTTOM_HEIGHT / 2.0f;
        float startX = -0.8f; float gap = 0.45f;
        float epsX = 0.08f, epsY = 0.06f;
        if (fabs(w.y - yPanel) < 0.12f) {
            if (fabs(w.x - startX) < epsX) { selectedTool = TOOL_OBSTACLE; statusMessage = "Obstacle drawing mode"; return; }
            if (fabs(w.x - (startX + gap)) < epsX) { selectedTool = TOOL_COLLECTIBLE; statusMessage = "Collectible drawing mode"; return; }
            if (fabs(w.x - (startX + gap * 2)) < epsX) { selectedTool = TOOL_P_SHIELD; statusMessage = "Shield powerup drawing mode"; return; }
            if (fabs(w.x - (startX + gap * 3)) < epsX) { selectedTool = TOOL_P_SPEED; statusMessage = "Speed powerup drawing mode"; return; }
        }
        if (!gameStarted && selectedTool != TOOL_NONE) {
            if (!pointInsideGameArea(w)) { statusMessage = "Cannot place outside game area"; messageTimer = 2.0f; return; }
            if (!((w.y > playerY) && (w.y < targetPos.y))) { statusMessage = "Place object between player and target"; messageTimer = 2.0f; return; }
            if (tooCloseToExisting(w, 0.08f)) { statusMessage = "Too close to another object"; messageTimer = 2.0f; return; }
            if (selectedTool == TOOL_OBSTACLE) { Obstacle o; o.pos = w; o.w = 0.08f; o.h = 0.06f; obstacles.push_back(o); statusMessage = "Placed obstacle"; }
            else if (selectedTool == TOOL_COLLECTIBLE) { Collectible c; c.pos = w; c.active = true; c.phase = randf(0, 6.28f); collectibles.push_back(c); statusMessage = "Placed collectible"; }
            else if (selectedTool == TOOL_P_SHIELD) { PowerUp p; p.pos = w; p.type = P_SHIELD; p.active = true; p.phase = 0.0f; powerups.push_back(p); statusMessage = "Placed shield powerup"; }
            else if (selectedTool == TOOL_P_SPEED) { PowerUp p; p.pos = w; p.type = P_SPEED; p.active = true; p.phase = 0.0f; powerups.push_back(p); statusMessage = "Placed speed powerup"; }
            messageTimer = 1.5f; return;
        }
    }
}

// =====================
// Keyboard: R starts game, restarts etc.
// =====================
void keyboard(unsigned char key, int x, int y) {
    if (key == 'r' || key == 'R') {
        if (!gameStarted) { // start the game
            gameStarted = true; gameTimer = 30.0f; statusMessage = "Game started"; messageTimer = 1.5f; // <-- start uses 30s
            // ensure powerup state reset when starting
            playerSpeed = basePlayerSpeed; speedActive = false; speedTimer = 0.0f; shieldActive = false; shieldTimer = 0.0f;
        }
        else if (gameOver) { // restart fully
            collectibles.clear(); obstacles.clear(); powerups.clear(); selectedTool = TOOL_NONE; gameStarted = false; gameOver = false; gameWin = false; score = 0; lives = 5; statusMessage = "Editing mode: place objects"; messageTimer = 1.5f; playerX = 0; playerY = -0.9f;
            // reset speed/shield
            playerSpeed = basePlayerSpeed; speedActive = false; speedTimer = 0.0f; shieldActive = false; shieldTimer = 0.0f;
            // reset game timer to 30 as well (editing mode)
            gameTimer = 30.0f;
        }
    }
}

// =====================
// Collision helpers
// =====================

bool collidesWithObstacle(float nx, float ny) { return obstacleIndexAt(nx, ny) != -1; }

int collectAt(float nx, float ny) { for (size_t i = 0;i < collectibles.size();++i) { if (collectibles[i].active && hypot(collectibles[i].pos.x - nx, collectibles[i].pos.y - ny) < 0.07f) { collectibles[i].active = false; return 1; } } return 0; }

int powerupAt(float nx, float ny, PowerUp& out, int& index) { for (size_t i = 0;i < powerups.size();++i) { if (powerups[i].active && hypot(powerups[i].pos.x - nx, powerups[i].pos.y - ny) < 0.07f) { out = powerups[i]; out.active = false; index = (int)i; powerups[i].active = false; return 1; } } return 0; }

// =====================
// Update loop
// =====================
void update(int val) {
    float dt = 0.016f;
    globalTime += dt;

    // animate target along bezier
    if (targetBezier.size() == 4) {
        targetAnimT += dt / 8.0f; if (targetAnimT > 1.0f) targetAnimT -= 1.0f; float t = targetAnimT;
        Vec2 a = targetBezier[0]; Vec2 b = targetBezier[1]; Vec2 c = targetBezier[2]; Vec2 d = targetBezier[3];
        float tt = t; float u = 1 - tt;
        targetPos.x = u * u * u * a.x + 3 * u * u * tt * b.x + 3 * u * tt * tt * c.x + tt * tt * tt * d.x;
        targetPos.y = u * u * u * a.y + 3 * u * u * tt * b.y + 3 * u * tt * tt * c.y + tt * tt * tt * d.y;
    }

    // animate collectibles & powerups (phases)
    for (auto& c : collectibles) c.phase += dt * 2.0f;
    for (auto& p : powerups) p.phase += dt * 1.5f;

    if (gameStarted && !gameOver) {
        // timer
        gameTimer -= dt;
        if (gameTimer <= 0.0f) { // lose unless at target
            if (hypot(playerX - targetPos.x, playerY - targetPos.y) < 0.12f) { gameWin = true; }
            else { gameWin = false; }
            gameOver = true; messageTimer = 3.0f;
        }
        // shield timer
        if (shieldActive) { shieldTimer -= dt; if (shieldTimer <= 0.0f) { shieldActive = false; shieldTimer = 0.0f; statusMessage = "Shield expired"; messageTimer = 1.5f; } }

        // SPEED timer decrement & expiry handling (NEW)
        if (speedActive) {
            speedTimer -= dt;
            if (speedTimer <= 0.0f) {
                speedActive = false;
                speedTimer = 0.0f;
                playerSpeed = basePlayerSpeed;
                statusMessage = "Speed expired";
                messageTimer = 1.5f;
            }
        }
    }

    // message timer
    if (messageTimer > 0.0f) messageTimer -= dt;

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// =====================
// Player controls (special keys)
// =====================
void specialKeys(int key, int x, int y) {
    if (gameOver) return;
    if (!gameStarted) return; // no movement in editing mode

    // compute movement vector based on key, normalize, set angle using atan2 so rocket faces direction of motion
    float dx = 0.0f, dy = 0.0f;
    switch (key) {
    case GLUT_KEY_LEFT:  dx -= 1.0f; break;
    case GLUT_KEY_RIGHT: dx += 1.0f; break;
    case GLUT_KEY_UP:    dy += 1.0f; break;
    case GLUT_KEY_DOWN:  dy -= 1.0f; break;
    default: return;
    }

    if (dx == 0.0f && dy == 0.0f) return;

    // normalize movement vector so diagonal isn't faster
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 0.0f) { dx /= len; dy /= len; }

    // compute angle: atan2(dy,dx) gives 0 = right, +90 = up; our rocket points up at angle 0 -> subtract 90 degrees
    playerAngle = atan2f(dy, dx) * 180.0f / (float)M_PI - 90.0f;

    // attempt move
    float nx = playerX + dx * playerSpeed;
    float ny = playerY + dy * playerSpeed;

    // clamp to game area (world and UI)
    float topLimit = 1.0f - UI_TOP_HEIGHT - 0.02f; float bottomLimit = -1.0f + UI_BOTTOM_HEIGHT + 0.02f;
    if (nx < WORLD_LEFT + 0.02f) nx = WORLD_LEFT + 0.02f; if (nx > WORLD_RIGHT - 0.02f) nx = WORLD_RIGHT - 0.02f;
    if (ny > topLimit) ny = topLimit; if (ny < bottomLimit) ny = bottomLimit;

    int obsIndex = obstacleIndexAt(nx, ny);
    if (obsIndex != -1) {
        if (shieldActive) {
            // shield protects: destroy the obstacle and allow movement
            obstacles.erase(obstacles.begin() + obsIndex);
            score += 5;
            statusMessage = "Shield absorbed obstacle (destroyed)";
            messageTimer = 1.5f;
            playerX = nx; playerY = ny; // move into position
            lastMoveTime = globalTime;
        }
        else {
            // hit obstacle: lose a life and block motion
            lives--; statusMessage = "Hit obstacle! -1 life"; messageTimer = 1.5f; if (lives <= 0) { gameOver = true; gameWin = false; }
            // do not move into obstacle
        }
    }
    else {
        // no obstacle, move freely
        playerX = nx; playerY = ny;
        lastMoveTime = globalTime;
        // collect collectibles
        if (collectAt(playerX, playerY)) { score += 5; statusMessage = "Collected +5"; messageTimer = 0.9f; }
        // powerups
        PowerUp picked; int pindex = -1; if (powerupAt(playerX, playerY, picked, pindex)) {
            if (picked.type == P_SHIELD) { shieldActive = true; shieldTimer = shieldDuration; statusMessage = "Shield picked"; messageTimer = 1.5f; }
            else if (picked.type == P_SPEED) {
                // activate speed for speedDuration seconds
                speedActive = true;
                speedTimer = speedDuration;
                playerSpeed = basePlayerSpeed * speedMultiplier;
                statusMessage = "Speed Up!";
                messageTimer = 1.5f;
            }
            if (pindex >= 0) powerups.erase(powerups.begin() + pindex);
        }
        // win if reach target
        if (hypot(playerX - targetPos.x, playerY - targetPos.y) < 0.12f) { gameWin = true; gameOver = true; }
    }
}

// =====================
// Drawing world: objects placed by user; animate collectibles & powerups; draw obstacles; draw target; background anim
// =====================

void drawBackground() {
    // moving stars background (animated) - use GL_POINTS
    glBegin(GL_POINTS);
    for (int i = 0;i < 80;i++) {
        float sx = -1.0f + (i % 16) * 0.13f + fmod(globalTime * 0.02f + i * 0.01f, 0.2f);
        float sy = -1.0f + (i / 16) * 0.6f + fmod(globalTime * 0.01f * i, 0.4f);
        glVertex2f(sx, sy);
    }
    glEnd();
}

void drawObstacles() {
    for (auto& o : obstacles) {
        glColor3f(0.4f, 0.2f, 0.1f);
        drawQuad(o.pos.x, o.pos.y, o.w, o.h);
        // create loop vertices explicitly using Vec2 constructors to avoid initializer-list ambiguity on some compilers
        std::vector<Vec2> loop = {
            Vec2(o.pos.x - o.w, o.pos.y - o.h),
            Vec2(o.pos.x + o.w, o.pos.y - o.h),
            Vec2(o.pos.x + o.w, o.pos.y + o.h),
            Vec2(o.pos.x - o.w, o.pos.y + o.h)
        };
        glColor3f(0, 0, 0);
        drawLineLoop(loop);
    }
}

void drawCollectibles() {
    for (auto& c : collectibles) if (c.active) { float dy = sin(c.phase) * 0.02f; glColor3f(1.0f, 0.9f, 0.2f); drawStarTriangles(c.pos.x, c.pos.y + dy, 0.03f); glColor3f(1, 1, 1); drawCircle(c.pos.x, c.pos.y + dy, 0.01f, 8); glColor3f(0, 0, 0); drawLine(c.pos.x - 0.02f, c.pos.y + dy, c.pos.x + 0.02f, c.pos.y + dy); }
}

void drawPowerups() {
    for (auto& p : powerups) if (p.active) {
        if (p.type == P_SHIELD) { glColor3f(0.2f, 0.6f, 1.0f); glPushMatrix(); glTranslatef(p.pos.x, p.pos.y, 0); glRotatef(p.phase * 40.0f, 0, 0, 1); drawShieldIcon(0, 0, 0.05f); glPopMatrix(); }
        else { // P_SPEED
            glColor3f(0.8f, 0.2f, 0.9f);
            glPushMatrix(); glTranslatef(p.pos.x, p.pos.y, 0); glRotatef(p.phase * 120.0f, 0, 0, 1);
            // draw a speed icon using triangle strip + line strip (retains primitive requirements)
            drawScorePowerupShape(0, 0, 0.035f);
            glPopMatrix();
            // small arrow point (GL_TRIANGLES) to make it look like speed
            glColor3f(1, 1, 1);
            glBegin(GL_TRIANGLES);
            glVertex2f(p.pos.x + 0.03f, p.pos.y);
            glVertex2f(p.pos.x, p.pos.y + 0.015f);
            glVertex2f(p.pos.x, p.pos.y - 0.015f);
            glEnd();
        }
    }
}

// nicer sun target with glow and rays
void drawSunTarget(float cx, float cy, float radius) {
    const int N = 24;
    // glow layers (GL_TRIANGLE_FAN)
    for (int layer = 3;layer >= 0;--layer) { float r = radius * (0.4f + 0.2f * layer); float alpha = 0.2f + 0.2f * (3 - layer); glColor4f(1.0f, 0.85f - 0.08f * layer, 0.0f, alpha); glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy); for (int i = 0;i <= N;i++) { float a = i * 2 * M_PI / N; glVertex2f(cx + cos(a) * r, cy + sin(a) * r); } glEnd(); }
    // rays (GL_TRIANGLES)
    glBegin(GL_TRIANGLES);
    for (int i = 0;i < 12;i++) { float a = i * 2 * M_PI / 12 + globalTime * 0.5f; float innerR = radius * 1.05f; float outerR = radius * (1.4f + 0.2f * (i % 2)); glColor3f(1, 0.9f, 0.1f); glVertex2f(cx + cos(a) * innerR, cy + sin(a) * innerR); glVertex2f(cx + cos(a + 0.08f) * outerR, cy + sin(a + 0.08f) * outerR); glVertex2f(cx + cos(a - 0.08f) * outerR, cy + sin(a - 0.08f) * outerR); }
    glEnd();
    // core (GL_TRIANGLE_FAN)
    glColor3f(1, 1, 0.6f); drawCircle(cx, cy, radius * 0.6f, 20);
}

// =====================
// main display
// =====================

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // draw background (screen space)
    glColor3f(0.02f, 0.02f, 0.05f); drawQuad(0, 0, 1.0f, 1.0f);
    glColor3f(1, 1, 1); glPointSize(2.0f);
    drawBackground();
    glPointSize(1.0f);

    // draw UI panels
    glColor3f(0.1f, 0.1f, 0.12f); drawTopPanel(); drawBottomPanel();

    // draw world objects
    drawSunTarget(targetPos.x, targetPos.y, 0.06f);
    drawObstacles();
    drawCollectibles();
    drawPowerups();

    // draw player (animated rotation is visualized via antenna lines orientation using playerAngle)
    glPushMatrix();
    glTranslatef(playerX, playerY, 0);
    glRotatef(playerAngle, 0, 0, 1);
    drawPlayer();
    glPopMatrix();

    // draw status messages
    if (messageTimer > 0.0f) { glColor3f(1, 1, 1); displayText(-0.4f, -0.85f + UI_BOTTOM_HEIGHT, statusMessage); }

    if (gameOver) { glColor3f(1, 1, 1); displayText(-0.12f, 0.0f, gameWin ? "YOU WIN!" : "GAME OVER"); displayText(-0.15f, -0.1f, std::string("Final Score: ") + std::to_string(score)); displayText(-0.25f, -0.2f, "Press R to Restart (returns to editor)"); }

    glutSwapBuffers();
}

// =====================
// Initialization and main
// =====================

void initGame() {
    srand((unsigned)time(0));
    // prepare bezier control points for target within visible frame
    float left = -0.6f, right = 0.6f, y = 0.7f;
    targetBezier.clear(); targetBezier.push_back(Vec2(left, y)); targetBezier.push_back(Vec2(-0.2f, y + 0.3f)); targetBezier.push_back(Vec2(0.2f, y - 0.3f)); targetBezier.push_back(Vec2(right, y));
    targetAnimT = 0.0f;
    // clear editor arrays
    collectibles.clear(); obstacles.clear(); powerups.clear(); selectedTool = TOOL_NONE;
    playerX = 0; playerY = -0.9f; score = 0; lives = 5; gameStarted = false; gameOver = false; shieldActive = false; shieldTimer = 0.0f; statusMessage = "Editing mode: place objects"; messageTimer = 2.0f;
    // reset player speed state
    playerSpeed = basePlayerSpeed; speedActive = false; speedTimer = 0.0f;
    // ensure game timer default is 30
    gameTimer = 30.0f;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv); glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB); glutInitWindowSize(windowWidth, windowHeight); glutCreateWindow("Space Editor - Place Objects then Press R");
    glutDisplayFunc(display); glutTimerFunc(16, update, 0); glutMouseFunc(mouseClick); glutKeyboardFunc(keyboard); glutSpecialFunc(specialKeys);
    glClearColor(0, 0, 0, 1);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    initGame(); glutMainLoop(); return 0;
}
