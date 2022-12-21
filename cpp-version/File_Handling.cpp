//File handling is done through classes
//ifstream ->read data from file
//ofstream->write data into the file
//fstream->read and write operation
//open() - open the file
//read() ->read operation
//write() -> write into the file
//seekg()/seekp()-> place the pointer at a particular position in the file
//tellg/tellp() -> gives us the position of the pointer
//dirent -> ino:serial no of file
//-> d_name: name of directory
//stat() -> gives info about the file

#include<fstream>
#include<iostream>
using namespace std;


int main()
{
    fstream file;
    file.open("myfile.txt");

    file.write("this is an apple",16);

    int pos = file.tellp();

    file.seekp(pos-5);
    file.write("orange",6);
    file.close();

    fstream test("test2.txt");

    if(test.is_open())
    {
        char ch;
        while(true)
        {
            
        }
    }
}