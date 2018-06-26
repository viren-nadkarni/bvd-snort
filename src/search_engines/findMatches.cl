__kernel void findMatches(__global int* stateTable, __global uchar* translate, __global uchar* input, __global int* length, __global int* resultList)
{
	unsigned int i = get_global_id(0);
	int state = 0;
	int counter = 0;
	int sindex;

	int kernels = 768;
	int window = ((*length)/kernels);
	int startPos = i*window;
	int limit = startPos + window + 10;

	for (int j=startPos; j<limit && j<*length; j++){
		sindex = translate[input[j]];
		if (stateTable[(state*258)+1] == 1)
		{
			counter += 1;
			atomic_add(&resultList[state], 1); 
		}
		state = stateTable[(state*258) + 2u + sindex];
	}
	atomic_add(&resultList[0], counter); 
}
