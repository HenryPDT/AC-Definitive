#pragma once

namespace Hooks
{
    void Initialize();
    void Shutdown();
    void Update(); 

    void SyncPointers();
    
    // Get the captured NotorietyManager pointer (notoriety value is at +0x0C)
    void* GetNotorietyPointer();
    
    // Get the captured DayTimeMgr pointer (time scale is at +0xA0)
    void* GetDayTimeMgrPointer();
    
    // Get the captured FreeRoam pointer (FOV is at +0x30)
    void* GetFreeRoamPointer();
    
    // Get the captured FreeCam pointer (yaw at +0x20/+0x30, pitch at +0x38)
    void* GetFreeCamPointer();
    
    // Get the captured MapManage pointer (map flags at +0x24)
    void* GetMapManagePointer();
    
    // Get the captured FreeCamera object (ECX) - holds coordinates at +0x30
    void* GetFreeCameraObjectPointer();
    
    // Camera position control for Camera Fly Mode
    float* GetCameraPosPointer();
    
    // Time scale control (CE fTimeDelay equivalent)
    // Default: 48.0 = normal speed, higher = slower, lower = faster
    void SetTimeScale(float scale);
    float GetTimeScale();
}
