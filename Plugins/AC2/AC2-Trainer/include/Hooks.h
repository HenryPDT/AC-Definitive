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
    
    // Time scale control (CE fTimeDelay equivalent)
    // Default: 48.0 = normal speed, higher = slower, lower = faster
    void SetTimeScale(float scale);
    float GetTimeScale();
}
