#pragma once

//TODO: CShip, CLoot, CSolar.

enum class CShipVTable
{
    IntializeInstance,
    DoSimulationController,
    DestroyInstance,
    SetPosition,
    GetPosition,
    SetOrientation,
    GetTransform,
    SetTransform,
    GetTransform2,
    GetCenteredRadius,
    SetCenteredRadius,
    SetInstanceFlags,
    GetInstanceFlags,
    WriteFile,
    dunno, //Vtable has it as sub_62881E10, just seems to be an empty ptr.
    GetVelocity,
    SetVelocity,
    GetAngularVelocity,
    SetVelocity2,
    Open,
    Update,
    GetVelocity2,
    GetAngularVelocity2,
    DisableControllers,
    EnableControllers,
    GetPhysicalRadiusR,
    GetCenterOfMass,
    GetMass,
    GetSurfaceExtents,
    UnMakePhysical,
    RemakePhysical,
    BeamObject,
    InitCSimple,
    CachePhysicalProperties,
    GetName,
    IsTargetable,
    Connect,
    Disconnect,
    SetHitPoints,
    InitPhysics,
    InitCObject,
    LoadEquipAndCargo,
    ClearEquipAndCargo,
    GetEquipDescList,
    AddItem,
    Activate,
    GetActivateState,
    Disconnect2,
    Disconnect3,
    Connect2,
    Notify,
    FlushAnimations,
    GetTotalHitPoints,
    GetTotalMaxHitPoints,
    GetTotalRelativeHealth,
    GetSubObjectVelocity,
    GetSubObjectCenterOfMass,
    GetSubObjectHitPoints,
    GetSubObjectHitPoints2,
    GetSubObjectMaxHitPoints,
    GetSubObjectRelativeHealth,
    InstToSubObjectId, //inst = instance?
    SubObjectIdToInst,
    DestroySubObjects,
    EnumerateSubObjects,
    AllocEquip,
    LinkShields,
    InitCShip,
    Size = InitCShip,
    Start = 0x0639C02C,
    End = 0x0639C138
};

//Note CLoot VTable seems to have this after the initCLoot, not sure if its random or actually apart of the VTable
//.rdata:0639D8C0                 dd offset unk_63A7F38

enum class CLootVTable
{
    InitializeInstance,
    DoSimulationController,
    DestroyInstance,
    SetPosition,
    GetPosition,
    SetOrientation,
    GetTransform,
    SetTransform,
    GetTransform2,
    GetCenteredRadius,
    SetCenteredRadius,
    SetInstanceFlags,
    GetInstanceFlags,
    WriteFile,
    dunno, //.rdata:0639D854                 dd offset sub_629D800
    GetVelocity,
    SetVelocity,
    GetAngularVelocity,
    SetVelocity2,
    Open,
    Update,
    GetVelocity2,
    GetAngularVelocity2,
    Unload,
    Unload2,
    GetPhysicalRadius,
    GetcenterOfMass,
    GetMass,
    GetSurfaceExtents,
    UnmakePhysical,
    RemakePhysical,
    BeamObject,
    InitCSimple,
    CachePhysicalProperties,
    GetName,
    IsTargetable,
    Connect,
    Disconnect,
    SetHitPoints,
    InitPhysics,
    InitCLoot,
    Start = 0x0639D81C,
    End = 0x0639D8BC
};

enum class CSolarVTable
{
    InitializeInstance,
    DoSimulationController,
    DestroyInstance,
    SetPosition,
    GetPosition,
    SetOrientation,
    GetTransform,
    SetTransform,
    GetTransform2,
    GetCenteredRadius,
    SetCenteredRadius,
    SetInstanceFlags,
    GetInstanceFlags,
    WriteFile,
    dunno, //.rdata:0639D3FC                 dd offset sub_629AC10
    GetVelocity,
    SetVelocity,
    GetAngularVelocity,
    SetVelocity2,
    Open,
    Update,
    GetVelocity2,
    GetAngularVelocity2,
    DisableControllers,
    EnableControllers,
    GetPhysicalRadius,
    GetCenterOfMass,
    GetMass,
    GetSurfaceExtents,
    UnmakePhysicals,
    RemakePhysicals,
    BeamObject,
    InitCSimple,
    CachePhysicalProps,
    GetName,
    IsTargetable,
    Connect,
    Disconnect,
    SetHitPoints,
    InitPhysics,
    InitCEquipObject,
    LoadAndEquipAndCargo,
    ClearEquipAndCargo,
    GetEquipDescriptionList,
    AddItem,
    Activate,
    GetActiveState,
    Disconnect2,
    Disconnect3,
    Connect2,
    Notify,
    FlushAnimations,
    GetTotalHitPoints,
    GetTotalMaxhitPoints,
    GetTotalRelativeHealth,
    GetSubObjectVelocity,
    GetSubObjectCenterOfMass,
    GetSubOjectHitPoints,
    GetSubOjectHitPoints2,
    GetSubObjectMaxHitPoints,
    GetSubObjectRelativeHealth,
    InstToSubObjectId,
    SubObjectIdToInst,
    DstroySubObjects,
    EnumerateSubObjects,
    AllocEquip,
    LinkShields,
    Start = 0x0639D3C4,
    End = 0x0639D4CC
};
