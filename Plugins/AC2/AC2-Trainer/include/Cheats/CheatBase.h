#pragma once
#include <string>

class CheatBase
{
public:
    virtual ~CheatBase() = default;
    virtual void DrawUI() = 0;
    virtual void Update() {}
    virtual std::string GetName() const = 0;
};
