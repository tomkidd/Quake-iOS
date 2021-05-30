<hr>
<img align="left" width="50" height="50" src="https://schnapple.com/wp-content/uploads/2021/05/m1_small.jpg">
I'm trying to fund an M1 Mac to keep old games running on new Macs. If you'd like to help you can contribute here: https://ko-fi.com/schnapple
<hr>
<img align="left" width="100" height="100" src="https://raw.githubusercontent.com/tomkidd/Quake-iOS/master/icon_quake.png">  

#  Quake for iOS and tvOS for Apple TV

&nbsp;

This is my port of Quake for iOS, running in modern resolutions including the full width of the iPhone X. I have also made a target and version for tvOS to run on Apple TV.

![screenshot](https://raw.githubusercontent.com/tomkidd/Quake-iOS/master/ss_quake.png)

Features

- Tested and builds without warnings on Xcode 9.4.1.
- Runs single player campaigns at full screen and full speed on iOS
- MFi controller support (reccomended) and on-screen control options
- Capable of launching Quake, it expansions and the new episode from MachineGames
- Quick save/load support per-game (Quake or expanion packs separate)
- Music support for original soundtrack via ogg files.
- ***EXPERIMENTAL*** support for tilt aiming (enable it via the pause menu)
- Second project target for tvOS that takes advantage of focus model and removes on-screen controls.

This commit does not need any placeholder resources as it is not an update of an existing id Software port. 

You will need to provide your own copies of the `id1` (Quake), `hipnotic` (Mission Pack #1) and `rogue` (Mission Pack #2) directories from an existing instalation of Quake. You can grab the whole thing with expansions on [GOG](https://www.gog.com/game/quake_the_offering) or individually on Steam ([Quake](https://store.steampowered.com/app/2310/QUAKE/), [XP1](https://store.steampowered.com/app/9040/QUAKE_Mission_Pack_1_Scourge_of_Armagon/), [XP2](https://store.steampowered.com/app/9030/QUAKE_Mission_Pack_2_Dissolution_of_Eternity/)). Quake for iOS also supports the `dopa` directory for the 2016 "fifth episode" [*Dimension of the Past*](http://quake.wikia.com/wiki/Episode_5:_Dimension_of_the_Past) from MachiineGames.

You will need to drag your directories into the project and select "Create Folder References" and add them to both the Quake and QuakeTV targets. The folders will be blue if you've done it right:

![folders](https://raw.githubusercontent.com/tomkidd/Quake-iOS/master/folders.png)

Place the music in .ogg file format in a subdirectory of each folder called "music". The music for at least the original game can be found [here](https://www.moddb.com/games/quake/downloads/ultimate-quake-patch-v1-11). They need to be named `track02.ogg`, `track03.ogg`, etc. They have to start at 2 because the engine thinks track 1 is the data of the game. *NOTE:* Mission Pack #1 (hipnotic) plays track 98 in the prerecorded demo for some reason so duplicate `track02.ogg` as `track98.ogg` to hear it. 

***NOTE:*** iOS is **case sensitive**, so the filenames for the data (i.e., `pak0.pak`, `pak1.pak`, etc.) need to be in **lower case** because that's what the engine is looking for. It has come to my attention that some digital retailers such as GOG install the files with upper case filenames. If you run into issues with the game this may be the cause. If you see an error message to the effect of `couldn't load gfx.wad` but you can verify the pak files are in place this may be the cause.

You can read a lengthy blog article on how I did all this [here](http://schnapple.com/quake-for-ios-and-tvos-for-apple-tv/).

This repo was based on the Google Cardboard port contained in this [Quake For OSX](https://github.com/Izhido/Quake_For_OSX) port by Izhido on GitHub. Background music support came from [QuakeSpasm](http://quakespasm.sourceforge.net/). On-screen joystick code came from [this repo](https://github.com/bradhowes/Joystick) by Brad Howe. Quake font DpQuake by Dead Pete available [here](https://www.dafont.com/quake.font)

[Video of Quake running on an iPhone X](https://www.youtube.com/watch?v=5awJDcu-cAs)

[Video of Quake running on an Apple TV](https://www.youtube.com/watch?v=jC_qnGjzO7s)

I have also made apps for [*Wolfenstein 3-D*](https://github.com/tomkidd/Wolf3D-iOS), [*DOOM*, *DOOM II* and *Final DOOM*](https://github.com/tomkidd/DOOM-iOS), [*Quake II*](https://github.com/tomkidd/Quake2-iOS), [*Quake III: Arena*](https://github.com/tomkidd/Quake3-iOS), [*Return to Castle Wolfenstein*](https://github.com/tomkidd/RTCW-iOS) and [*DOOM 3*](https://github.com/tomkidd/DOOM3-iOS).

Have fun. For any questions I can be reached at tomkidd@gmail.com
