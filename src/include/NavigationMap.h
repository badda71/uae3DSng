typedef struct
{
  std::string activeWidget;
  std::string leftWidget;
  std::string rightWidget;
  std::string upWidget;
  std::string downWidget;
} NavigationMap;

NavigationMap navMap[] = 
{
  { "Reset", "A1200", "Resume", "tabbedArea", "tabbedArea" },
  { "Resume", "Reset", "Quit", "ConfManager", "tabbedArea" },
  { "Save Config", "ConfManager", "A500", "tabbedArea", "Quit" },
  { "ConfManager", "Reset", "Save Config", "tabbedArea", "Resume" },
  { "Quit", "Resume", "A1200", "Save Config", "tabbedArea" },
  { "A500", "Save Config", "Reset", "tabbedArea", "A1200" },
  { "A1200", "Quit", "Reset", "A500", "tabbedArea" },

  // Tab Main
  { "68000", "ChipMem", "1.2", "tabbedArea", "68020" },
  { "68020", "ChipMem", "1.3", "68000", "OCS" },
  { "OCS", "FastMem", "3.1", "68020", "ECS" },
#if defined(PANDORA) && !(defined(WIN32) || defined(AROS))
  { "ECS", "PandSpeed", "AROS", "OCS", "AGA" },
  { "AGA", "PandSpeed", "BlitterMode", "ECS", "Reset" },
#else
  { "ECS", "FastMem", "AROS", "OCS", "AGA" },
  { "AGA", "BM_immediate", "CustomRom", "ECS", "BrowseROM" },
#endif

  { "1.2", "68000", "7MHz", "tabbedArea", "1.3" },
  { "1.3", "68020", "14MHz", "1.2", "2.0" },
  { "2.0", "OCS", "28MHz", "1.3", "3.1" },
#if defined(PANDORA) && !(defined(WIN32) || defined(AROS))
  { "3.1", "OCS", "28MHz", "2.0", "AROS" },
  { "AROS", "ECS", "PandSpeed", "3.1", "BlitterMode" },
#endif
#if defined(AROS) || defined(WIN32)
  { "3.1", "OCS", "56MHz", "2.0", "AROS" },
  { "AROS", "ECS", "112MHz", "3.1", "CustomRom" },
#endif
  { "CustomRom", "AGA", "112MHz", "AROS", "BrowseROM" },
  
  { "7MHz", "1.2", "ChipMem", "tabbedArea", "14MHz" },
  { "14MHz", "1.3", "ChipMem", "7MHz", "28MHz" },
#if defined(PANDORA) && !(defined(WIN32) || defined(AROS))
  { "28MHz", "2.0", "SlowMem", "14MHz", "BlitterMode" },
  { "BlitterMode", "AGA", "PandSpeed", "---", "---" },
#else
  { "28MHz", "2.0", "SlowMem", "14MHz", "56MHz" },
  { "56MHz", "3.1", "FastMem", "28MHz", "112MHz" },
  { "112MHz", "AROS", "FastMem", "56MHz", "Reset" },
  { "BM_normal", "CustomRom", "BM_immediate", "FastMem", "Save Config" },
  { "BM_immediate", "BM_normal", "AGA", "FastMem", "BM_improved" },
  { "BM_improved", "BM_normal", "BrowseROM", "BM_immediate", "A500" },
#endif

  { "PandSpeed", "AROS", "ECS", "---", "---" },

  { "ChipMem", "---", "---", "tabbedArea", "SlowMem" },
  { "SlowMem", "---", "---", "ChipMem", "FastMem" },
#if defined(PANDORA) && !(defined(WIN32) || defined(AROS))
  { "FastMem", "---", "---", "SlowMem", "PandSpeed" },
#else
  { "FastMem", "---", "---", "SlowMem", "BM_normal" },
#endif
  { "BrowseROM", "AGA", "BM_normal", "CustomRom", "Reset" },
  
  // Tab Floppy Drive
  { "DF0", "Drives1", "ejectDF0", "tabbedArea", "DF1" },
  { "DF1", "Drives2", "ejectDF1", "DF0", "DF2" },
  { "DF2", "Drives3", "ejectDF2", "DF1", "DF3" },
  { "DF3", "Drives4", "ejectDF3", "DF2", "Eject" },

  { "ejectDF0", "DF0", "Drives1", "tabbedArea", "ejectDF1" },
  { "ejectDF1", "DF1", "Drives2", "ejectDF0", "ejectDF2" },
  { "ejectDF2", "DF2", "Drives3", "ejectDF1", "ejectDF3" },
  { "ejectDF3", "DF3", "Drives4", "ejectDF2", "Speed1x" },
  
  { "Drives1", "ejectDF0", "DF0", "tabbedArea", "Drives2" },
  { "Drives2", "ejectDF1", "DF1", "Drives1", "Drives3" },
  { "Drives3", "ejectDF2", "DF2", "Drives2", "Drives4" },
  { "Drives4", "ejectDF3", "DF3", "Drives3", "Speed2x" },

  { "Speed1x", "SaveCfgGame", "Speed2x", "ejectDF3", "Speed4x" },
  { "Speed2x", "Speed1x", "Eject", "Drives4", "Speed8x" },
  { "Speed4x", "SaveCfgGame", "Speed8x", "Speed1x", "Save Config" },
  { "Speed8x", "Speed4x", "Eject", "Speed2x", "A500" },

  { "Eject", "Speed2x", "SaveCfgGame", "DF3", "Reset" },
  { "SaveCfgGame", "Eject", "Speed1x", "DF3", "Reset" },

  // Tab HD
  { "HDOff", "HDDir", "HDDir", "tabbedArea", "Dir" },
  { "Dir", "HDFile", "HDFile", "HDOff", "File" },
  { "File", "HDFile", "HDFile", "Dir", "SaveHD" },
  
  { "HDDir", "HDOff", "HDOff", "tabbedArea", "HDFile" },
  { "HDFile", "Dir", "Dir", "HDDir", "SaveHD" },
  { "SaveHD", "File", "File", "HDFile", "ConfManager" },

  // Tab Display/Sound
  { "320", "8K", "200", "tabbedArea", "640" },
  { "640", "11K", "216", "320", "352" },
  { "352", "22K", "240", "640", "704" },
  { "704", "32K", "256", "352", "384" },
  { "384", "44K", "262", "704", "768" },
  { "768", "44K", "270", "384", "Reset" },

  { "200", "320", "Frameskip0", "tabbedArea", "216" },
  { "216", "640", "Frameskip1", "200", "240" },
  { "240", "352", "Frameskip1", "216", "256" },
  { "256", "704", "VertPos", "240", "262" },
  { "262", "384", "CutLeft", "256", "270" },
  { "270", "768", "CutRight", "262", "Reset" },

  { "Frameskip0", "200", "50Hz", "tabbedArea", "Frameskip1" },
  { "Frameskip1", "216", "60Hz", "Frameskip0", "VertPos" },

  { "50Hz", "Frameskip0", "SoundOff", "tabbedArea", "60Hz" },
  { "60Hz", "Frameskip1", "SoundFast", "50Hz", "VertPos" },

  { "VertPos", "256", "SoundAcc", "---", "---" },
  { "CutLeft", "262", "mono", "---", "---" },
  { "CutRight", "270", "mono", "---", "---" },

  { "SoundOff", "50Hz", "8K", "tabbedArea", "SoundFast" },
  { "SoundFast", "60Hz", "11K", "SoundOff", "SoundAcc" },
  { "SoundAcc", "VertPos", "22K", "SoundFast", "mono" },

  { "mono", "CutLeft", "44K", "SoundAcc", "stereo" },
  { "stereo", "CutRight", "44K", "mono", "Save Config" },

  { "8K", "SoundOff", "320", "tabbedArea", "11K" },
  { "11K", "SoundFast", "640", "8K", "22K" },
  { "22K", "SoundAcc", "352", "11K", "32K" },
  { "32K", "mono", "704", "22K", "44K" },
  { "44K", "mono", "384", "32K", "A500" },

  // Tab Savestates
  { "Savestate0", "---", "---", "tabbedArea", "Savestate1" },
  { "Savestate1", "---", "---", "Savestate0", "Savestate2" },
  { "Savestate2", "---", "---", "Savestate1", "Savestate3" },
  { "Savestate3", "---", "---", "Savestate2", "LoadState" },

  { "LoadState", "---", "---", "Savestate3", "SaveState" },
  { "SaveState", "---", "---", "LoadState", "Reset" },
  
  // Tab Control
  { "ControlCfg1", "TapNormal", "ControlCfg2", "tabbedArea", "ControlCfg3" },
  { "ControlCfg2", "ControlCfg1", "Port0", "tabbedArea", "ControlCfg4" },
  { "ControlCfg3", "TapShort", "ControlCfg4", "ControlCfg1", "Reset" },
  { "ControlCfg4", "ControlCfg3", "Port1", "ControlCfg2", "Reset" },
  { "Port0", "ControlCfg2", "Light", "tabbedArea", "Port1" },
  { "Port1", "ControlCfg4", "Medium", "Port0", "Both" },

#if defined(AROS) || defined(WIN32)
  { "Both", "ControlCfg4", "Heavy", "Port1", "Reset" },
#else
  { "Both", "ControlCfg4", "Heavy", "Port1", "StatusOn" },
  { "StatusOn", "ControlCfg4", "Heavy", "Both", "StatusOff" },
  { "StatusOff", "ControlCfg4", "Heavy", "StatusOn", "Reset" },
#endif

  { "Light", "Port0", "Mouse.25", "tabbedArea", "Medium" },
  { "Medium", "Port1", "Mouse.5", "Light", "Heavy" },
  { "Heavy", "Both", "Mouse1x", "Medium", "ConfManager" },

  { "Mouse.25", "Light", "TapNormal", "tabbedArea", "Mouse.5" },
  { "Mouse.5", "Medium", "TapShort", "Mouse.25", "Mouse1x" },
  { "Mouse1x", "Heavy", "TapNo", "Mouse.5", "Mouse2x" },
#if defined(AROS) || defined(WIN32)
  { "Mouse2x", "Heavy", "StatusLine", "Mouse1x", "Mouse4x" },
  { "Mouse4x", "Heavy", "StatusLine", "Mouse2x", "StylusOffset" },
#else
  { "Mouse2x", "Heavy", "TapNo", "Mouse1x", "Mouse4x" },
  { "Mouse4x", "Heavy", "TapNo", "Mouse2x", "StylusOffset" },
#endif

  { "TapNormal", "Mouse.25", "ControlCfg1", "tabbedArea", "TapShort" },
  { "TapShort", "Mouse.5", "ControlCfg3", "TapNormal", "TapNo" },
#if defined(AROS) || defined(WIN32)
  { "TapNo", "Mouse1x", "ControlCfg3", "TapShort", "StatusLine" },
  
  { "StatusLine", "Mouse4x", "ControlCfg3", "TapNo", "StylusOffset" },

  { "StylusOffset", "Mouse4x", "Mouse4x", "---", "---" },
#else
  { "TapNo", "Mouse1x", "ControlCfg3", "TapShort", "StylusOffset" },

  { "StylusOffset", "StatusOff", "StatusOff", "---", "---" },
#endif

  // Tab Custom Control
  { "CustomCtrlOff", "CtrlUp", "CustomCtrlOn", "tabbedArea", "DPadCustom" },
  { "CustomCtrlOn", "CustomCtrlOff", "CtrlB", "tabbedArea", "DPadCustom" },
  
  { "DPadCustom", "CtrlRight", "CtrlL", "CustomCtrlOff", "DPadJoystick" },
  { "DPadJoystick", "CtrlRight", "CtrlR", "DPadCustom", "DPadMouse" },
  { "DPadMouse", "CtrlRight", "CtrlR", "DPadJoystick", "Reset" },
  
  { "CtrlA", "CustomCtrlOn", "CtrlUp", "---", "---" },
  { "CtrlB", "CustomCtrlOn", "CtrlDown", "---", "---" },
  { "CtrlX", "CustomCtrlOn", "CtrlLeft", "---", "---" },
  { "CtrlY", "DPadCustom", "CtrlRight", "---", "---" },
  { "CtrlL", "DPadCustom", "CtrlRight", "---", "---" },
  { "CtrlR", "DPadJoystick", "CtrlRight", "---", "---" },

  { "CtrlUp", "CtrlA", "CustomCtrlOff", "---", "---" },
  { "CtrlDown", "CtrlB", "CustomCtrlOff", "---", "---" },
  { "CtrlLeft", "CtrlX", "CustomCtrlOff", "---", "---" },
  { "CtrlRight", "CtrlY", "DPadCustom", "---", "---" },

  // File dialog
  { "dirList1", "cmdCancel1", "cmdOk", "---", "---" },
  { "cmdCancel1", "cmdOk", "cmdOk", "dirList1", "dirList1" },
  { "cmdOk", "cmdCancel1", "cmdCancel1", "dirList1", "dirList1" },

  { "dirList2", "cmdCancel2", "cmdSelect", "---", "---" },
  { "cmdCancel2", "cmdOpen", "cmdSelect", "dirList2", "dirList2" },
  { "cmdSelect", "cmdCancel2", "cmdOpen", "dirList2", "dirList2" },
  { "cmdOpen", "cmdSelect", "cmdCancel2", "dirList2", "dirList2" },

  // Config Manager
  { "cfgText", "cfgLoad", "cfgLoad", "configList", "configList" },
  { "configList", "cfgLoad", "cfgLoad", "---", "---" },
  { "cfgSave", "cfgText", "configList", "cfgLoad", "cfgDelete" },
  { "cfgDelete", "configList", "configList", "cfgSave", "cfgCancel" },
  { "cfgLoad", "cfgText", "configList", "cfgCancel", "cfgSave" },
  { "cfgCancel", "configList", "configList", "cfgDelete", "cfgLoad" },
  
  { "END", "", "", "", "" }
};
