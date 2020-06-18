/**

*/
#include "private_vector.hpp"

TVector::TVector(int initialCapacity, int capacityIncrement) : TObject()
{
	elementData = new Object[initialCapacity];
	length = initialCapacity;
	this->capacityIncrement = capacityIncrement;
	elementCount = 0;
}

TVector::~TVector()
{
	if(elementData) delete[] elementData;
}

void 
TVector::trimToSize() 
{
	if (elementCount < length) {
		Object* oldData = elementData;
		elementData = new Object[elementCount];
		for(int i = 0; i < elementCount; i++) {
			elementData[i] = oldData[i];
		}
		delete[] oldData;
		length = elementCount;
	}
}

void 
TVector::ensureCapacity(int minCapacity) 
{
	if (minCapacity > length) {
		ensureCapacityHelper(minCapacity);
	}
}

void 
TVector::ensureCapacityHelper(int minCapacity) 
{
	int oldCapacity = length;
	Object* oldData = elementData;
	int newCapacity = (capacityIncrement > 0) ?
	    (oldCapacity + capacityIncrement) : (oldCapacity * 2);
	if (newCapacity < minCapacity) {
	    newCapacity = minCapacity;
	}
	elementData = new Object[newCapacity];
	for(int i = 0; i < elementCount; i++) {
		elementData[i] = oldData[i];
	}
	length = newCapacity;
	delete[] oldData;
}
    
void 
TVector::setSize(int newSize) 
{
	if ((newSize > elementCount) && (newSize > length)) {
		ensureCapacityHelper(newSize);
	} else {
		for (int i = newSize ; i < elementCount ; i++) {
			elementData[i] = 0;
		}
	}
	elementCount = newSize;
}

int 
TVector::indexOf(Object elem, int index) 
{
	for (int i = index ; i < elementCount ; i++) {
		if (elem == elementData[i]) {
			return i;
		}
	}
	return -1;
}

int 
TVector::lastIndexOf(Object elem, int index) 
{
	for (int i = index ; i >= 0 ; i--) {
		if (elem == elementData[i]) {
			return i;
		}
	}
	return -1;
}

void 
TVector::removeElementAt(int index) 
{
	int j = elementCount - index - 1;
	if (j > 0) {
		for(int i = index; i < index+j; i++) {
			elementData[i] = elementData[i+1];
		}
	}
	elementCount--;
	elementData[elementCount] = 0; /* to let gc do its work */
}

void 
TVector::insertElementAt(Object obj, int index) 
{
	int newcount = elementCount + 1;
	if (newcount > length) {
	    ensureCapacityHelper(newcount);
	}
	for(int i = elementCount-1; i >= index; i--) {
		elementData[i+1] = elementData[i];
	}
	elementData[index] = obj;
	elementCount++;
}

void 
TVector::addElement(Object obj) 
{
	int newcount = elementCount + 1;
	if (newcount > length) {
	    ensureCapacityHelper(newcount);
	}
	elementData[elementCount++] = obj;
}

bool
TVector::removeElement(Object obj) 
{
	int i = indexOf(obj);
	if (i >= 0) {
	    removeElementAt(i);
	    return true;
	}
	return false;
}

void 
TVector::removeAllElements() 
{
	for (int i = 0; i < elementCount; i++) {
	    elementData[i] = 0;
	}
	elementCount = 0;
}
