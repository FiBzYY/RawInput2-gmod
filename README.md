## x64 Gmod RawInput2

- Main useage BHOP and Surf and fast pace moving gamemodes.

An external software that ports [momentum mod's](https://momentum-mod.org/) ``m_rawinput 2`` behaviour. This option provides mouse interpolation which will ["line up with the tickrate properly without needing to have a specific framerate"](https://discord.com/channels/235111289435717633/356398721790902274/997026787995435088) (rio). The code for this isn't public and was reverse engineered from the game.

### Usage
* Run the application either the RawInput2_smoothing.exe one for new m_filter logic or RawInput2.exe without filter logic.
* Start GMod (NOTE: Only works on the x64 branch.)
* Make sure to set ``m_filter 0`` in game.

### DO TO:
* Add CVar support for m_rawinput 2 (Right now its always on when ran).
* Replace CVar ``m_filter 1`` logic with new one due to the m_filter logic with low FPS it can cause choppiness and bad input.

### Credits:
* Haze for the logic. Github: https://github.com/Haze1337
* schweiziske for getting the correct patterns for GMod. - Discord: schweiziske Steam: [https://steamcommunity.com/id/fibzy_](https://steamcommunity.com/id/fibzy_/)
* Me for adding new filter logic. - Discord: schweiziske Steam: [https://steamcommunity.com/id/schweiziske](https://steamcommunity.com/id/schweiziske/)

### Building requirements
* [Microsoft Detours](https://github.com/microsoft/Detours)

* ### Note:
* The source code patterns for GMod will not be released due to game security of facepunch.
