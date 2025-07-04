// this file is a part of module1

float float1 = 345.5;

// create a circular reference.
Object3 @circular;

class ChildObject
{
	Object3 @obj;	
};

class Object3
{
	int v;
	float y;
	bool z;
	ChildObject @c;
	
	Object3()
	{
		v = 85;
		y = 43.3333;
		z = true;
		@c = ChildObject();
		@c.obj = @this;
	}
	
	void DoStuff()
	{
		v = 4;
		Object3 anotherCircular;
	}
};

