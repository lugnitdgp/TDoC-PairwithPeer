#include<iostream>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<dirent.h>
#include<fstream>
#include<sys/socket.h>
#include<sys/types.h>
#include<cstring>
#include<mutex>
#include <thread>
#include<string>
#include<cstdlib>
#include<fstream>
#include<dirent.h>
#include<unistd.h>

#define PORT 8080 ///the listening port no of server
#define PACKETSIZE 8 //packet size of the data being received

using namespace std;

//declare the Server address for the Server's socket
struct sockaddr_in Server_Address;

//Client's username
string ClientUsername;


//FUNCTIONS

string getip()
{
    FILE *f = popen("ip a | grep 'scope global' | grep -v ':' | awk '{print $2}' | cut -d '/' -f1", "r");
    char c;
    string s = "";
    while((c=getc(f))!=EOF)
    {
        s+=c;
    }
    pclose(f);
    return s;
}
//this function is called when server asks for the filename if it is present
void search_file(int Sock_fd)
{
    //receive filename
    char filename[100];
    int datasize;

    memset(filename,0,sizeof(filename));
    recv(Sock_fd,&datasize,sizeof(datasize),0);
    recv(Sock_fd,filename,datasize,0);

    //open the searching directory and search for file
    struct dirent *d;//contains two values dinfo and dname
    struct stat dst;
    DIR *dir;
    string path = "./shared/public"; 
    dir = opendir(path.c_str());
    bool res = false;

    if(dir!=NULL)
    {
        for(d = readdir(dir);d!=NULL;d=readdir(dir))
        {
            stat((path+(string)d->d_name).c_str(),&dst);
            if(S_ISDIR(dst.st_mode))
            {
                continue;
            }
            if(strcmp(d->d_name,filename))
            {
                res = true;
                break;
            }
        }
    }
    //Searching complete , send the response to the server
    send(Sock_fd,&res,sizeof(res),0);
    closedir(dir);
}
void send_file_list(int Sock_fd)
{
    int datasize;

    //open the searching directory and start searching
    struct dirent *d;
    struct stat dst;
    DIR *dir;

    string path  = "./shared/public";
    dir = opendir(path.c_str());

    bool res = false;
    if(dir==NULL)
    {
        send(Sock_fd,&res,sizeof(res),0);
        return;
    }
    for(d = readdir(dir);d!=NULL;d=readdir(dir))
    {
        stat((path+(string)d->d_name).c_str(),&dst);
        if(S_ISDIR(dst.st_mode))
        {
            continue;
        }
        res = true;
        send(Sock_fd,&res,sizeof(res),0);
        datasize = strlen(d->d_name);
        send(Sock_fd,&datasize,sizeof(datasize),0);
        send(Sock_fd,d->d_name,datasize,0);
    }
    res = false; //to terminate the loop to server side
    send(Sock_fd,&res,sizeof(res),0);

    //close the directory
    closedir(dir);
}
void sharepublic(int sock_fd)
{
    //receive the filename
    int datasize;
    char filename[100];

    memset(filename,0,sizeof(filename));
    recv(sock_fd,&datasize,sizeof(datasize),0);
    recv(sock_fd,filename,datasize,0);

    //cout<<filename<<endl;

    //open and send the file
    fstream fin;
    string pathtofile = "./shared/public"+(string)filename;
    fin.open(pathtofile,ios::in|ios::out);

    //calculate the size of the file
    fin.seekg(0,ios::beg);
    unsigned long long int begin = fin.tellg();

    fin.seekg(0,ios::end);
    unsigned long long int end = fin.tellg();
    unsigned long long int filesize = end-begin;
    
    //sending the filesize
    send(sock_fd,&filesize,sizeof(filesize),0);

    //start sending the file in packets , the packetsize is defined as
    //global variable

    unsigned long long int i, packetCnt = filesize/PACKETSIZE;

    char tmp[10] = {0};
    for(i = 0;i<packetCnt;i++)
    {
        fin.seekg((i*PACKETSIZE),ios::beg); //moving the pointer
        fin.read(tmp,PACKETSIZE);

        send(sock_fd,tmp,PACKETSIZE,0);
        memset(tmp,'\0',sizeof(tmp));
    }
    packetCnt = filesize%PACKETSIZE; //remaining filesize 
    fin.seekg((i*PACKETSIZE),ios::beg);
    fin.read(tmp,packetCnt);

    //close the file
    fin.close();
}
//sharepublic and download functions are running on different clients
void download(int sock_fd,string &filename)
{
    cout<<"Downloading the file...."<<endl;

    //receive the filesize 
    unsigned long long int filesize;
    recv(sock_fd,&filesize,sizeof(filesize),0);

    //prepare the output file
    fstream fout;
    fout.open(filename.c_str(),ios::in|ios::out|ios::trunc);


    //download file
    unsigned long long int packetCnt = filesize/PACKETSIZE,i;
    char tmp[10] = {0};

    for(i = 0;i<packetCnt;i++)
    {
        recv(sock_fd,tmp,PACKETSIZE,0);
        fout.write(tmp,PACKETSIZE); //no need to move the pointer it will automatically happen
        memset(tmp,'\n',sizeof(tmp));

    }
    packetCnt = filesize%PACKETSIZE;
    recv(sock_fd,tmp,packetCnt,0);
    fout.write(tmp,packetCnt);

    //close the file
    fout.close();

    cout<<"Download completed..."<<endl;
    cout<<"Filename : "<<filename<<" "<<filesize<<endl;

}
//function: if a successful match for file is found , then we will 
//make the connection with the particular client where the file was found,
//and make a connection request with that client and then we'll proceed with 
//downloading the file.
void try_download(string filename)
{
    //create socket
    int sock_fd;
    if(sock_fd=socket(AF_INET,SOCK_STREAM,0)<0)
    {
        cout<<"Error in socket creation"<<endl;
        exit(1);
    }
    //connection request
    if(connect(sock_fd,(struct sockaddr*)&Server_Address,sizeof(Server_Address))<0)
    {
        cout<<"Cannot connect to the server"<<endl;
        return;
    }
    //Send the requestid
    int req = 2;
    send(sock_fd,&req,sizeof(req),0);

    //send username
    int datasize = ClientUsername.length();
    send(sock_fd,&datasize,sizeof(datasize),0);
    send(sock_fd,ClientUsername.c_str(),datasize,0);

    //send filename
    datasize = filename.length();
    send(sock_fd,&datasize,sizeof(datasize),0);
    send(sock_fd,filename.c_str(),datasize,0);

    cout<<"Searching for the file..."<<endl;

    //receive response
    bool res;
    recv(sock_fd,&res,sizeof(res),0);
    if(res==false)
    {
        cout<<"File not found"<<endl;
        close(sock_fd);
        return;
    }
    else
    {
        cout<<"File found"<<endl;
    }

    //receive Sender's ip and port no
    char senderip[100],senderport;
    recv(sock_fd,&datasize,sizeof(datasize),0);
    memset(senderip,0,sizeof(senderip));
    recv(sock_fd,&senderip,datasize,0);
    recv(sock_fd,&senderport,sizeof(senderport),0);
    close(sock_fd);

    //connect to the sending/peer client
    struct sockaddr_in Senderaddr;
    memset(&Senderaddr,0,sizeof(Senderaddr));//clear the object

    Senderaddr.sin_family = AF_INET;
    Senderaddr.sin_addr.s_addr= inet_addr(senderip);
    Senderaddr.sin_port = htons(senderport);

    //we've closed that socket so we can use it again 
    if(sock_fd=socket(AF_INET,SOCK_STREAM,0)<0)
    {
        cout<<"Client req socket cannot be created"<<endl;
    }

    if(connect(sock_fd,(struct sockaddr*)&Senderaddr,sizeof(Senderaddr))<0)
    {
        cout<<"Cannot connect"<<endl;
        exit(1);
    }

    cout<<"Connected with :"<<senderip<<": "<<senderport<<endl;


    //downloading the file 
    //send the request to the client(reqid=1)
    req  = 1;
    send(sock_fd,&req,sizeof(req),0);

    //Send the filename to the sending client
    datasize =  filename.length();
    send(sock_fd,&datasize,sizeof(datasize),0);
    send(sock_fd,filename.c_str(),datasize,0);

    download(sock_fd,filename);

    //close socket
    close(sock_fd);

}   
void getfilelist()
{
    //connect to the server
    int sock_fd;
    if(sock_fd=socket(AF_INET,SOCK_STREAM,0)<0)
    {
        cout<<"Error in connecting to server";
        return;
    }
    if(connect(sock_fd,(struct sockaddr*)&Server_Address,sizeof(Server_Address)<0))
    {
        cout<<"Cannot connect to the server"<<endl;
        return;
    }

    //Send the requestID
    cout<<"Sending request..."<<endl;
    int req = 3;
    send(sock_fd,&req,sizeof(req),0);

    //Send the username
    int datasize = ClientUsername.length();
    send(sock_fd,&datasize,sizeof(datasize),0);
    send(sock_fd,ClientUsername.c_str(),datasize,0);

    cout<<"Fetching file list"<<endl;
    bool next_user = false ,next_file = false;

    recv(sock_fd,&next_user,sizeof(next_user),0);

    if(next_user==false)
    {
        cout<<"No file found"<<endl;
        close(sock_fd);
        return;
    }
    cout<<"Files available in the network"<<endl<<endl;

    char clientName[100],filename[100];
    while(next_user==true)
    {
        //receiving the username of client
        memset(clientName,0,sizeof(clientName));
        recv(sock_fd,&datasize,sizeof(datasize),0);
        recv(sock_fd,clientName,datasize,0);

        cout<<"Client Name: "<<clientName<<endl;
        for(recv(sock_fd,&next_file,sizeof(next_file),0);next_file==true;recv(sock_fd,&next_file,sizeof(next_file),0))
        {
            
            recv(sock_fd,&datasize,sizeof(datasize),0);
            recv(sock_fd,filename,sizeof(filename),0);

            filename[datasize] ='\0';
            cout<<" "<<filename<<endl;
        }
        cout<<"\n";
        send(sock_fd,&next_user,sizeof(next_user),0);
        
    }
    //close socket
    close(sock_fd);
}
void user_interface()
{
    while(true)
    {
        cout<<"What do you need to perform"<<endl;
        cout<<"1> Download file\n";
        cout<<"2> List Files \n";
        cout<<"3> Refresh Screen \n";

        int option;
        cin>>option;
        if(option==1)
        {
            string filename;
            cout<<"Enter the filename to download: ";
            cin>>filename;
            try_download(filename);
        }
        else if(option==2)
        {
            getfilelist();
        }
        else if(option==3)
        {
            continue;
        }
        else
        {
            cout<<"Invalid choice"<<endl;
        }
    }
}
void handle_request(int tmp_sockfd,int requestid)
{
    switch(requestid)
    {
        case 0:
            search_file(tmp_sockfd);// request made from server side
            break;
        case 1://this will be a request from the client side itself
            sharepublic (tmp_sockfd);
            break;
        case 2:
            // shareprivate(tmp_sockfd) sending file privately 
            break;
        case 3:
            send_file_list(tmp_sockfd);
            break;
        default:
            cout<<"Invalid request monseiur"<<endl;
            break;
    }

}
void RunClient()
{
    int Client_Listen_Sockfd, Client_Request_Sockfd;
    struct sockaddr_in Client_Address;

    memset(&Client_Address,0,sizeof(Client_Address));
    memset(&Server_Address,0,sizeof(Server_Address));


    //creating listening socket

    if((Client_Listen_Sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        cout<<"Listening Socket could not be created"<<endl;
        exit(1);
    }

    //filling up the ClientAddress
    string myip;
    int myport;
    cout<<"Enter your ipaddress"<<endl;
    cin>>myip;
    cout<<"\nEnter your port number"<<endl;
    cin>>myport;


    Client_Address.sin_family = AF_INET;
    Client_Address.sin_addr.s_addr = inet_addr(myip.c_str());
    Client_Address.sin_port = htons(myport);

    //binding the client address with the socket
    if(bind(Client_Listen_Sockfd,(struct sockaddr*)&Client_Address,sizeof(Client_Address))<0)
    {
        cout<<"Binding error"<<endl;
        exit(1);
    }

    cout<<"Listening at ip address "<<myip<<" and port number "<<myport<<endl;

    //client request socket
    if((Client_Request_Sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        cout<<"Request Socket could not be created"<<endl;
        exit(1);
    }
    string serverip;
    cout<<"Enter server ip"<<endl;
    cin>>serverip;


    Server_Address.sin_family=AF_INET;
    Server_Address.sin_addr.s_addr=inet_addr(serverip.c_str());
    Server_Address.sin_port=htons(PORT);

    if(connect(Client_Request_Sockfd,(struct sockaddr*)&Server_Address,sizeof(Server_Address))<0)
    {
        cout<<"Cannot connect to the server "<<endl;
        exit(1);
    }

    cout<<"Connected Successfully....\n";

    //send register request
    int requestid = 1;
    send(Client_Request_Sockfd,&requestid,sizeof(requestid),0);

    cout<<"Enter username for registration"<<endl;
    cin>>ClientUsername;

    int datasize = ClientUsername.length();

    send(Client_Request_Sockfd,&datasize,sizeof(datasize),0);
    send(Client_Request_Sockfd,ClientUsername.c_str(),datasize,0);


    datasize = myip.length();
    send(Client_Request_Sockfd,&datasize,sizeof(datasize),0);
    send(Client_Request_Sockfd,myip.c_str(),datasize,0);


    send(Client_Request_Sockfd,&myport,sizeof(myport),0);

    bool res;
    recv(Client_Request_Sockfd,&res,sizeof(res),0);
    close(Client_Request_Sockfd);

    if(res==false)
    {
        cout<<"Registration unsuccessful , try another username"<<endl;
        close(Client_Listen_Sockfd);
        exit(1);
    }
    else
    {
        cout<<"Registration successful"<<endl;
    }
    
    //start listening from the server side
    if(listen(Client_Listen_Sockfd,6)<0)
    {
        cout<<"Error in listening"<<endl;
        exit(1);
    }
    cout<<"Listening...."<<endl;


    //start user interface
    thread thr_ui(user_interface);
    thr_ui.detach();

    //start accepting requests
    int tmp_sockfd;
    struct sockaddr_in tmpaddr;
    socklen_t addrsize;
    while(true)
    {
        if((tmp_sockfd=accept(Client_Listen_Sockfd,(struct sockaddr*)&tmpaddr,&addrsize))<0)
        {
            cout<<"Connection request failed"<<endl;
            exit(1);
        }
        //recieve the requestid
        recv(tmp_sockfd,&requestid,sizeof(requestid),0);

        //handle request
        thread thr_hr(handle_request,tmp_sockfd,requestid);
        thr_hr.detach();
    }
}