#include <raylib.h>
#include <stdbool.h>

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
  float brightness_intensity;
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
void handle_crop_event(ImageObject *image);
PosSize set_dynamic_position(float x, float y, float width, float height);
Rectangle set_dynamic_position_rect(float x, float y, float width,
                                    float height);
void update_and_reflect_image_changes(ImageObject *image);
// global variables (used globally)

CustomCanvas canvas = {{0}};

int main() {
  ImageObject image;
  bool close_window = false;
  bool draw_window_close_confirm_dialog = false;
  bool draw_info_dialog = false;
  bool draw_error_dialog = false;
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
    GuiToggle(set_dynamic_position_rect(36, 1, 10, 5), "#99#Crop",
              &image.start_crop);
    handle_crop_event(&image);

    // undo changes button
    if (GuiButton((Rectangle){GetScreenWidth() - 128, 1, 30, 30}, "#211#")) {
      image.brightness_intensity = 0;
      image.blur_intensity = 0;
      image.snap_pixels = false;
      image.start_crop = false;

      if (image.isLoaded) {
        handle_dynamic_canvas_resizing(&image);
      }
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
  ImageBlurGaussian(&image->img_copy, image->blur_intensity);
  ImageColorBrightness(&image->img_copy, image->brightness_intensity);
}
