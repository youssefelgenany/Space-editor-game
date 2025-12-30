# Space Editor Game 

## Overview

**Space Editor** is a 2D OpenGL-based game that combines **level editing** and **real-time gameplay** into a single interactive experience. The player first designs a custom level by placing obstacles, collectibles, and power-ups, then starts the game and attempts to navigate a rocket to a moving target within a limited time.

The project is implemented in **C++ using legacy OpenGL (immediate mode)** and the **GLUT** framework. It serves both as a playable game and as a demonstration of core computer graphics concepts, including rendering primitives, transformations, animation, collision detection, and game-state management.

---

## Features

### Dual-Mode Gameplay

**Editing Mode**
- Place obstacles, collectibles, and power-ups using the mouse
- Tool selection via a visual bottom UI panel
- Placement rules prevent invalid or overlapping objects

**Play Mode**
- Real-time keyboard-controlled movement
- Timer-based gameplay
- Scoring, lives, and win/lose conditions

---

### Core Mechanics

**Player (Rocket)**
- Rotates to face movement direction
- Normalized movement (no diagonal speed advantage)
- Thruster animation when moving

**Obstacles**
- Block movement
- Reduce lives unless shield power-up is active

**Collectibles**
- Increase score when collected
- Floating animation

**Power-Ups**
- **Shield**: Absorbs and destroys obstacles for 5 seconds
- **Speed Boost**: Increases movement speed for 5 seconds

**Target**
- Animated sun-like goal
- Moves continuously along a cubic Bézier curve

---

## Controls

### Editing Mode

| Action | Input |
|------|------|
| Select tool | Mouse click on bottom panel |
| Place object | Left mouse click in game area |
| Start game | R |

### Play Mode

| Action | Input |
|------|------|
| Move up | Up Arrow |
| Move down | Down Arrow |
| Move left | Left Arrow |
| Move right | Right Arrow |
| Restart (after game over) | R |

---

## User Interface

**Top Panel**
- Player lives (heart icons)
- Score
- Remaining time
- Active power-up timers

**Bottom Panel**
- Tool icons:
  - Obstacle
  - Collectible
  - Shield power-up
  - Speed power-up

**Status Messages**
- Feedback for actions such as placement, collisions, and power-up activation

---

## Win and Lose Conditions

**Win**
- Reach the animated target before the timer expires

**Lose**
- Timer reaches zero without reaching the target
- Player loses all lives

---

## Graphics and Rendering

This project intentionally uses **legacy OpenGL (immediate mode)** to demonstrate fundamental graphics concepts.

### OpenGL Primitives Used

- GL_QUADS  
- GL_TRIANGLES  
- GL_TRIANGLE_FAN  
- GL_TRIANGLE_STRIP  
- GL_LINES  
- GL_LINE_LOOP  
- GL_LINE_STRIP  
- GL_POINTS  
- GL_POLYGON  

### Techniques Demonstrated

- 2D transformations (translation and rotation)
- Alpha blending for glow effects
- Bézier curve animation
- Layered UI rendering
- Primitive-based icon design
- Fixed-timestep update loop (approximately 60 FPS)

---

## Technical Details

- **Language:** C++
- **Graphics API:** OpenGL (Immediate Mode)
- **Windowing/Input:** GLUT
- **Architecture:**
  - Timer-driven game loop using `glutTimerFunc`
  - State-based logic (editing, playing, game over)
  - Distance-based collision detection
- **Default Game Time:** 30 seconds

---

## How to Run the Project (Visual Studio)

This project is provided as a **pre-configured Visual Studio solution**.

### Steps to Execute

1. Open the file: OpenGL3DTemplate.sln
2. Visual Studio will load the solution with all required settings.
3. Make sure the build configuration is set to:
4. Click **Local Windows Debugger** or press: F5
5. The game window will launch automatically.

No manual compilation, linking, or library configuration is required.

---
