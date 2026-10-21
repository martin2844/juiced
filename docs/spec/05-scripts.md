# Text scripts of the stock game

Behaviour of Juiced 1.0 (PC) as observed; addresses are those of the retail build.

Most data files of the game (`scripts\*.txt`, the interface windows, the controls, effects, atmospheres, collision
objects, sound banks) are read by one script reader. A handler class names the map of its keywords (vtable slot 1);
each map is filled at start from a table of 16-byte entries {name, -, handler, adjustment} ending with a null name
(`0x004FB6C0`).

## Reading (`0x004FB7C0`, `0x004FBCF0`)

- The file is read whole. Every ` ~` (a space and a tilde) and the rest of its line, with the line break after it,
  becomes spaces: a line ending in ` ~` goes on in the next line.
- The text is split into lines at CR, LF and CR LF.
- For each line: white space at the start is skipped; empty lines and lines starting with `;` are skipped; the
  first word (up to white space) is the keyword, looked up without regard to case (`0x00525640`, `_stricmp`); an
  unknown keyword is skipped; a handler returning 0 stops the file.
- A handler takes the line's further words in turn (`0x004FBC40`): white space is skipped; a word is the text up to
  the next white space, or, starting with `"`, the text up to the next `"` (the quotes are not part of it; `""` is
  an empty word); none at the end of the line. `0x004FBBE0` tells whether the line has no more words.

## Keywords and handlers

### ArcadeUnlock.txt (map `0x00767D28`)

`Level` 0x0042A870, `Car` 0x0042A890

### ArcadeRaces.txt (map `0x00767D40`)

`[Race]` 0x0042BA70, `Type` 0x0042BBF0, `ExtremeNitro` 0x0042BBD0, `DevChallenge` 0x0042BBE0, `Location` 0x0042BD30, `NumLaps` 0x0042C120, `Car` 0x0042C170, `Time` 0x0042C240, `Weather` 0x0042C310, `Challenge` 0x0042C3E0, `AILevel` 0x0042C3A0, `CarColour` 0x0042BD60, `CarDecal` 0x0042BF30, `CarWheel` 0x0042BFE0, `CarNeon` 0x0042C090, `[EndRace]` 0x0042C430, `[UnlockTracks]` 0x0042C440, `Track` 0x0042C490, `BonusTrack` 0x0042C4D0, `[EndUnlock]` 0x0042E470

### surface effects (map `0x00768948`)

`[BeginSurfaceEffect]` 0x004929B0, `WHEELSPIN` 0x00492AA0, `SKID` 0x00492AF0, `TRACKS` 0x00492B40, `SPARKS` 0x00492B90, `FLAKES` 0x00492BE0, `RUSH` 0x00492C30, `[EndSurfaceEffect]` 0x00492CA0

### cardefs.txt (map `0x00768978`)

`[BeginCar]` 0x00499370, `MANUFACTURER` 0x00499430, `VALUE` 0x00499460, `STATE` 0x004994B0, `STOCKTURBO` 0x00499500, `STANDARD` 0x00499510, `CUSTOM` 0x004995F0, `ALLOW_NONE` 0x00499700, `STANDARDWHEELS` 0x00499590, `STANDARDEXHAUST` 0x004995C0, `ENGINE_SOUND` 0x00499760, `MAXDROPHEIGHT` 0x00499790, `[EndCar]` 0x004997E0, `[BeginColour]` 0x00499840, `BaseColour` 0x00499A10, `MetallicColour` 0x00499A60, `PearlescentColour` 0x00499AC0, `[EndColour]` 0x00499B20, `[DialColour]` 0x00499B70, `[NeonColour]` 0x00499C40, `[DecalColour]` 0x00499CE0, `[WheelColour]` 0x00499D80

### parts (mods) (map `0x00768990`)

`[BeginMod]` 0x00499F30, `LEVEL` 0x00499FB0, `SUPPLIER` 0x0049A030, `TEXT` 0x0049A060, `SHAPE` 0x0049A090, `COST` 0x0049A0C0, `PHYSICS` 0x0049A110, `UIONTEX` 0x0049A1C0, `UIOFFTEX` 0x0049A1F0, `DATAVAL` 0x0049A220, `[EndMod]` 0x0049A270

### car collections (CMSPlayersCrewCollection.txt, CMSCrewNNCollection.txt, ...) (map `0x007689A8`)

`[BeginCollection]` 0x0049A380, `BeginCar` 0x0049A390, `MetallicColour` 0x0049A510, `PearlescentColour` 0x0049A5A0, `BaseColour` 0x0049A480, `Modification` 0x0049A650, `Prototype` 0x0049A640, `AutoMod` 0x0049A630, `EndCar` 0x0049A7E0, `[EndCollection]` 0x004A1760

### routes (map `0x007689D4`)

`GRIDPOINT` 0x004A1410, `ROUTETYPE` 0x004A14F0, `MINCORNERSPEED` 0x004A1560, `BEGINSPLINE` 0x004A15B0, `POINT` 0x004A1630, `ENDSPLINE` 0x004A1760

### CMSCrewChallenges.txt (map `0x007689EC`)

`[BeginCrew]` 0x004A8890, `[EndCrew]` 0x004A88E0, `Challenge` 0x004A8900

### CMSCrews.txt (map `0x00768A04`)

`[BeginCrew]` 0x004A7A10, `[EndCrew]` 0x004A7A50, `CarColours` 0x004A7A80, `WheelColor` 0x004A7C70, `Wheels` 0x004A7CC0, `NeonColour` 0x004A7D10, `EntranceFee` 0x004A7D60, `PrizeMoney` 0x004A7DB0, `DriverName` 0x004A7E50, `ShortDriverName` 0x004A7F10, `WillAttendAt` 0x004A7F60, `RespectFactor` 0x004A7FE0, `Circuit` 0x004A8040, `P2P` 0x004A8160, `Cruise` 0x004A8240, `Illegal` 0x004A8320, `Sprint` 0x004A8400, `ProbMix` 0x004A84E0, `ProbCircuit` 0x004A8540, `ProbP2P` 0x004A8600, `ProbCruising` 0x004A8660, `ProbSprint` 0x004A85A0, `ProbTerritory` 0x004A86C0, `ProbMake` 0x004A8720, `ProbModel` 0x004A8780, `CarPref` 0x004A87E0

### CMSCrewmates.txt (map `0x00768A1C`)

`[BeginCrewmate]` 0x004A9FE0, `Name` 0x004AA020, `ShortName` 0x004AA060, `JoinThreshold` 0x004AA0A0, `JoinCrewIdx` 0x004AA0F0, `PhoneNumber` 0x004AA140, `DrivingAbility` 0x004AA190, `DrivingLimit` 0x004AA1E0, `Recover` 0x004AA230, `RecoverLimit` 0x004AA280, `LearnRate` 0x004AA2D0, `ForgetRate` 0x004AA320, `[EndCrewmate]` 0x004AA370

### CMSFlyers.txt (map `0x00768A64`)

`Crew` 0x004B2A50, `FontColour` 0x004B2A70, `PlayerFontColour` 0x004B2B20, `Logo` 0x004B2BE0

### collision objects, dynamic objects (map `0x0076A254`)

`[BeginCollisionObject]` 0x005366B0, `[EndCollisionObject]` 0x00536740, `Name` 0x00536760, `MaxCollidees` 0x005367A0, `Primary` 0x005367F0, `ID` 0x00536810, `SurfaceType` 0x00536850, `Mu` 0x005368A0, `Restitution` 0x005368F0, `File` 0x00536940, `Point` 0x00536960, `Normal` 0x005369A0, `Centre` 0x005369E0, `End` 0x00536A20, `Radius` 0x00536A60, `[BeginMeshCollider]` 0x0042E470, `[EndMeshCollider]` 0x00536AB0, `[BeginMeshCollidee]` 0x0042E470, `[EndMeshCollidee]` 0x00536BA0, `[BeginClippingPlane]` 0x0042E470, `[EndClippingPlane]` 0x00536CA0, `[BeginSphereCollider]` 0x0042E470, `[EndSphereCollider]` 0x00536D00, `[BeginLineCollider]` 0x0042E470, `[EndLineCollider]` 0x00536DA0, `[BeginSphereCollidee]` 0x0042E470, `[EndSphereCollidee]` 0x00536DE0, `[BeginDynamicObject]` 0x004EF550, `Mass` 0x004EF5E0, `Inertia` 0x004EF640, `CentreOfGravity` 0x004EF690, `LocalGravity` 0x004EF6E0, `DragCoefficient` 0x004EF740, `Sleep` 0x004EF7F0, `[BeginAirfoil]` 0x004EF870, `Position` 0x004EF890, `Force` 0x004EF8F0, `DragCoefficient` 0x004EF740, `[EndAirfoil]` 0x004EF950, `[BeginRigidContact]` 0x004EF990, `[BeginSpringContact]` 0x004EF9B0, `ContactId` 0x004EF9D0, `ClippingPlane` 0x004EFA20, `NoClippingPlane` 0x004EFB70, `MaxCompression` 0x004EFBD0, `Stiffness` 0x004EFC30, `DampingRatio` 0x004EFC90, `WeightDistribution` 0x004EFCF0, `[EndRigidContact]` 0x004EFD50, `[EndSpringContact]` 0x004EFDF0, `[EndDynamicObject]` 0x004EFEF0

### collision objects (map `0x007756E8`)

`[BeginCollisionObject]` 0x005366B0, `[EndCollisionObject]` 0x00536740, `Name` 0x00536760, `MaxCollidees` 0x005367A0, `Primary` 0x005367F0, `ID` 0x00536810, `SurfaceType` 0x00536850, `Mu` 0x005368A0, `Restitution` 0x005368F0, `File` 0x00536940, `Point` 0x00536960, `Normal` 0x005369A0, `Centre` 0x005369E0, `End` 0x00536A20, `Radius` 0x00536A60, `[BeginMeshCollider]` 0x0042E470, `[EndMeshCollider]` 0x00536AB0, `[BeginMeshCollidee]` 0x0042E470, `[EndMeshCollidee]` 0x00536BA0, `[BeginClippingPlane]` 0x0042E470, `[EndClippingPlane]` 0x00536CA0, `[BeginSphereCollider]` 0x0042E470, `[EndSphereCollider]` 0x00536D00, `[BeginLineCollider]` 0x0042E470, `[EndLineCollider]` 0x00536DA0, `[BeginSphereCollidee]` 0x0042E470, `[EndSphereCollidee]` 0x00536DE0, `[BeginDynamicObject]` 0x004EF550, `Mass` 0x004EF5E0, `Inertia` 0x004EF640, `CentreOfGravity` 0x004EF690, `LocalGravity` 0x004EF6E0, `DragCoefficient` 0x004EF740, `Sleep` 0x004EF7F0, `[BeginAirfoil]` 0x004EF870, `Force` 0x004EF8F0, `Position` 0x004EF890, `DragCoefficient` 0x004EF740, `[EndAirfoil]` 0x004EF950, `[BeginRigidContact]` 0x004EF990, `[BeginSpringContact]` 0x004EF9B0, `ContactId` 0x004EF9D0, `ClippingPlane` 0x004EFA20, `NoClippingPlane` 0x004EFB70, `MaxCompression` 0x004EFBD0, `Stiffness` 0x004EFC30, `DampingRatio` 0x004EFC90, `WeightDistribution` 0x004EFCF0, `[EndRigidContact]` 0x004EFD50, `[EndSpringContact]` 0x004EFDF0, `[EndDynamicObject]` 0x004EFEF0, `[BeginCar]` 0x004F4470, `[BeginTyreContact]` 0x004F4510, `LateralStiffness` 0x004F4530, `LongitudinalStiffness` 0x004F4590, `LateralMu` 0x004F45F0, `LongitudinalMu` 0x004F4650, `MagicFormulaB` 0x004F46B0, `MagicFormulaC` 0x004F4710, `MagicFormulaD` 0x004F4770, `MagicFormulaE` 0x004F47D0, `[EndTyreContact]` 0x004F4830, `TorqueCurveX` 0x004F4AA0, `TorqueCurveY` 0x004F4C00, `MaxTorque` 0x004F4D60, `ReverseGearRatio` 0x004F4DC0, `ForwardGearRatios` 0x004F4E20, `EngineInertia` 0x004F4F80, `ClutchSpeed` 0x004F4FE0, `SteeringFactor` 0x004F5040, `SteeringSpeed` 0x004F50A0, `PowerRangeMin` 0x004F5100, `PowerRangeMax` 0x004F5160, `Turbo` 0x004F51C0, `Nitro` 0x004F5220, `ABS` 0x004F52A0, `ASR` 0x004F52C0, `ASRFactor` 0x004F52E0, `TimeFactor` 0x004F5340, `FrontWeightDistribution` 0x004F53A0, `[BeginWheel]` 0x004F5400, `WheelId` 0x004F5420, `WheelRadius` 0x004F5480, `FinalDrive` 0x004F54E0, `WheelInertia` 0x004F5560, `Width` 0x004F55E0, `MaxSteerAngle` 0x004F5640, `BrakeMu` 0x004F56A0, `HandbrakeMu` 0x004F5700, `AxleMu` 0x004F5780, `Drive` 0x004F57E0, `BaseHeight` 0x004F5840, `[EndWheel]` 0x004F58A0, `[EndCar]` 0x004F58D0

### effects (map `0x00778A20`)

`[BeginEffect]` 0x00506BE0, `MATERIAL_NAME` 0x00507210, `BASE_TYPE` 0x00507350, `NUM_POINTS` 0x00507260, `NUM_POINTS_PS2` 0x005072D0, `COLOUR_START` 0x00507580, `COLOUR_END` 0x00507640, `COLOUR_VARY` 0x00507700, `BRIGHTNESS_RANGE` 0x005077C0, `EMIT_INTERVAL` 0x00507810, `AGE_RANGE` 0x00507860, `START_SIZE` 0x005078B0, `GROWTH_RATE` 0x00507900, `GRAVITY` 0x00507C80, `FOLLOW_FRAC` 0x00507CD0, `FOLLOW_FADE` 0x00507D20, `ROTATION_RATE` 0x00507950, `FADEIN_TIME` 0x005079A0, `INITIAL_TEMP` 0x005079F0, `COOLING_RATE` 0x00507A40, `WIND_RESISTANCE` 0x00507A90, `EMIT_DIRECTION` 0x00507AE0, `EMIT_SPEED` 0x00507B90, `SPEED_RANGE` 0x00507BE0, `RANDOM_DRIFT` 0x00507C30, `TRIGGER_LEVEL` 0x00507D70, `SAME_AS` 0x00506ED0, `SOUND_EFFECT` 0x00507E50, `NO_SOUND_EFFECT` 0x00507E10, `SOUND_VOLUME` 0x00507F80, `SOUND_ROLLOFF` 0x00507FD0, `SOUND_MINDIST` 0x00508020, `SOUND_PITCHMUL` 0x00508070, `MIN_AMOUNT` 0x00507DC0, `SOUND_TRIGGER_LEVEL` 0x005080C0, `SOUND_TRIGGER` 0x005081B0, `SOUND_MIN_VOL` 0x00508110, `SOUND_DOPPLER` 0x00508160, `LOD_OFF_MUL` 0x00508200, `NUM_TEXTURES` 0x00507300, `BOUNCINESS` 0x00508250, `MAX_VIS_SIZE` 0x005082A0, `LIFETIME` 0x005082F0, `[EndEffect]` 0x00508340

### atmosphere (.atm) (map `0x0079D9B8`)

`[BeginAtmosphere]` 0x00531E30, `FogColour` 0x00531F50, `FogColourFront` 0x00531FF0, `FogColourBack` 0x00532080, `FogMin` 0x00532110, `FogMax` 0x00532150, `FogNear` 0x00532190, `FogFar` 0x005321D0, `AmbientColour` 0x00532210, `LightColour1` 0x005322C0, `LightColour2` 0x00532370, `LightColour3` 0x00532420, `LightDirection1` 0x005324D0, `LightDirection2` 0x00532590, `LightDirection3` 0x00532650, `SkyColour` 0x00532710, `SkyAngle` 0x005327A0, `HorizonColourFront` 0x005327F0, `HorizonColourBack` 0x00532880, `SkyColourFront` 0x00532910, `SkyColourBack` 0x005329A0, `ZenithColour` 0x00532A30, `SkyHeightFactor` 0x00532AC0, `SkyHeightManual` 0x00532B00, `SunShape` 0x00532B40, `SunReflection` 0x00532B70, `SunFlare` 0x00532BA0, `SunSize` 0x00532BD0, `SunFlareSize` 0x00532C40, `SunReflectionSize` 0x00532C80, `SunAngle` 0x00532CC0, `Envmap` 0x00532C10, `ExposureBias` 0x00532D10, `ExposureEnable` 0x00532D50, `ExposureLimits` 0x00532D90, `Night` 0x00532E50, `SkyLuminosity` 0x00532E70, `Wet` 0x00532EB0, `LightBlur` 0x00532ED0, `ShadowSet` 0x00532F10, `WorldShadowColour` 0x00532F50, `WorldShadowAdditive` 0x00532FF0, `TestMipmapBias` 0x00532E10, `NightRoadShininess` 0x00533010, `MinRoadWetness` 0x00533050, `MaxRoadWetness` 0x005330A0, `RoadWetnessTime` 0x005330E0, `MinRainOnTime` 0x00533130, `MaxRainOnTime` 0x005331A0, `MinRainOffTime` 0x00533210, `MaxRainOffTime` 0x00533280, `PuddleColour` 0x005332F0, `PS2BuildingGlassColour` 0x00533380, `[EndAtmosphere]` 0x0042E470

### collision objects (map `0x0079D9D0`)

`[BeginCollisionObject]` 0x005366B0, `[EndCollisionObject]` 0x00536740, `Name` 0x00536760, `MaxCollidees` 0x005367A0, `Primary` 0x005367F0, `ID` 0x00536810, `SurfaceType` 0x00536850, `Mu` 0x005368A0, `Restitution` 0x005368F0, `File` 0x00536940, `Point` 0x00536960, `Normal` 0x005369A0, `Centre` 0x005369E0, `End` 0x00536A20, `Radius` 0x00536A60, `[BeginMeshCollider]` 0x0042E470, `[EndMeshCollider]` 0x00536AB0, `[BeginMeshCollidee]` 0x0042E470, `[EndMeshCollidee]` 0x00536BA0, `[BeginClippingPlane]` 0x0042E470, `[EndClippingPlane]` 0x00536CA0, `[BeginSphereCollider]` 0x0042E470, `[EndSphereCollider]` 0x00536D00, `[BeginLineCollider]` 0x0042E470, `[EndLineCollider]` 0x00536DA0, `[BeginSphereCollidee]` 0x0042E470, `[EndSphereCollidee]` 0x00536DE0

### controls_dx9_*.txt (map `0x0079E2A0`)

`[BeginActionMap]` 0x0058EF70, `Name` 0x0058F040, `Device` 0x0058F0C0, `GUID` 0x0058F090, `[BeginAction]` 0x0058F0F0, `Type` 0x0058F210, `OneShot` 0x0058F290, `Control` 0x0058F2B0, `Control2` 0x0058F2F0, `Deadzone` 0x0058F330, `Saturation` 0x0058F390, `Sign` 0x0058F3F0, `Attack` 0x0058F470, `Decay` 0x0058F4D0, `[EndAction]` 0x0058F530, `[EndActionMap]` 0x0058F550

### interface windows (.ui) (map `0x0079E680`)

`[BeginUI]` 0x005AA660, `BeginLine` 0x005AACB0, `EndLine` 0x005AB160, `BeginBox` 0x005AAE40, `EndBox` 0x005AB160, `BeginText` 0x005AAFD0, `EndText` 0x005AB160, `BeginButton` 0x005AB170, `EndButton` 0x005AB160, `Begin3d` 0x005AB300, `End3d` 0x005AB550, `BeginMenu` 0x005AB5A0, `EndMenu` 0x005AB160, `BeginSlider` 0x005AB740, `EndSlider` 0x005AB160, `BeginTicker` 0x005AB8E0, `EndTicker` 0x005AB160, `BeginMovie` 0x005ABA80, `EndMovie` 0x005AB160, `BeginDial` 0x005ABC20, `EndDial` 0x005AB160, `BeginMeter` 0x005ABDC0, `EndMeter` 0x005AB160, `BeginColourChart` 0x005ABF60, `EndColourChart` 0x005AB160, `Colour` 0x005AC100, `Outline` 0x005AC1A0, `Texture` 0x005AC240, `UVs` 0x005AC270, `UVScroll` 0x005AC330, `Font` 0x005AC3A0, `HighFont` 0x005AC500, `Caption` 0x005AC670, `Align` 0x005AC6F0, `Highlight` 0x005AC780, `VertexColour` 0x005AC7A0, `FontModifier` 0x005AC8C0, `Animated` 0x005ACA10, `Additive` 0x005ACAB0, `InactivePos` 0x005ACAD0, `Multiline` 0x005ACB20, `Autoscroll` 0x005ACB40, `TickerSpeed` 0x005ACB70, `SetModel` 0x005ACBC0, `ModelOrientation` 0x005ACC70, `MoveMode` 0x005ACE50, `ModelRotation` 0x005ACD50, `CameraPosition` 0x005ACF00, `CameraOrientation` 0x005ACFD0, `CameraLookAt` 0x005AD0A0, `Ambient` 0x005AD140, `Directional` 0x005AD220, `Atmosphere` 0x005AD380, `AddItem` 0x005AD3A0, `AddTexturedItem` 0x005AD4B0, `ItemHeight` 0x005AD620, `HighlightItemHeight` 0x005AD670, `SelectItem` 0x005AD6C0, `Shadowed` 0x005AD700, `TextXOffset` 0x005AD740, `UnselectedOffset` 0x005AD790, `OptionCentered` 0x005AD7E0, `MenuType` 0x005AD800, `ClipMenu` 0x005AD860, `AutoArrows` 0x005AD880, `HelpText` 0x005AD8A0, `SliderType` 0x005ADA10, `SliderColour` 0x005ADA70, `SliderTexture` 0x005ADB10, `KnobTexture` 0x005ADB60, `Movie` 0x005ADBB0, `Looped` 0x005ADC10, `PlayFast` 0x005ADC30, `NeedleTexture` 0x005ADC50, `NeedleData` 0x005ADD30, `MeterType` 0x005ADE70, `MeterTexture` 0x005ADED0, `MeterVertexColour` 0x005ADF20, `MeterCentered` 0x005AE030, `UpButton` 0x005AE050, `DownButton` 0x005AE090, `LeftButton` 0x005AE0D0, `RightButton` 0x005AE110, `HighlightButton` 0x005AE150, `Transition` 0x005AE180, `[EndUI]` 0x005AE400

### sound banks (%s.cfg) (map `0x0079E6F8`)

`[BeginBank]` 0x0042E470, `bank_size` 0x005BE930, `bank_name` 0x005BEA80, `instance_count` 0x005BE970, `banktype` 0x005BE9B0, `declare_sample` 0x005BEAB0, `declare_stream` 0x005BEAB0, `[EndBank]` 0x005BEAE0, `[BeginSS]` 0x005BEB00, `type` 0x005BEC40, `name` 0x005BEB50, `displayname` 0x005BEB80, `sample` 0x005BEBB0, `instances` 0x005BEBF0, `[EndSS]` 0x005BEC80

### sound banks (map `0x0079E71C`)

`[Begin]` 0x005BEE10, `CHANNELS` 0x005BEE20, `bank_size` 0x005BE930, `bank_name` 0x005BEA80, `instance_count` 0x005BE970, `banktype` 0x005BE9B0, `declare_sample` 0x005BEAB0, `declare_stream` 0x005BEAB0, `[BeginSS]` 0x005BEB00, `type` 0x005BEC40, `name` 0x005BEB50, `displayname` 0x005BEB80, `sample` 0x005BEBB0, `instances` 0x005BEBF0, `[EndSS]` 0x005BEC80, `[End]` 0x005BEAE0
