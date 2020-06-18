/**
 * Copyright(c) 2019 I4VINE Inc.,
 * All right reserved by Seungwoo Kim <ksw@i4vine.com>
 *  @file  object.h
 *  @brief generalized root of classes
 *  @author Seungwoo Kim <ksw@i4vine.com>
 *
 *
*/

#ifndef __ctk_object_h
#define __ctk_object_h

typedef long long ctklong;

class TObject {
	friend class Handle;
private:
protected:
	unsigned int f_rc;
	void _incref();
	void _decref();
protected:
#if defined(NDEBUG)
	TObject() : f_rc(0) {}
	TObject(const TObject& obj) : f_rc(0) {}
#else
	TObject() : f_rc(0), __debug(0) {}
	TObject(const TObject& obj) : f_rc(0), __debug(0) {}
#endif
	TObject& operator=(const TObject& obj) { return *this; }
public:
	virtual ~TObject() {}
#if !defined(NDEBUG)
	int __debug;
#endif
};

class Handle {
protected:
	TObject* f_object;
	void _incref() const { if (f_object) f_object->_incref(); }
	void _decref() { if (f_object) f_object->_decref(); }
	Handle(TObject* obj = 0) : f_object(obj) { _incref(); }
	Handle(const Handle& obj) : f_object(obj.f_object) { _incref(); }
	~Handle() { _decref(); }
	void _assign(TObject* obj) { if(obj) obj->_incref(); _decref(); f_object = obj; }
	void _assign(const Handle& handle) { handle._incref(); _decref(); f_object = handle.f_object; }
};

class Object : public Handle {
protected:
	typedef TObject ImpType;
public:
	Object(TObject* obj = 0) : Handle(obj) {}
	Object(const Object& obj) : Handle(obj) {}
	Object& operator=(TObject* obj) { _assign(obj); return *this; }
	Object& operator=(const Object& obj) { _assign(obj); return *this; }
	~Object() {}
	const TObject* operator->() const { return f_object; }
	TObject* operator->() { return f_object; }
	TObject& operator*() { return *f_object; }
	const TObject& operator*() const { return *f_object; }
	bool operator==(const Object& obj) const { return f_object == obj.f_object; }
	bool operator==(const TObject* obj) const { return f_object == obj; }
	bool operator!=(const Object& obj) const { return f_object != obj.f_object; }
	bool operator!=(const TObject* obj) const { return f_object != obj; }
	bool operator!() const { return !f_object; }
};

#if 0
template<class T, class Super>
class ObjectHandle : public Super {
private:
	typedef typename Super::ImpType SuperImpType;
protected:
	typedef T ImpType;
public:
	ObjectHandle(T* obj = 0) : Super(obj) {};
	ObjectHandle<T,Super>& operator=(T* obj) { return (ObjectHandle<T,Super>&)Super::operator=(obj); };
	ObjectHandle(const ObjectHandle<T,Super>& obj) : Super(obj) {};
	ObjectHandle<T,Super>& operator=(const ObjectHandle<T,Super>& obj) { return (ObjectHandle<T,Super>&)Super::operator=(obj); };
	const T* operator->() const { return dynamic_cast<const T*>(f_object); };
	T* operator->() { return dynamic_cast<T*>(f_object); };
	T& operator*() { return dynamic_cast<T&>(*f_object); };
	const T& operator*() const { return dynamic_cast<const T&>(f_object); };
};
#else
template<class T, class Object>
class ObjectHandle : public Object {
private:
	typedef typename Object::ImpType SuperImpType;
protected:
	typedef T ImpType;
public:
	ObjectHandle(T* obj = 0) : Object(obj) {};
	ObjectHandle<T,Object>& operator=(T* obj) { return (ObjectHandle<T,Object>&)Object::operator=(obj); };
	ObjectHandle(const ObjectHandle<T,Object>& obj) : Object(obj) {};
	ObjectHandle<T,Object>& operator=(const ObjectHandle<T,Object>& obj) { return (ObjectHandle<T,Object>&)Object::operator=(obj); };
	const T* operator->() const { return dynamic_cast<const T*>(Object::f_object); };
	T* operator->() { return dynamic_cast<T*>(Object::f_object); };
	T& operator*() { return dynamic_cast<T&>(*Object::f_object); };
	const T& operator*() const { return dynamic_cast<const T&>(Object::f_object); };
};
#endif

#define CASTTO(Class,object)\
	(&dynamic_cast<T##Class&>(*(object)))
	
#define CONSTCASTTO(Class,object)\
	(&dynamic_cast<const T##Class&>(*(object)))
	
#define DECLARE_CTK_CLASS(Class, Super) \
	class T##Class;\
	typedef ObjectHandle<T##Class, Super> Class;
	
#define DECLARE_CTK_INTERFACE(Interface, Super) \
	class I##Interface;\
	typedef I##Interface T##Interface;\
	typedef ObjectHandle<I##Interface, Super> Interface;
	
#define EXPORT_CTK_INTERFACE(Class, Interface) \
	Interface as##Interface() { return (I##Interface*)this; }

#undef assert
#if defined(NDEBUG)
#define ctkassert(test) ((void)0)
#define assertif(cond,test) ((void)0)
#define assertcond(value) ((void)0)
#define assertifobject(obj,cond,test) ((void)0)
#define assertcondobject(obj,value) ((void)0)
#else
#define ctkassert(test) ((test)?(void)0:(void)__ctk_assert(#test,__FILE__,__LINE__))
#define assertif(cond,test) (((__ctk_cond!=(cond))||(test))?(void)0:(void)__ctk_assert(#cond " => " #test,__FILE__,__LINE__))
#define setassertcond(value) (__ctk_cond=(value))
#define assertifobject(obj,cond,test) ((((obj)->__debug!=(cond))||(test))?(void)0:(void)__ctk_assert(#obj "::" #cond " => " #test,__FILE__,__LINE__))
#define setassertcondobject(obj,value) ((obj)->__debug=(value))
extern int __ctk_cond;
void	__ctk_assert(const char *,const char *,int);
#endif

#endif
