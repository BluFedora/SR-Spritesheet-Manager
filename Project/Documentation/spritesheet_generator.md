# SR Sprite Sheet Manager (SRSM)

This program is designed to help manage making sprite sheet texture atlases with associated animations.

## Features
- Packing multiple images into one big atlas for efficient rendering.
- Previewing the animations live in the tool.


## Usage

When you first open the program you should be greeted to a Welcome screen.

![Screenshot of Main Window](sg_screenshot.png)

## Keyboard Shortcuts

> Ctrl+S - Saves the current Sprite Sheet project.

## Tech Details

Actions that can happen in the program:

- Add Frame From Library To Animation
- Reorder frame
- Resize Frame Timing
- Rename Project
- Save Project
- Add Folder to Image Library
- Import Images to Image Library
- Change Quality Settings
  - Spritesheet Size
  - Frame Size

### Binary File Format (.srsm.bytes)

> This format in encoded in Little Endian byte order.

> Two's compliment is used for any signed integers

> All floats are in the IEEE-754 Floating point format.

> This file format may change with each version update.

#### Spec v0

```cpp
// File Outline
//
// Header header;
// Chunk  chunks[header.num_chunks];
//

// Main Data Structures

struct Header {
  /* off:  0 */ uint32 magic;        // Is required to the the ASCII bytes "SRSM".
  /* off:  4 */ uint16 data_offset;  // The offset to move by to get to the data.
  /* off:  6 */ uint8  version;      // The current version of the file format. (must be 0)
  /* off:  7 */ uint8  num_chunks;   // Number of chunks contained in the binary.
  /* off:  8 */ uint16 atlas_width;  // The width of the spritesheet image.
  /* off: 10 */ uint16 atlas_height; // The height of the spritesheet image.
}

struct Chunk {
  uint8  chunk_type[4];                 // The name of the data chunk for identifying for to read it.
  uint32 chunk_data_length;             // The amount of data in the 'chunk_data' field.
  uint8  chunk_data[chunk_data_length]; // To be interpreted based on the 'chunk_type'.
}

// Chunk Types

struct ChunkFramesData /* chunk_type = "FRME" */ {
  uint32    num_frames;
  FrameData frames[num_frames];
}

struct ChuckAnimData /* chunk_type = "ANIM" */ {
  uint32        num_animations;
  AnimationData name[num_animations];
}

struct ChunkLiveEditData /* chunk_type = "EDIT" */ {
  uint8 uuid[37]; /*!< Nul terminated uuid string. 'xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0' */
}

// Optional Chunk used to indicate the end of the file.
struct FooterChunkData /* chunk_type = "FOOT" */ {
  /* NO DATA */
}

// Helper Structures

struct FrameData {
  uint32 image_x;      // units = pixels
  uint32 image_y;      // units = pixels
  uint32 image_width;  // units = pixels
  uint32 image_height; // units = pixels
}

struct FrameInstanceData {
  uint32  frame_index; // Index into the "FRME" Chunk.
  float32 frame_time;  // unit = seconds.
}

struct AnimationData {
  uint32            name_length;
  uint8             name[name_length + 1]; // Nul terminated.
  uint32            num_frames;
  FrameInstanceData frames[num_frames];
}
```

References:
  - (Designing File Formats)[https://www.fadden.com/tech/file-formats.html]

---

# OLD DOCS
* This program is designed to help with the creation of spritesheets for use in the engine.
* When you start the program is assumes a new Spritesheet so just start importing files (or open an existing Spritesheet).

## Features
* The "Spritesheet Size" sets the total width of the spritesheet, the default is fine.
* The "Frame Size" sets the size of each frame in the animation, the default is fine.
* The "Image Library" is a list of all images associated with a particular character.
  - To add images you can either:
    1) Click the "Import Images" Button
    2) Drag a folder with images to batch import.
    3) or Drag images over the list.
    - (Files are never added to the list twice so don't worry about duplicates)
  - To remove some images select them and press the delete or backspace key.
    - Some animations may become invalid if you delete images used by those animations, tbh I don't know if that would crash the Engine / Spritesheet Generator when loading the animation / spritesheet.
* The "Animation List" is the list of Animations that have been set up for this character.
  - To add an animation click the "Add Animation" button.
    - From there you can name it and set the framerate.
  - To remove an animation right-click and click "Remove".
* The "Frame List" shows the list of frame (in order of being displayed) of the currently selected animation.
  - To add frames drag from the "Image Library" into the list.
  - Frames can be reordered by dragging and dropping.
  - To remove some frames select them and press the delete or backspace key.

# TODO List

## Required for Release

[x] Deleting folders / images needs to recursively check children.
[x] Deleting folders / images should be accessible from r-click and as a menu item (+ thus a shortcut).
[x] Selecting an animation should show a preview.
  [x] An empty animation should show an image indicating there are no frames.
[x] Rename Animations
[x] Deleting of frames needs to be put in a right click menu.
[x] Moving the timeline scrubber.
[x] Mouse wheel to zoom
  [x] and scroll the Timeline View.
[x] Exporting a Spritesheet
[x] Sending out packets lol (pretty fundamental)
[ ] Document ALL of this...

## Would be nice but not important

[?] Rearrange Animations around
[?] Playback speed
[x] Hover rect bugs, moving the mouse too fast over the tl will do weird stuff. plus it should be cleared on animation select.
[ ] Make an animation not loop when playing.
[ ] Clearing the recent spritesheet list.
[ ] The zoom for the timeline does not correspond to mouse wheel.

# Runtimes I Should Support:

[ ] > Unity
[ ] > Cocos2D-X
[ ] > Cocos2D
[ ] > SpriteKit
[ ] > Starling
[ ] > Sparrow
[ ] > LibGDX
[ ] > Moai
[ ] > V-Play
[ ] > Corona(TM) SDK
[ ] > Phaser
[ ] > MelonJS
[ ] > Monogame
[ ] > HTML5 / CSS sprites
[ ] > Custom C/++
