#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

#define GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION
#include "gui_window_file_dialog.h"

typedef struct {
  char *text;
  Vector2 position;
} TextObject;

typedef struct {
  TextObject *buffer;
  size_t capacity;
  size_t index_size;
  int index;
} TextAllocator;

// intermediate image object type declaration
typedef struct {
  Image image;
  Image img_copy;
  char *path;
  char *extension;
  bool isLoaded;
  bool snap_pixels;
  Vector2 initial_size;
  float blur_intensity;
  float brightness_intensity;
  TextAllocator text_allocator;
} ImageObject;

typedef struct {
  Texture texture;
  Vector2 size;
  Vector2 position;
  bool selected;
  Vector2 mouseCell;
  Rectangle context;
} CustomCanvas;

// things defined using this are usually in percentage
// eg x=45% of GetScreenWidth()
typedef struct {
  float x;
  float y;
  float width;
  float height;
} PosSize;

// function declarations
bool close_window_dialog(bool *draw);
void info_dialog(bool *draw);
void error_dialog(bool *draw,
                  char *message); // issue with error dialog. TODO: fix later
void load_new_image(ImageObject *image, char *filename);

void load_texture(ImageObject *image);
void handle_dynamic_canvas_resizing(ImageObject *image);
PosSize set_dynamic_position(float x, float y, float width, float height);
Rectangle set_dynamic_position_rect(float x, float y, float width,
                                    float height);
void update_and_reflect_image_changes(ImageObject *image);
TextAllocator new_text_allocator(int capacity);
void append_to_text_allocator(TextAllocator *alloc, TextObject tobject);
void handle_context_state(ImageObject *image);
// global variables (used globally)

// there should be only one instance of the canvas, this canvas.
CustomCanvas canvas = {{0}};

int main() {
  ImageObject image = {0};
  image.text_allocator = new_text_allocator(2);
  bool close_window = false;
  bool draw_window_close_confirm_dialog = false;
  bool draw_info_dialog = false;
  bool draw_error_dialog = false;
  bool draw_add_text_dialog = false;
  char add_text_dialog_text[40] = "";
  char error_message[1024] = {0};
  float last_blur_change = 0.f;
  float last_brightness_change = 0.f;
  bool last_pixel_snap_change = false;

  InitWindow(700, 500, "Daisy v0.1");
  SetTargetFPS(60);

  GuiWindowFileDialogState file_dialog_state =
      InitGuiWindowFileDialog(GetWorkingDirectory());

  while (!close_window) {
    canvas.size = (Vector2){GetScreenWidth() / 2.f, GetScreenHeight() / 2.f};
    file_dialog_state.windowBounds = set_dynamic_position_rect(25, 20, 50, 60);
    if (WindowShouldClose())
      draw_window_close_confirm_dialog = true;
    BeginDrawing();
    ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
    canvas.position.x = (GetScreenWidth() / 2.f) / 2.f;
    canvas.position.y = (GetScreenHeight() / 2.f) / 2.f;

    // handling texture drawing and resizing
    if (image.isLoaded) {
      DrawTexture(canvas.texture, canvas.position.x, canvas.position.y, WHITE);
      if (IsWindowResized()) {
        handle_dynamic_canvas_resizing(&image);
      }

      static float blur_timer = 0.f;
      blur_timer += GetFrameTime();
      if (image.blur_intensity != last_blur_change && blur_timer > 1.f) {
        handle_dynamic_canvas_resizing(&image);
        last_blur_change = image.blur_intensity;
        blur_timer = 0.f;
      }

      static float brightness_timer = 0.f;
      brightness_timer += GetFrameTime();
      if (image.brightness_intensity != last_brightness_change &&
          brightness_timer > 1.f) {
        handle_dynamic_canvas_resizing(&image);
        last_brightness_change = image.brightness_intensity;
        brightness_timer = 0.f;
      }

      if (image.snap_pixels != last_pixel_snap_change) {
        handle_dynamic_canvas_resizing(&image);
        last_pixel_snap_change = image.snap_pixels;
      }
    } else {
      GuiGrid((Rectangle){canvas.position.x, canvas.position.y, canvas.size.x,
                          canvas.size.y},
              "No Image", 10, 1, &canvas.mouseCell);
    }

    if (GuiButton(set_dynamic_position_rect(1, 1, 20, 5), "#12#Open Image")) {
      file_dialog_state.windowActive = true;
    }

    GuiCheckBox(set_dynamic_position_rect(22, 2.9, 2, 2.8), "Pixel Perfect",
                &image.snap_pixels);

    // handle cropping
    GuiButton(set_dynamic_position_rect(36, 1, 10, 5), "#99#Crop");

    if (image.isLoaded)
      // text button
      if (GuiButton(set_dynamic_position_rect(47, 1, 11, 5), "#30#Add Text")) {
        draw_add_text_dialog = true;
      }

    if (draw_add_text_dialog) {

      int res = GuiTextInputBox(set_dynamic_position_rect(30, 30, 40, 40),
                                "Add Text", "Add text to display", "Cancel;Add",
                                add_text_dialog_text,
                                sizeof(add_text_dialog_text), NULL);

      switch (res) {
      case 0:
        draw_add_text_dialog = false;
        break;
      case 1:
        draw_add_text_dialog = false;
        break;
      case 2:
        draw_add_text_dialog = false;
        if (image.isLoaded) {
          append_to_text_allocator(
              &image.text_allocator,
              (TextObject){strdup(add_text_dialog_text),
                           (Vector2){GetRandomValue(0, canvas.size.x),
                                     GetRandomValue(0, canvas.size.y)}});

          handle_dynamic_canvas_resizing(&image);
          strcpy(add_text_dialog_text, "");
        }
        break;
      }
    }
    // undo changes button
    if (GuiButton((Rectangle){GetScreenWidth() - 128, 1, 30, 30}, "#211#")) {
      image.brightness_intensity = 0;
      image.blur_intensity = 0;
      image.snap_pixels = false;
      free(image.text_allocator.buffer);
      image.text_allocator = new_text_allocator(2);

      if (image.isLoaded) {
        handle_dynamic_canvas_resizing(&image);
      }
    }

    handle_context_state(&image);
    if (IsWindowResized()) {
      canvas.context = (Rectangle){0, 0, 0, 0};
    }
    // save button
    if (GuiButton((Rectangle){GetScreenWidth() - 97, 1, 30, 30}, "#2#")) {
    }

    // settings button
    if (GuiButton((Rectangle){GetScreenWidth() - 65, 1, 30, 30}, "#142#")) {
    }

    // help button
    if (GuiButton((Rectangle){GetScreenWidth() - 32, 1, 30, 30}, "#193#")) {
      draw_info_dialog = true;
    }

    info_dialog(&draw_info_dialog);
    error_dialog(&draw_error_dialog, error_message);

    if (file_dialog_state.SelectFilePressed) {
      file_dialog_state.SelectFilePressed = false;
      const char *filename =
          TextFormat("%s" PATH_SEPERATOR "%s", file_dialog_state.dirPathText,
                     file_dialog_state.fileNameText);
      load_new_image(&image, (char *)filename);
    }

    // blur slider
    GuiLabel(set_dynamic_position_rect(1, 17, 15, 3), "Blur");
    GuiSlider(set_dynamic_position_rect(1, 20, 20, 5), "", "",
              &image.blur_intensity, 0, 20);

    // brightness slider
    GuiLabel(set_dynamic_position_rect(1, 27, 15, 3), "Brightness");
    GuiSlider(set_dynamic_position_rect(1, 30, 20, 5), "", "",
              &image.brightness_intensity, -100, 100);

    // handling closing of application (dialog and state)
    // triggered by the WindowShouldClose() event

    GuiWindowFileDialog(&file_dialog_state);
    if (draw_window_close_confirm_dialog) {
      close_window = close_window_dialog(&draw_window_close_confirm_dialog);
    }

    EndDrawing();
  }

  if (image.isLoaded) {
    UnloadImage(image.image);
    UnloadImage(image.img_copy);
  }
  UnloadTexture(canvas.texture);
  CloseWindow();
  free(image.text_allocator.buffer);
  return 0;
}

// global variables (bad practice i know)
Vector2 start_context_pos;
Vector2 end_context_pos;

// chatgpt helped with the creation of this function
void handle_context_state(ImageObject *image) {
  Rectangle n_canvas = {canvas.position.x, canvas.position.y, canvas.size.x,
                        canvas.size.y};
  Vector2 mouse_pos = GetMousePosition();
  DrawRectangleRec(canvas.context, Fade(BLACK, 0.2f));
  DrawRectangleLinesEx(canvas.context, 2, BLACK);

  if (!CheckCollisionPointRec(mouse_pos, n_canvas)) {
    return;
  }
  // printf("rect y: %f\n", rect.y);
  mouse_pos.x = fmaxf(canvas.position.x,
                      fminf(mouse_pos.x, canvas.position.x + canvas.size.x));
  mouse_pos.y = fmaxf(canvas.position.y,
                      fminf(mouse_pos.y, canvas.position.y + canvas.size.y));

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    start_context_pos = mouse_pos;
  }

  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    end_context_pos = mouse_pos;

    float startX =
        fmaxf(canvas.position.x, fminf(start_context_pos.x, end_context_pos.x));
    float startY =
        fmaxf(canvas.position.y, fminf(start_context_pos.y, end_context_pos.y));
    float endX = fminf(canvas.position.x + canvas.size.x,
                       fmaxf(start_context_pos.x, end_context_pos.x));
    float endY = fminf(canvas.position.y + canvas.size.y,
                       fmaxf(start_context_pos.y, end_context_pos.y));

    canvas.context.x = startX;
    canvas.context.y = startY;
    canvas.context.width = endX - startX;
    canvas.context.height = endY - startY;
  }
}

void load_texture(ImageObject *image) {
  UnloadTexture(canvas.texture);
  canvas.texture = LoadTextureFromImage(image->img_copy);
}

void handle_dynamic_canvas_resizing(ImageObject *image) {
  update_and_reflect_image_changes(image);
  if (image->snap_pixels) {
    ImageResizeNN(&(image->img_copy), canvas.size.x, canvas.size.y);
  } else {
    ImageResize(&(image->img_copy), canvas.size.x, canvas.size.y);
  }

  load_texture(image);
}

void load_new_image(ImageObject *image, char *filename) {
  if (IsFileExtension(filename, ".png") || IsFileExtension(filename, ".jpeg") ||
      IsFileExtension(filename, ".jpg")) {

    if (image->isLoaded) {
      UnloadImage(image->image);
      UnloadImage(image->img_copy);
    }
    image->image = LoadImage(filename);
    image->img_copy = ImageCopy(image->image);
    image->path = filename;
    image->isLoaded = true;
    image->initial_size = (Vector2){image->image.width, image->image.height};
    handle_dynamic_canvas_resizing(image);
  } else {
    // error message was causing segmentation fault so i removed it for now
  }
}

void error_dialog(bool *draw, char *message) {
  Rectangle rect = {(GetScreenWidth() / 2.f) - 120,
                    (GetScreenHeight() / 2.f) - 50, 270, 150};

  if (*draw) {
    int response = GuiMessageBox(rect, "#128#Error",
                                 TextFormat("Error: %s", message), "Close");

    switch (response) {
    case 0:
      *draw = false;
      break;
    case 1:
      *draw = false;
    }
  }
}

void info_dialog(bool *draw) {
  Rectangle rect = {(GetScreenWidth() / 2.f) - 120,
                    (GetScreenHeight() / 2.f) - 50, 270, 150};

  if (*draw) {
    int response = GuiMessageBox(
        rect, "Info", "Daisy image editor v 0.1. lordryns/daisy on github",
        "OK");

    switch (response) {
    case 0:
      *draw = false;
      break;
    case 1:
      *draw = false;
    }
  }
}

bool close_window_dialog(bool *draw) {
  bool close = false;

  Rectangle rect = {(GetScreenWidth() / 2.f) - 120,
                    (GetScreenHeight() / 2.f) - 50, 270, 150};

  if (*draw) {
    int response = GuiMessageBox(
        rect, "Close Application?",
        "Are you sure you want to close this application?", "Stay;Close");

    switch (response) {
    case 0:
      *draw = false;
      break;
    case 1:
      *draw = false;
      break;
    case 2:
      close = true;
      break;
    }
  }
  return close;
}

PosSize set_dynamic_position(float x, float y, float width, float height) {
  PosSize pos_size;
  pos_size.x = (GetScreenWidth() / 100.f) * x;
  pos_size.y = (GetScreenHeight() / 100.f) * y;
  pos_size.width = (GetScreenWidth() / 100.f) * width;
  pos_size.height = (GetScreenHeight() / 100.f) * height;

  return pos_size;
}

Rectangle set_dynamic_position_rect(float x, float y, float width,
                                    float height) {
  PosSize e = set_dynamic_position(x, y, width, height);
  return (Rectangle){e.x, e.y, e.width, e.height};
}

void update_and_reflect_image_changes(ImageObject *image) {
  UnloadImage(image->img_copy);
  image->img_copy = ImageCopy(image->image);
  for (int i = 0; i < image->text_allocator.index; i++) {
    TextObject current = image->text_allocator.buffer[i];
    ImageDrawText(&image->img_copy, current.text, current.position.x,
                  current.position.y, 40, BLACK);
  }

  ImageBlurGaussian(&image->img_copy, image->blur_intensity);
  ImageColorBrightness(&image->img_copy, image->brightness_intensity);
}

TextAllocator new_text_allocator(int capacity) {
  return (TextAllocator){malloc(capacity * sizeof(TextObject)), capacity,
                         sizeof(TextObject), 0};
}
void append_to_text_allocator(TextAllocator *alloc, TextObject tobject) {
  if (alloc->index > alloc->capacity - 1) {
    alloc->capacity *= 2;
    alloc->buffer = realloc(alloc->buffer, alloc->index_size * alloc->capacity);
  }

  alloc->buffer[alloc->index] = tobject;
  alloc->index += 1;
}
