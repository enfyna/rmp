<pre>
  _ __ _ __ ___  _ __
 | '__| '_ ` _ \| '_ \
 | |  | | | | | | |_) |
 |_|  |_| |_| |_| .__/
                |_|         random music player™
</pre>

# Building

Install dependencies:

        vlc yt-dlp taglib ncurses

You can build rmp by running:

        make

# Usage

## Playing Musics

After building rmp you can run it like this:

        rmp play -min 67 -max 300 --shortest --exclude fast

rmp will search for all music files except the fast category in the current
folder then play the shortest ones first with a random delay of between 67 and
300 seconds. 

## Adding New Musics

        rmp add fast dQw4w9WgXcQ
        rmp add slow 4TYv2PhG89A

## Restoring Musics

If you downloaded your musics using `rmp add` you can use:

        rmp build extract

This will extract the youtube hashes from the file names to the `.rmp` file
like this:

        fast: dQw4w9WgXcQ | Rick Astley - Never Gonna Give You Up
        slow: 4TYv2PhG89A | Sade - Smooth Operator - Official - 1984

This file then can be used to restore the musics later with their categories.

        rmp build download

rmp will search for the '.rmp' file in the current folder. If it finds the file
it will try downloading all listed musics.
