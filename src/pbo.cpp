#include <fstream>
#include <iostream>
#include <string>


void readPBO(std::string filename)
{
	std::ifstream ifs(filename, std::ios::binary);
	std::cout << ifs.rdbuf() << std::flush;	
	//std::cout << filename << std::endl;
}

int main(int argc, char** args)
{
	readPBO(std::string("test.pbo"));
    return 0;
}