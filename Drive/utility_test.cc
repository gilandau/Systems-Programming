/*
 * utility_test.cc
 *
 *  Created on: Nov 14, 2018
 *      Author: cis505
 */

#include "utility.h"
#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <unistd.h>
#include <fstream>
#include <vector>


int main(int argc, char *argv[])
{

	map < char, vector < string >  > nodes = parse_config("sample_config");
	map < char, vector < string >  > ::iterator it;

	for ( it = nodes.begin(); it != nodes.end(); it++ )
	{
	    cout << "\n TYPE: " <<it->first << "\n";  // string (key)
	    cout << "Address: ";
	    for(int j = 0; j < it->second.size(); j++){
		    cout << it->second[j] << "\n";

	    }
	}
}


