#pragma once

#include "EventHandler.hpp"
class ITask
{
  private:
    bool _used = false;

  public:
    inline bool used() const
    {
        return _used;
    }
    inline void setUsed()
    {
        _used = true;
    }
};

class ITaskProvider
{
  private:
  public:
    EventHandler<ITask> onTaskReceived{};
};





