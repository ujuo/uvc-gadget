/**
*/

#ifndef __CAPTURE_VIDEO__
#define __CAPTURE_VIDEO__

#include <object.hpp>
#include <private_vector.h>
#include <listener.hpp>

class TApplication : public TObject {
private:
	bool   Terminated;
	bool   Done;
protected:
	int    Error_code;
	Vector appListener;

	virtual bool waitFor(void **data) = 0;
	virtual void preRun(void *data) = 0;
	virtual void runner(void *data) = 0;
	virtual void postRun(void *data) = 0;
	virtual int  Init() = 0;
	virtual void Uninit() = 0;
	virtual void do_delay() = 0;
public:
	TApplication();
	~TApplication();
	virtual void addListener(Listener listener);
	virtual void removeListener(Listener listener);
	virtual void Terminate();
	virtual void Run();
};

#endif
