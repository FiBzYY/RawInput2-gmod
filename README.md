## Gmod RawInput2

An external software that ports [momentum mod's](https://momentum-mod.org/) ``m_rawinput 2`` behaviour. This option provides mouse interpolation which will ["line up with the tickrate properly without needing to have a specific framerate"](https://discord.com/channels/235111289435717633/356398721790902274/997026787995435088) (rio). The code for this isn't public and was reverse engineered from the game.

### Usage
* Run the application.
* * Start GMod (NOTE: Only works on the x64 branch.)
* Make sure to set ``m_rawinput 0`` and ``m_filter 0`` in game.

### Building requirements
* [Microsoft Detours](https://github.com/microsoft/Detours)
