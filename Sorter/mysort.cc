#include <string>
#include <string.h>

#include <map>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <queue>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <iostream>
using namespace std;


bool S_flag = false;
bool L_flag = false;

void check_pipefd(FILE* file, int ln)
{
	if (file == NULL) {
		cerr << "File Error" << ln << '\n';
		exit(2);
	}
}


// read file
void read_file(vector<long long>&nums, char* file_name)
{
	fstream numFile(file_name);
	if (!numFile.is_open())
	{
		cerr << "File failed to open \n";
		exit(2);
	}
	else
	{
		long long num;
		while(numFile >> num)
		{
			nums.push_back(num);
		}
	}


}
void L_read_file(vector<string>&ls, char* file_name)
{
	ifstream LFile(file_name);
	if (!LFile.is_open())
	{
		cerr << "LFile failed to open \n";
		exit(2);
	}
	else
	{
		string s;
		while(getline(LFile, s))
		{
			ls.push_back(s);
		}
	}


}

//partitiion
vector< vector<long long> > partition(vector<long long> &nums, int num_subs)
{
	vector< vector<long long> >  parts;
	int part_size = nums.size()/num_subs;
	int remainder = nums.size() % num_subs;

	for(int i = 0; i < num_subs; i++)
	{
		vector<long long> part;
		for (int j = 0; j < part_size; j++)
		{
			part.push_back(nums[(i*part_size) + j]);
		}
		parts.push_back(part);
		vector<long long>().swap(part);
	}
	for (int r = 0; r < remainder; r++)
	{
		parts[r].push_back(nums[(num_subs*part_size)+r]);
	}

	return parts;
}
vector< vector<string> > L_partition(vector<string> &ls, int num_subs)
{
	vector< vector<string> >  parts;
	int part_size = ls.size()/num_subs;
	int remainder = ls.size() % num_subs;

	for(int i = 0; i < num_subs; i++)
	{
		vector<string> part;
		for (int j = 0; j < part_size; j++)
		{
			part.push_back(ls[(i*part_size) + j]);
		}
		parts.push_back(part);
		vector<string>().swap(part);
	}
	for (int r = 0; r < remainder; r++)
	{
		parts[r].push_back(ls[(num_subs*part_size)+r]);
	}

	return parts;
}
////Generic bubble sort
void bubblesort(vector<long long> &nums)
{
	int n = nums.size();
	for(int i=0; i < n-1; i++)
	{
		for(int j=0; j < n-i-1; j++)
		{
			if (nums[j] > nums[j+1])
			{
				long long temp = nums[j];
				nums[j] = nums[j+1];
				nums[j+1] = temp;
			}
		}
	}

}

void L_bubblesort(vector<string> &ls)
{
	int n = ls.size();
	for(int i=0; i < n-1; i++)
	{
		for(int j=0; j < n-i-1; j++)
		{
			if (strcmp(ls[j].c_str(), ls[j+1].c_str()) > 0)
			{
				string temp = ls[j];
				ls[j] = ls[j+1];
				ls[j+1] = temp;
			}
		}
	}

}
// sort using processes, err status 3 (process issue)
void sort_process(vector<vector<long long> > &parts )
{

	int num_parts = parts.size();

//	first make n pipes
	int p_w_pipes[num_parts][2];
	int c_w_pipes[num_parts][2];

	for (int i =0; i < num_parts; i++)
	{
		pipe(p_w_pipes[i]);
		pipe(c_w_pipes[i]);
		pid_t pid = fork();

		if (pid < 0)
		{
			cerr << "Fork Failed!\n";
			exit(3);
		}

		if (pid == 0)
		{
			int c_pipe = i;
//			close down uneeded pipes
			close(c_w_pipes[c_pipe][0]);
			close(p_w_pipes[c_pipe][1]);
			//			do child
			FILE* child_r_file = fdopen(p_w_pipes[c_pipe][0], "r");
//			first number is size of the partition
			vector<long long> child_nums;
			char n_s[100];
			fgets(n_s, 100, child_r_file);
			int n = atoi(n_s);
			for (int  j = 0; j < n; j++)
			{
				char num_s[100];
				fgets(num_s, 100, child_r_file);
				long long num = atoll(num_s);
				child_nums.push_back(num);

			}
			fclose(child_r_file);
			close(p_w_pipes[c_pipe][0]);

			bubblesort(child_nums);

			FILE* child_w_file = fdopen(c_w_pipes[c_pipe][1], "w");
			for (int j = 0; j < n; j++)
			{
				fprintf(child_w_file,  "%lld\n", child_nums[j]);
			}

			vector<long long>().swap(child_nums);
			fclose(child_w_file);
			close(c_w_pipes[c_pipe][1]);

			exit(0);
		}
//		Parent sets up for children
		else
		{
			int p_pipe = i;
//			Close down uneeded pipes
			close(c_w_pipes[p_pipe][1]);
			close(p_w_pipes[p_pipe][0]);
			FILE* parent_w_file = fdopen(p_w_pipes[p_pipe][1], "w");

			int s = parts[p_pipe].size();
			fprintf(parent_w_file, "%d\n", s);
			for(int j = 0; j < parts[p_pipe].size(); j++)
			{
				fprintf(parent_w_file, "%lld\n", parts[p_pipe][j]);
			}
			fclose(parent_w_file);
			close(p_w_pipes[p_pipe][1]);

		}
	}

//		Parent starts reading and waits for the kids
	for (int i = 0; i < num_parts; i++)
	{
		FILE* parent_r_file = fdopen(c_w_pipes[i][0], "r");
		for (int j = 0; j < parts[i].size(); j++)
		{
			char num_s[100];
			fgets(num_s, 100, parent_r_file);
			long long num = atoll(num_s);
			parts[i][j]  = num;
		}


		fclose(parent_r_file);
		close(c_w_pipes[i][0]);

	}
//		wait
	for(int i =0; i < num_parts; i++)
	{
		wait(NULL);
	}


}

void L_sort_process(vector<vector<string> > &parts )
{

	int num_parts = parts.size();

//	first make n pipes
	int p_w_pipes[num_parts][2];
	int c_w_pipes[num_parts][2];

	for (int i =0; i < num_parts; i++)
	{
		pipe(p_w_pipes[i]);
		pipe(c_w_pipes[i]);
		pid_t pid = fork();

		if (pid < 0)
		{
			cerr << "Fork Failed!\n";
			exit(3);
		}

		if (pid == 0)
		{
			int c_pipe = i;
//			close down uneeded pipes
			close(c_w_pipes[c_pipe][0]);
			close(p_w_pipes[c_pipe][1]);
			//			do child
			FILE* child_r_file = fdopen(p_w_pipes[c_pipe][0], "r");
//			first number is size of the partition
			vector<string> child_nums;
			char n_s[100];
			fgets(n_s, 100, child_r_file);
			int n = atoi(n_s);
			for (int  j = 0; j < n; j++)
			{
				char l_s[100];
				fgets(l_s, 100, child_r_file);
				int n_l = atoi(l_s);
				char c_s[n_l];
				fgets(c_s, n_l+2,child_r_file);
				string s = string(c_s).substr(0,n_l);
				child_nums.push_back(s);

			}
			fclose(child_r_file);
			close(p_w_pipes[c_pipe][0]);

			L_bubblesort(child_nums);
			FILE* child_w_file = fdopen(c_w_pipes[c_pipe][1], "w");
			for (int j = 0; j < n; j++)
			{
				int line_size = child_nums[j].length();
				fprintf(child_w_file, "%d\n", line_size);
				fprintf(child_w_file,  "%s\n", child_nums[j].c_str());
			}

			vector<string>().swap(child_nums);
			fclose(child_w_file);
			close(c_w_pipes[c_pipe][1]);

			exit(0);
		}
//		Parent sets up for children
		else
		{
			int p_pipe = i;
//			Close down uneeded pipes
			close(c_w_pipes[p_pipe][1]);
			close(p_w_pipes[p_pipe][0]);
			FILE* parent_w_file = fdopen(p_w_pipes[p_pipe][1], "w");

			int s = parts[p_pipe].size();
			fprintf(parent_w_file, "%d\n", s);
			for(int j = 0; j < parts[p_pipe].size(); j++)
			{
				int line_size = parts[p_pipe][j].length();
				fprintf(parent_w_file, "%d\n",line_size);
				fprintf(parent_w_file, "%s\n", parts[p_pipe][j].c_str());
			}
			fclose(parent_w_file);
			close(p_w_pipes[p_pipe][1]);

		}
	}

//		Parent starts reading and waits for the kids
	for (int i = 0; i < num_parts; i++)
	{
		FILE* parent_r_file = fdopen(c_w_pipes[i][0], "r");
		for (int j = 0; j < parts[i].size(); j++)
		{
			char l_s[100];
			fgets(l_s, 100, parent_r_file);
			int n_l = atoi(l_s);
			char c_s[n_l+5];
			fgets(c_s, n_l+5,parent_r_file);
			string s = string(c_s).substr(0,n_l);
			parts[i][j] = s;
		}


		fclose(parent_r_file);
		close(c_w_pipes[i][0]);

	}
//		wait
	for(int i =0; i < num_parts; i++)
	{
		wait(NULL);
	}


}



struct thread_arg
{

	int c_w_p;
	int p_w_p;

};

void *thread_sort_f(void *arg)
{
//
//	struct thread_arg *t_arg = (thread_arg*)arg;
	vector<int>* t_arg = (vector<int>*) arg;
	int c_w_pipe = (*t_arg)[0];
	int p_w_pipe = (*t_arg)[1];

			//			do child
	FILE* child_r_file = fdopen(p_w_pipe, "r");
	check_pipefd(child_r_file, 210);

//			first number is size of the partition
	vector<long long> child_nums;
	char n_s[100];
	fgets(n_s, 100, child_r_file);
	int n = atoi(n_s);

	for (int  j = 0; j < n; j++)
	{
		char num_s[100];
		fgets(num_s, 100, child_r_file);
		long long num = atoll(num_s);
		child_nums.push_back(num);

	}
	fclose(child_r_file);
//	close(p_w_pipe[0]);

	bubblesort(child_nums);

	FILE* child_w_file = fdopen(c_w_pipe, "w");
	check_pipefd(child_w_file, 231);

	for (int j = 0; j < n; j++)
	{
		fprintf(child_w_file,  "%lld\n", child_nums[j]);
	}

	vector<long long>().swap(child_nums);
	fclose(child_w_file);
//	close(c_w_pipe[1]);
	pthread_exit(0);
}

void *L_thread_sort_f(void *arg)
{
//
//	struct thread_arg *t_arg = (thread_arg*)arg;
	vector<int>* t_arg = (vector<int>*) arg;
	int c_w_pipe = (*t_arg)[0];
	int p_w_pipe = (*t_arg)[1];

			//			do child
	FILE* child_r_file = fdopen(p_w_pipe, "r");
	check_pipefd(child_r_file, 210);

//			first number is size of the partition
	vector<string> child_nums;
	char n_s[100];
	fgets(n_s, 100, child_r_file);
	int n = atoi(n_s);

	for (int  j = 0; j < n; j++)
	{

		char l_s[100];
		fgets(l_s, 100, child_r_file);
		int n_l = atoi(l_s);
		char c_s[n_l];
		fgets(c_s, n_l+2,child_r_file);
		string s = string(c_s).substr(0,n_l);
		child_nums.push_back(s);

	}
	fclose(child_r_file);
//	close(p_w_pipe[0]);

	L_bubblesort(child_nums);

	FILE* child_w_file = fdopen(c_w_pipe, "w");
	check_pipefd(child_w_file, 231);

	for (int j = 0; j < n; j++)
	{
		int line_size = child_nums[j].length();
		fprintf(child_w_file, "%d\n", line_size);
		fprintf(child_w_file,  "%s\n", child_nums[j].c_str());
	}
	vector<string>().swap(child_nums);
	fclose(child_w_file);
//	close(c_w_pipe[1]);
	pthread_exit(0);
}


// sort using threads, err status 4 (thread issue)
void sort_thread(vector<vector<long long> > &parts )
{
	int num_parts = parts.size();
	//	first make n pipes
	int p_w_pipes[num_parts][2];
	int c_w_pipes[num_parts][2];
	pthread_t tid[num_parts];
	int result;

	for (int i =0; i < num_parts; i++)
	{
		pipe(p_w_pipes[i]);
		pipe(c_w_pipes[i]);
		////			close down uneeded pipes
//		close(c_w_pipes[i][0]);
//		close(p_w_pipes[i][1]);
		vector<int>* vect_args = new vector<int>();

		vect_args->push_back(c_w_pipes[i][1]);
		vect_args->push_back(p_w_pipes[i][0]);

		result = pthread_create(&tid[i], NULL, thread_sort_f, (void *)vect_args);

//			Parent sets up for children
//				Close down uneeded pipes

		FILE* parent_w_file = fdopen(p_w_pipes[i][1], "w");
		check_pipefd(parent_w_file, 274);

		int s = parts[i].size();

		fprintf(parent_w_file, "%d\n", s);
		for(int j = 0; j < parts[i].size(); j++)
		{
			fprintf(parent_w_file, "%lld\n", parts[i][j]);
		}
		fclose(parent_w_file);

//		close(p_w_pipes[i][1]);

	}

	//		Parent starts reading and waits for the kids
	for (int i = 0; i < num_parts; i++)
	{
		FILE* parent_r_file = fdopen(c_w_pipes[i][0], "r");
		check_pipefd(parent_r_file, 291);
		for (int j = 0; j < parts[i].size(); j++)
		{
			char num_s[100];
			fgets(num_s, 100, parent_r_file);
			long long num = atoll(num_s);
			parts[i][j]  = num;
		}

		fclose(parent_r_file);
//		close(c_w_pipes[i][0]);
	}
//	//		wait
	for(int i =0; i < num_parts; i++)
	{
		pthread_join(tid[i],NULL);
	}
}

void L_sort_thread(vector<vector<string> > &parts )
{


	int num_parts = parts.size();

	//	first make n pipes
	int p_w_pipes[num_parts][2];
	int c_w_pipes[num_parts][2];
	pthread_t tid[num_parts];
	int result;

	for (int i =0; i < num_parts; i++)
	{
		pipe(p_w_pipes[i]);
		pipe(c_w_pipes[i]);
		////			close down uneeded pipes
//		close(c_w_pipes[i][0]);
//		close(p_w_pipes[i][1]);
		vector<int>* vect_args = new vector<int>();

		vect_args->push_back(c_w_pipes[i][1]);
		vect_args->push_back(p_w_pipes[i][0]);

		result = pthread_create(&tid[i], NULL, L_thread_sort_f, (void *)vect_args);


//			Parent sets up for children
//				Close down uneeded pipes

		FILE* parent_w_file = fdopen(p_w_pipes[i][1], "w");
		check_pipefd(parent_w_file, 274);

		int s = parts[i].size();
		fprintf(parent_w_file, "%d\n", s);
		for(int j = 0; j < parts[i].size(); j++)
		{
			int line_size = parts[i][j].length();
			fprintf(parent_w_file, "%d\n",line_size );
			fprintf(parent_w_file, "%s\n", parts[i][j].c_str());
		}
		fclose(parent_w_file);

//		close(p_w_pipes[i][1]);

	}

	//		Parent starts reading and waits for the kids
	for (int i = 0; i < num_parts; i++)
	{
		FILE* parent_r_file = fdopen(c_w_pipes[i][0], "r");
		check_pipefd(parent_r_file, 291);
		for (int j = 0; j < parts[i].size(); j++)
		{

			char l_s[100];
			fgets(l_s, 100, parent_r_file);
			int n_l = atoi(l_s);
			char c_s[n_l];
			fgets(c_s, n_l+2,parent_r_file);
			string s = string(c_s).substr(0,n_l);
			parts[i][j] = s;
		}

		fclose(parent_r_file);
//		close(c_w_pipes[i][0]);
	}
	cout<< tid[0] << "\n";

}
struct PQ_Node{
	long long data;
	int index;

	PQ_Node(long long d, int i)
	{
		data = d;
		index = i;
	}
};

struct L_PQ_Node{
	string data;
	int index;

	L_PQ_Node(string d, int i)
	{
		data = d;
		index = i;
	}
};


struct compare {
	bool operator() (const PQ_Node* a, const PQ_Node* b)
	{
		return a->data > b->data;
	}
};


struct L_compare {
	bool operator() (const L_PQ_Node* a, const L_PQ_Node* b)
	{
		return a->data > b->data;
	}
};
void merge_parts( vector<vector<long long> > &parts,  vector<long long> &nums)
{
	priority_queue<PQ_Node*, vector<PQ_Node*>, compare> pq;
	int num_parts = parts.size();
	int pointers[num_parts] = {0};
	int cur_index = 0;


	for (int i = 0; i < parts.size(); i++)
	{
		PQ_Node* pqnode = new PQ_Node(parts[i][0],i);
		pq.push(pqnode);
		pointers[i] = pointers[i] + 1;
	}

	while (!pq.empty()) {
		PQ_Node* top = pq.top();
		nums[cur_index] = top->data;
		cur_index = cur_index +1;
		int part_num = top->index;
		pq.pop();
		int part_size = parts[part_num].size();
		if (pointers[part_num] < part_size)
		{
			PQ_Node* new_node = new PQ_Node(parts[part_num][pointers[part_num]],part_num);
			pointers[part_num] = pointers[part_num] +1;
			pq.push(new_node);
		}
	}
}


void L_merge_parts( vector<vector<string> > &parts,  vector<string> &ls)
{
	priority_queue<L_PQ_Node*, vector<L_PQ_Node*>, L_compare> pq;
	int num_parts = parts.size();
	int pointers[num_parts] = {0};
	int cur_index = 0;


	for (int i = 0; i < parts.size(); i++)
	{
		L_PQ_Node* pqnode = new L_PQ_Node(parts[i][0],i);
		pq.push(pqnode);
		pointers[i] = pointers[i] + 1;
	}

	while (!pq.empty()) {
		L_PQ_Node* top = pq.top();
		ls[cur_index] = top->data;
		cur_index = cur_index +1;
		int part_num = top->index;
		pq.pop();
		int part_size = parts[part_num].size();
		if (pointers[part_num] < part_size)
		{
			L_PQ_Node* new_node = new L_PQ_Node(parts[part_num][pointers[part_num]],part_num);
			pointers[part_num] = pointers[part_num] +1;
			pq.push(new_node);
		}
	}
}

int main(int argc, char *argv[])
{
//	Default variables
	int c;
	int num_subs = 4;
	bool t_flag = false;
	vector<long long > nums;
	vector <string> ls;

//	if not enough arguments, if err code is 1, argument error
	if (argc < 2)
	{
		cerr << "*** Author: Gil Landau (glandau) \n";
		exit(1);
	}
//	Parse args and read in files
	else {
		while ((c = getopt(argc, argv, "n:tL")) != -1) {
			switch (c) {
			case 'n':

				num_subs = atoi(optarg);
				if (num_subs < 1) {
					cerr << "Invalid num count \n";
					exit(1);
				}
				break;

			case 't':
				t_flag = true;
				break;

			case 'L':
				L_flag = true;
				break;

			case 'S':
				S_flag = true;
				break;

			default:
				cerr << "Arguments error";
				exit(1);
			}
		}
	}

//		read files (err code 2) store in nums and partition the data

	if (!L_flag) {
		for (int i = optind; i < argc; i++) {
			read_file(nums, argv[i]);
		}
		vector<vector<long long> > parts = partition(nums, num_subs);

		//		If 1 use main
		if (num_subs == 1) {
			bubblesort(nums);

		}
		//		if threads_on, use threads
		else if (t_flag) {
			sort_thread(parts);
			merge_parts(parts, nums);

		}
		//		otherwise fork the police
		else {
			sort_process(parts);
			merge_parts(parts, nums);
		}
		// CLEAN UP and OUTPUT
		for (int i = 0; i < nums.size(); i++) {
			cout << nums[i] << "\n";
		}

		vector<long long>().swap(nums);

		for (int i = 0; i < parts.size(); i++) {
			vector<long long>().swap(parts[i]);
		}

		vector<vector<long long> >().swap(parts);
	}
	else {

		for (int i = optind; i < argc; i++) {
			L_read_file(ls, argv[i]);
		}
		vector<vector<string> > parts = L_partition(ls, num_subs);

		//		If 1 use main
		if (num_subs == 1) {
			L_bubblesort(ls);

		}
		//		if threads_on, use threads
		else if (t_flag) {

			L_sort_thread(parts);
			L_merge_parts(parts, ls);

		}
		//		otherwise fork the police
		else {
			L_sort_process(parts);
			L_merge_parts(parts, ls);
		}
		// CLEAN UP and OUTPUT
		for (int i = 0; i < ls.size(); i++) {
			cout << ls[i] << "\n";
		}

		vector<string>().swap(ls);

		for (int i = 0; i < parts.size(); i++) {
			vector<string>().swap(parts[i]);
		}

		vector<vector<string> >().swap(parts);

	}

	exit(0);
}
