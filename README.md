# CLAFORD - clang-format-daemon

## Build

Run these scripts:
```
./0_make_deps.sh
./1_config.sh # Optional args passed to cmake, e.g. ./1_config.sh -G Xcode
./2_make.sh
```

## Usage

```
./i/bin/claford <dir-to-watch>
```

The program starts watching the specified directory and applies `clang-format -i -style=file ...` if a file gets modified.

When you save a file in your text editor, which then gets formatted immediately by claford, the text editor might fail to detect that the file has been changed. With Xcode it happens all the time. That's why there's a 1 second delay before each clang-format call.
