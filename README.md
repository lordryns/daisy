# daisy
A lightweight image editor built in C

Daisy uses Raylib for window management and image processing and might as well just be a gui wrapper for the library. Of course I do plan on introducing its own special twist but for now, I'm focusing on giving it all the features You'd expect from a text editor while keeping it lightweight and fast.

Daisy is still under active development and new features are being added daily 

### New features (updated daily):

- "Undo all changes" button
- Brightness/Darkness
- Blur 
- Pixel perfect 
- Image loading


### How to install ? 
There are two ways of using Daisy (or at least there should be). The first are the pre-built binaries that'll be provided by the end of 0.1-stable but for now option 2 is the only viable solution 

#### Build it yourself ! 
You can build this entire application by yourself as long as you have a C compiler installed.

Then clone this repo and inside:

Install raylib.h either by cloning the repo or installing from a package manager 

Then build using:

```bash 
clang main.c -o app -lraylib -lm
```

This should build the application successfully and provide you with a `./app` or `app.exe` depending on your platform.
@lordryns on X in case you care.
