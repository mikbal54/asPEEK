// this file is a part of module2

class Object
{
	int x;
	int y;
	float z;
	
	Child child;
	
	Object()
	{
		x = 0;
		y = 1;
		z = 12000;
	}
};

class Child
{
	float v;
	Child()
	{
		v = 99.99f;
	}
};

void thread2()
{
	if(obj.child.v == 99.99)
	{
		obj.x++;
		obj.y--;
		obj.z *= 0.9999;
	}
}

