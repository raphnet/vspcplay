# vspcplay, a visual spc player and optimisation tool

![vspcplay screenshot](/screenshots/playing.png?raw=true "vspcplay")

## Overview

This is vspcplay, a visual spc player and optimisation tool.

I used the code from the spc-xmms plugin, which uses the snes9x APU emulation
code. I updated the snes9x code using a newer version of snes9x (I dont remember
which version exactly).

The display code is mine.

** WARNING **
I coded this really fast and added feature after feature without any planning and minimal
thinking. The code is really *really* ugly.


## Use (graphical)

vspcplay does not have a built-in file browser, so you must pass one or more files
on the command line to start it. For instance, to listen to all the .SPC files in 
subdirectory music, you would use this command:

`./vspcplay music/*.spc`

Since version 1.4, the following keyboard controls are available:

 - `N` `P` : Play next / previous song
 - `R` : Restart current song
 - `ESC` : Quit program
 - `1` to `8` : Toggle mute on channel
 - `0` : Toggle mute all / mute enone


## Use (command-line)

### Playing without opening a window

vspcplay can also be used from the command-line and can play files without opening a
window. Just add the `--novideo` and optionally, `--status_line` options.

Example:
`./vspcplay music/*.spc --novideo --status_line`


### Generating a wave file (SPC to Wav)

It has been requested, so a `--waveout` option was added to version 1.4. In addition
of playing the files (unless `--nosound` was specified) a copy of the output is
saved to the specified wave file.

Note that if you play several .spc files, only one .wav file will be created.

Example:
`
./vspcplay music/ct-304.spc --nosound --novideo --waveout corridor-of-time.wav --status
`

Note: When --nosound is used, the SPC is played as fast as possible. Otherwise playback
is done at normal speed.

### Command-line option reference

The supported option can be listed by passing `-help` in argument:

```
./vspcplay --help

vspcplay v1.4
Usage: ./vspcplay [options] files...

Valid options:
 -h, --help             Print help
 --nosound              Do not output sound
 --novideo              Do not open video window
 --waveout file.wav     (Also) Create a wave file
 --update_in_callback   Update spc sound buffer inside
                        sdl audio callback
 --interpolation        Use sound interpolatoin
 --echo                 Enable echo
 --auto_write_mask      Write mask file automatically when a
                        tune ends due to playtime from tag or
                        default play time.
 --default_time t       Set the default play time in seconds
                        for when there is not id666 tag. (default: 300
 --ignore_tag_time      Ignore the time from the id666 tag and
                        use default time
 --extra_time t         Set the number of extra seconds to play (relative to
                        the tag time or default time).
 --nice                 Try to use less cpu for graphics
 --status_line          Enable a text mode status line

!!! Careful with those!, they can ruin your sets so backup first!!!
 --apply_mask_block  Apply the mask to the file (replace unused blocks(256 bytes) with a pattern)
 --filler val        Set the pattern byte value. Use with the option above. Default 0

The mask will be applied when the tune ends due to playtime from tag
or default playtime.
```


## Compilation

 * libsdl1.2
 * gcc and g++
 * make

If all goes well, a single executable named vspcplay will be generated.

## Authors

* **Raphael Assenat**

## License

This work is derived from the Snes9x code, to which the Snes9x license applies.
See the header of spc700.cpp for details.

