#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

#define GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION
#include "gui_window_file_dialog.h"

// intermediate image object type declaration
typedef struct {
  Image image;
  Image img_copy;
  char *path;
  char *extension;
  bool isLoaded;
  bool snap_pixels;
  Vector2 initial_size;
  bool start_crop;
  float blur_intensity;
} ImageObject;

typedef struct {
  Texture texture;
  Vector2 size;
  Vector2 position;
  bool selected;
  Vector2 mouseCell;
} CustomCanvas;

// things defined using this are usually in percentage
// eg x=45% of GetScreenWidth()
typedef struct {
  int x;
  int y;
  int width;
  int height;
} PosSize;

// function declarations
bool close_window_dialog(bool *draw);
void info_dialog(bool *draw);
void error_dialog(bool *draw,
                  char *message); // issue with error dialog. TODO: fix later
void load_new_image(ImageObject *image, char *filename, bool *show_error_popup,
                    char *error_message);
void load_texture(ImageObject *image);
void handle_dynamic_canvas_resizing(ImageObject *image);
void handle_crop_event(ImageObject *image);
PosSize set_dynamic_position(int x, int y, int width, int height);
Rectangle set_dynamic_position_rect(int x, int y, int width, int height);
// global variables (used globally)

CustomCanvas canvas = {{0}};

int main() {
  ImageObject image;
  bool close_window = false;
  bool draw_window_close_confirm_dialog = false;
  bool draw_info_dialog = false;
  bool draw_error_dialog = false;
  char error_message[1024] = {0};

  InitWindow(700, 500, "Image Editor");
  SetTargetFPS(60);

  GuiWindowFileDialogState file_dialog_state =
      InitGuiWindowFileDialog(GetWorkingDirectory());

  file_dialog_state.windowBounds.x = (GetScreenWidth() / 2.f) - 200;
  file_dialog_state.windowBounds.y = (GetScreenHeight() / 2.f) - 150;

  while (!close_window) {
    canvas.size = (Vector2){GetScreenWidth() / 2.f, GetScreenHeight() / 2.f};
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
    } else {
      GuiGrid((Rectangle){canvas.position.x, canvas.position.y, canvas.size.x,
                          canvas.size.y},
              "No Image", 10, 1, &canvas.mouseCell);
    }

    if (GuiButton(set_dynamic_position_rect(1, 1, 20, 5), "#12#Open Image")) {
      file_dialog_state.windowActive = true;
    }

    GuiCheckBox(set_dynamic_position_rect(22, 2, 3, 4), "Pixel Perfect",
                &image.snap_pixels);

    // handle cropping
    GuiToggle(set_dynamic_position_rect(36, 1, 20, 5), "#99#Crop",
              &image.start_crop);
    handle_crop_event(&image);

    if (GuiButton((Rectangle){GetScreenWidth() - 32, 1, 30, 30}, "#193#")) {
      draw_info_dialog = true;
    }

    if (GuiButton((Rectangle){GetScreenWidth() - 65, 1, 30, 30}, "#2#")) {
    }

    info_dialog(&draw_info_dialog);
    error_dialog(&draw_error_dialog, error_message);

    if (file_dialog_state.SelectFilePressed) {
      file_dialog_state.SelectFilePressed = false;
      const char *filename =
          TextFormat("%s" PATH_SEPERATOR "%s", file_dialog_state.dirPathText,
                     file_dialog_state.fileNameText);
      load_new_image(&image, (char *)filename, &draw_error_dialog,
                     error_message);
      handle_dynamic_canvas_resizing(&image);
    }

    // blur slider
    GuiSlider(set_dynamic_position_rect(4, 20, 15, 5), "Blur", "",
              &image.blur_intensity, 0, 10);
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
  }
  UnloadTexture(canvas.texture);
  CloseWindow();
  return 0;
}

void handle_crop_event(ImageObject *image) {
  Rectangle rect = {canvas.position.x, canvas.position.y, canvas.size.x,
                    canvas.size.y};

  if (image->start_crop) {
    DrawRectangleLinesEx(rect, 2, BLACK);
  }
}

void load_texture(ImageObject *image) {
  UnloadTexture(canvas.texture);
  canvas.texture = LoadTextureFromImage(image->image);
}

void handle_dynamic_canvas_resizing(ImageObject *image) {
  if (image->snap_pixels) {
    ImageResizeNN(&(image->image), canvas.size.x, canvas.size.y);
  } else {
    ImageResize(&(image->image), canvas.size.x, canvas.size.y);
  }

  ImageBlurGaussian(&image->image, image->blur_intensity);

  printf("%d\n", image->snap_pixels);

  UnloadTexture(canvas.texture);
  canvas.texture = LoadTextureFromImage(image->image);
}

void load_new_image(ImageObject *image, char *filename, bool *show_error_popup,
                    char *error_message) {
  if (IsFileExtension(filename, ".png") || IsFileExtension(filename, ".jpeg") ||
      IsFileExtension(filename, ".jpg")) {

    if (image->isLoaded) {
      UnloadImage(image->image);
    }
    image->image = LoadImage(filename);
    // image->img_copy = image->image;
    image->path = filename;
    image->isLoaded = true;
    image->initial_size = (Vector2){image->image.width, image->image.height};
  } else {
    *show_error_popup = true;
    error_message = "Unsupported filetype!";
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
        rect, "Info", "Image Editor v 0.1. Created by @lordryns on X", "OK");

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

PosSize set_dynamic_position(int x, int y, int width, int height) {
  PosSize pos_size;
  pos_size.x = (GetScreenWidth() / 100) * x;
  pos_size.y = (GetScreenHeight() / 100) * y;
  pos_size.width = (GetScreenWidth() / 100) * width;
  pos_size.height = (GetScreenHeight() / 100) * height;

  return pos_size;
}

Rectangle set_dynamic_position_rect(int x, int y, int width, int height) {
  PosSize e = set_dynamic_position(x, y, width, height);
  return (Rectangle){e.x, e.y, e.width, e.height};
}
