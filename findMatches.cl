__kernel void findMatches(__global int* stateTable, __global uchar* translate, __global uchar* input, __global int* length, __global int* resultList, __global int * match_count, __global int * matchLen)
{
	unsigned int i = get_global_id(0);

	int state = 0;
	int counter = 0;
	int sindex;

	match_count[i]=0;

	int kernels = 768;
	int window = ((*length)/kernels);
	int startPos = i*window;
	int limit = startPos + window + 50;

	for (int j=startPos; j<limit && j<*length; j++){
		sindex = translate[input[j]];
		if (stateTable[(state*258)+1] == 1)
		{
			if ((j>=matchLen[state])&& (j-matchLen[state])<=(startPos+window))
			counter += 1;
		}
		state = stateTable[(state*258) + 2u + sindex];
	}
	match_count[i] = counter;
}
