#ifndef __ctk_listener_h
#define __ctk_listener_h

#include "object.hpp"

struct IListener : virtual  public TObject {
	virtual void eventFired(void *data) = 0;
};

DECLARE_CTK_INTERFACE(Listener, Object);

#endif
