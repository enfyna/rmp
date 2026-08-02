<pre>
  _ __ _ __ ___  _ __
 | '__| '_ ` _ \| '_ \
 | |  | | | | | | |_) |
 |_|  |_| |_| |_| .__/
                |_|         random music player™
</pre>

# Building

Install dependencies:

        ffmpeg vlc yt-dlp

You can build rmp by running:

        make

# Usage

## Playing Musics

After building rmp you can run it like this:

        ./rmp ~/Music -min 67 -max 300 --shortest --exclude fast

rmp will search for all music files except the fast category in the Music
folder then play the shortest ones first with a random delay of between 67 and
300 seconds. 

## Adding New Musics

        ./rmp add ~/Music fast dQw4w9WgXcQ
        ./rmp add ~/Music slow 4TYv2PhG89A

rmp will take the youtube hash and download it to the Music folder. It will
also create a '.rmp' file inside the folder with the following content:

        fast: dQw4w9WgXcQ | Rick Astley - Never Gonna Give You Up (Official Video) (4K Remaster)
        slow: 4TYv2PhG89A | Sade - Smooth Operator - Official - 1984

This file then can be used to restore the musics later with their categories.

## Restoring Musics

        ./rmp build ~/Music

rmp will search for the '.rmp' file in the Music folder. If it finds the file
it will try downloading all listed musics.
