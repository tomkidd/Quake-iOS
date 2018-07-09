#  Quake for iOS and tvOS for Apple TV

This is my port of Quake for iOS, running in modern resolutions including the full width of the iPhone X. I have also made a target and version for tvOS to run on Apple TV.

Features

- Tested and builds without warnings on Xcode 9.4.1.
- Runs single player campaigns at full screen and full speed on iOS
- MFi controller support (reccomended) and on-screen control options
- Capable of launching Quake or either of its expansion packs
- Quick save/load support per-game (Quake or expanion packs separate)
- Music support for original soundtrack via ogg files.
- Second project target for tvOS that takes advantage of focus model and removes on-screen controls.

This commit does not need any placeholder resources as it is not an update of an existing id Software port. 

You will need to provide your own copies of the `id1` (Quake), `hipnoitc` (Mission Pack #1) and `rogue` (Mission Pack #2) directories from an existing instalation of Quake. You can grab the whole thing with expansions on [GOG](https://www.gog.com/game/quake_the_offering) or individually on Steam ([Quake](https://store.steampowered.com/app/2310/QUAKE/), [XP1](https://store.steampowered.com/app/9040/QUAKE_Mission_Pack_1_Scourge_of_Armagon/), [XP2](https://store.steampowered.com/app/9030/QUAKE_Mission_Pack_2_Dissolution_of_Eternity/))

You will need to drag your directories into the project and select "Create Folder References" and add them to both the Quake and QuakeTV targets. The folders will be blue if you've done it right:

![folders](https://github.com/tomkidd/Quake-iOS11/raw/master/folders.png)

Place the music in .ogg file format in a subdirectory of each folder called "music". The music for at least the original game can be found [here](https://www.moddb.com/games/quake/downloads/ultimate-quake-patch-v1-11). They need to be named `track02.ogg`, `track03.ogg`, etc. They have to start at 2 because the engine thinks track 1 is the data of the game. 

You can read a lengthy blog article on how I did all this [here](http://schnapple.com/?page_id=1313&preview=true).

This repo was based on the Google Cardboard port contained in this [Quake For OSX](https://github.com/Izhido/Quake_For_OSX) port by Izhido on GitHub. Background music support came from [QuakeSpasm](http://quakespasm.sourceforge.net/). On-screen joystick code came from [this repo](https://github.com/bradhowes/Joystick) by Brad Howe. 

[Video of Quake running on an iPhone X](https://www.youtube.com/watch?v=5awJDcu-cAs)

[Video of DOOM running on an Apple TV](https://www.youtube.com/watch?v=jC_qnGjzO7s)

Have fun. For any questions I can be reached at tomkidd@gmail.com
