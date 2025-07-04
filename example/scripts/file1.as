// this file is a part of module1

int MyInt = 234234;

Object obj;

array<int> intArray;
array<float> floatArray;
array<Object@> objectArray;

Vector3 MyVector;

string str = "some text";

bool aBoolean = false;

enum ObjectTypes
{
    Useless,
    Common,
    Rare
};

class SubObject
{
	string str;
	int int1;
	float float1;
	
	SubObject()
	{
		str = "subobject string this is";
		int1 = 1111;
		float1 = 35791.25651;
	}
};

class Base
{
	int count;
    
    string name;
    
    int level;

    ObjectTypes type; 

	Base()
	{
    type = Useless;
    level = 0;
    name = "I am base";
		count = 0;
	}
};

class Object : Base
{
	array<SubObject> subobjectArray;
	
	array<Vector3> vector3Array;

	Object()
	{
	
		SubObject obj;
		obj.str = "modified subobject string";
		
		type = Rare;
		
		subobjectArray.insertLast(obj);
		subobjectArray.insertLast(SubObject());;
		
		Vector3 v;
		v.x = 2;
		v.y = 4;
		v.z = 5;
		
		vector3Array.insertLast(v);
		
		v.x = 54;
		v.y = 6;
		v.z = 17;
		
		vector3Array.insertLast(v);
		
	}
	
};

void Once()
{
	intArray.insertLast(2);
	
	floatArray.insertLast(2.12);
	floatArray.insertLast(4.12);
	
	objectArray.insertLast(@Object());
	
	objectArray.insertLast(@Object());
	
	MyVector.x = 3242;
	MyVector.y = 232;
	MyVector.z = 564;
	
	@circular = Object3();
}

namespace NS
{
  int NSint = 24;
  float NSfloat = 4.25;
  
  ::Object NSfather;
  
  bool aBoolean = true;
};

void thread1()
{	
/**/
	intArray.resize(0);
	floatArray.resize(0);
	objectArray.resize(0);

	string s = "a local string";
	
    Object localObject;
	
	int a = 12414;
	
	float b = 23.88;

	MyInt++;
	
	LoopyFunction(MyInt);
	
	
}