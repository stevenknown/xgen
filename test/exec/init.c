void * malloc(int);
int main()
{
	const int *buffer = (int*)malloc(sizeof(short)*(1<<8));
	int* p = (int*)buffer;
	int m = 0;
	for(int j = 0;j< (1<<23);j++)
	{
        int inner = 20;
	    const int *buffer = (int*)malloc(sizeof(short)*(1<<8));
		p = (int*)buffer;
		for(int i = 0;i< (1<<8);i = i + 2)
		{
		}
	}
	return 0;
}
