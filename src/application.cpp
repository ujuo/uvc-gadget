/**

*/
#include "application.hpp"
#include "listener.hpp"
#include "private_vector.hpp"

/* This basic application does not handle any error messages. */
TApplication::TApplication() : TObject()
{
	Terminated = false;
	Done = false;
	Error_code = 0;
	appListener = new TVector();
}

/* Application may or maynot use thread for Run function */
TApplication::~TApplication()
{
	if (!Terminated) {
		Terminate();
		while (!Done) ; /* wait for termination */
	}
}

void TApplication::addListener(Listener listener)
{
	appListener->addElement(listener);
}

void TApplication::removeListener(Listener listener)
{
	if (!appListener)	return;
	appListener->removeElement(listener);
}

void TApplication::Terminate()
{
	Terminated = true;
}

void TApplication::Run()
{
	void *data;

	Error_code = Init();
	if (Error_code != 0)
		return;
	while (!Terminated) {
		if (waitFor(&data)) {
			preRun(data);
			runner(data);
			for(int i = appListener->size()-1; i >= 0; i--) {
				Listener listener = CASTTO(Listener, appListener->elementAt(i));
				listener->eventFired(data);
			};
			postRun(data);
		}
		do_delay();
	}
	Uninit();
	Done = true;
}
