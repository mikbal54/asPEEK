// this file is a part of module3

// handle to object. wouldnt matter for asPEEK
DeepObject @obj = DeepObject();

// global with same name also in module1
Vector3 MyVector;

class DeepObject
{
	float g;
	Depth1 child;
};

class Depth1
{
	Depth2 child;
};

class Depth2
{
	Depth3 child;
};

class Depth3
{
	Depth4 child;
};

class Depth4
{
	Depth5 child;
};

class Depth5
{
	Depth6 youngestChild;
};

class Depth6
{
	string s;
	array<int> intArray; 
	
	bool amIme;
	
	Depth6()
	{
		amIme = true;
		
		s = "i am a grandchild of Depth4";
		
		intArray.insertLast(1);
		intArray.insertLast(45);
		intArray.insertLast(7);
		intArray.insertLast(2);
	}
};

void thread3()
{

	MyVector.x = 0;
	MyVector.y = 0;
	MyVector.z = 0;

	if(obj.child.child.child.child.child.youngestChild.amIme)
	{
		int count = 0;
		
		for(int i = 0; i<10; ++i)
			count++;
	}
}