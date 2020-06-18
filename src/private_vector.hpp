#ifndef __ctk_private_vector
#define __ctk_private_vector

#include <object.hpp>
#include <private_vector.h>

class TVector : virtual public TObject {
private:
    virtual void ensureCapacityHelper(int);
protected:    
    Object* elementData;
    int elementCount;
    int length;
    int capacityIncrement;
public:    
    TVector(int = 10,int = 0);
    ~TVector();
    void trimToSize();
    void ensureCapacity(int);
    void setSize(int);
    int capacity() { return length; }
    int size() const { return elementCount; }
    bool isEmpty() { return elementCount == 0; }
    //Enumeration elements();
    bool contains(Object elem) { return indexOf(elem, 0) >= 0; }
    int indexOf(Object, int = 0);
    int lastIndexOf(Object elem) { return lastIndexOf(elem, elementCount-1); }
    int lastIndexOf(Object, int);
    Object elementAt(int index) const { return elementData[index]; }
    Object firstElement() { return elementData[0]; }
    Object lastElement() { return elementData[elementCount-1]; }
    void setElementAt(Object obj, int index) { elementData[index] = obj; }
    void removeElementAt(int);
    void insertElementAt(Object, int);
    void addElement(Object);
    bool removeElement(Object);
    void removeAllElements();
    //virtual Object clone();
    //String toString();
};

#endif
